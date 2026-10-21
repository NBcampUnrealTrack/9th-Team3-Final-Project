#include "Combat/knightMechAttackTiming.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/knightAnimNotifyState_MeleeTrace.h"
#include "Animation/knightAnimNotify_MeleeArea.h"
#include "Camera/CameraShakeBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

namespace
{
	TAutoConsoleVariable<int32> CVarMechTiming(
		TEXT("knight.Player.MechTiming"), 1,
		TEXT("1: mechanical attack timing (lock -> burst -> stop) on player attacks. 0: original animation timing."),
		ECVF_Cheat);
}

TSharedPtr<FknightMechAttackTiming> FknightMechAttackTiming::Start(UAnimInstance* AnimInstance, UAnimMontage* InMontage, float InBaseRate,
	const FknightMechTimingSettings& InSettings, bool bTwoHanded)
{
	if (!InSettings.bEnabled || CVarMechTiming.GetValueOnGameThread() == 0 || !AnimInstance || !InMontage || InBaseRate <= 0.f
		|| !AnimInstance->Montage_IsPlaying(InMontage))
	{
		return nullptr;
	}

	// 첫 근접 판정 구간. 트레이스가 없고 범위 판정(한 번)만 있으면 그 순간을 짧은 구간으로 본다.
	float FirstStart = MAX_FLT;
	float FirstEnd = MAX_FLT;
	for (const FAnimNotifyEvent& Event : InMontage->Notifies)
	{
		const bool bTrace = Event.NotifyStateClass && Event.NotifyStateClass->IsA<UknightAnimNotifyState_MeleeTrace>();
		const bool bArea = Event.Notify && Event.Notify->IsA<UknightAnimNotify_MeleeArea>();
		if ((bTrace || bArea) && Event.GetTriggerTime() < FirstStart)
		{
			FirstStart = Event.GetTriggerTime();
			FirstEnd = bTrace ? Event.GetEndTriggerTime() : FirstStart + 0.05f;
		}
	}
	if (FirstStart == MAX_FLT)
	{
		return nullptr;
	}

	TSharedPtr<FknightMechAttackTiming> Timing = MakeShared<FknightMechAttackTiming>();
	Timing->Anim = AnimInstance;
	Timing->Montage = InMontage;
	Timing->Mesh = AnimInstance->GetSkelMeshComponent();
	Timing->Settings = InSettings;
	Timing->BaseRate = InBaseRate;
	Timing->TraceStart = FirstStart;
	Timing->TraceEnd = FMath::Max(FirstEnd, FirstStart + 0.02f);
	Timing->LockStart = FMath::Max(0.f, FirstStart - InSettings.LockLeadTime);
	Timing->LockTime = InSettings.LockTime * (bTwoHanded ? InSettings.TwoHandedLockScale : 1.f);
	Timing->BurstRate = InSettings.BurstRate * (bTwoHanded ? InSettings.TwoHandedBurstScale : 1.f);
	Timing->bTwoHandedStance = bTwoHanded;
	Timing->Phase = EPhase::Windup;
	Timing->SetRate(InSettings.WindupRate);
	TWeakPtr<FknightMechAttackTiming> Weak = Timing;
	Timing->Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float DeltaTime)
	{
		const TSharedPtr<FknightMechAttackTiming> Pinned = Weak.Pin();
		return Pinned.IsValid() && Pinned->Tick(DeltaTime);
	}));
	return Timing;
}

FknightMechAttackTiming::~FknightMechAttackTiming()
{
	StopEffects(BurstFx);
	StopEffects(StopFx);
	if (Ticker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
		Ticker.Reset();
	}
}

void FknightMechAttackTiming::Stop()
{
	StopEffects(BurstFx);
	StopEffects(StopFx);
	if (Phase != EPhase::Done)
	{
		SetRate(1.f);
		Phase = EPhase::Done;
	}
	if (Ticker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
		Ticker.Reset();
	}
}

bool FknightMechAttackTiming::IsStillPlaying() const
{
	const UAnimInstance* const AnimInstance = Anim.Get();
	const UAnimMontage* const PlayingMontage = Montage.Get();
	return AnimInstance && PlayingMontage && AnimInstance->Montage_IsPlaying(PlayingMontage);
}

void FknightMechAttackTiming::SetRate(float Factor) const
{
	if (IsStillPlaying())
	{
		Anim->Montage_SetPlayRate(Montage.Get(), BaseRate * Factor);
	}
}

bool FknightMechAttackTiming::Tick(float DeltaRealTime)
{
	if (Phase == EPhase::Done || !IsStillPlaying())
	{
		// 다른 공격으로 바뀌었거나 끊겼다. 그 몽타주는 이미 끝났으니 손대지 않는다.
		StopEffects(BurstFx);
		StopEffects(StopFx);
		Phase = EPhase::Done;
		Ticker.Reset();
		return false;
	}
	const USkeletalMeshComponent* const MeshComp = Mesh.Get();
	const AActor* const Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	const UWorld* const World = Owner ? Owner->GetWorld() : nullptr;
	if (!World || World->IsPaused())
	{
		return true;
	}
	// 락·스톱 길이는 캐릭터 시간 기준(히트스톱·그로기 슬로 중이면 같이 길어진다).
	const float Delta = DeltaRealTime * World->GetWorldSettings()->GetEffectiveTimeDilation() * FMath::Max(Owner->CustomTimeDilation, 0.f);
	const float Position = Anim->Montage_GetPosition(Montage.Get());

	switch (Phase)
	{
	case EPhase::Windup:
		if (Position >= TraceStart)
		{
			EnterPhase(EPhase::Burst);
		}
		else if (Position >= LockStart)
		{
			EnterPhase(LockTime > 0.f ? EPhase::Lock : EPhase::Burst);
		}
		break;
	case EPhase::Lock:
		PhaseElapsed += Delta;
		if (PhaseElapsed >= LockTime)
		{
			EnterPhase(EPhase::Burst);
		}
		break;
	case EPhase::Burst:
		if (Position >= TraceEnd)
		{
			EnterPhase(Settings.StopTime > 0.f ? EPhase::StopHold : EPhase::Done);
		}
		break;
	case EPhase::StopHold:
		PhaseElapsed += Delta;
		if (PhaseElapsed >= Settings.StopTime)
		{
			EnterPhase(EPhase::Done);
		}
		break;
	default:
		break;
	}
	if (Phase == EPhase::Done)
	{
		Ticker.Reset();
		return false;
	}
	return true;
}

void FknightMechAttackTiming::EnterPhase(EPhase NewPhase)
{
	Phase = NewPhase;
	PhaseElapsed = 0.f;
	switch (NewPhase)
	{
	case EPhase::Lock:
		SetRate(Settings.LockRate);
		PlaySound(Settings.LockSound);
		break;
	case EPhase::Burst:
	{
		SetRate(BurstRate);
		SpawnEffects(Settings.BurstEffect, bTwoHandedStance && !Settings.TwoHandedBurstSockets.IsEmpty()
			? Settings.TwoHandedBurstSockets : Settings.BurstSockets, Settings.BurstEffectScale, BurstFx);
		PlaySound(Settings.BurstSound);
		const USkeletalMeshComponent* const MeshComp = Mesh.Get();
		const APawn* const Pawn = MeshComp ? Cast<APawn>(MeshComp->GetOwner()) : nullptr;
		APlayerController* const PC = Pawn && Pawn->IsLocallyControlled() ? Pawn->GetController<APlayerController>() : nullptr;
		if (PC && Settings.BurstShake && Settings.BurstShakeScale > 0.f)
		{
			PC->ClientStartCameraShake(Settings.BurstShake, Settings.BurstShakeScale);
		}
		break;
	}
	case EPhase::StopHold:
		SetRate(Settings.StopRate);
		// 휘두르기가 끝났으니 분사를 끈다.
		StopEffects(BurstFx);
		SpawnEffects(Settings.StopEffect, Settings.StopSockets, Settings.StopEffectScale, StopFx);
		PlaySound(Settings.StopSound);
		break;
	case EPhase::Done:
		SetRate(1.f);
		StopEffects(BurstFx);
		StopEffects(StopFx);
		break;
	default:
		break;
	}
}

void FknightMechAttackTiming::StopEffects(TArray<TWeakObjectPtr<UNiagaraComponent>>& Spawned) const
{
	for (const TWeakObjectPtr<UNiagaraComponent>& Fx : Spawned)
	{
		if (UNiagaraComponent* const Component = Fx.Get())
		{
			Component->Deactivate();
		}
	}
	Spawned.Reset();
}

void FknightMechAttackTiming::SpawnEffects(UNiagaraSystem* Effect, const TArray<FName>& Sockets, float Scale,
	TArray<TWeakObjectPtr<UNiagaraComponent>>& OutSpawned) const
{
	USkeletalMeshComponent* const MeshComp = Mesh.Get();
	if (!Effect || !MeshComp || Scale <= 0.f)
	{
		return;
	}
	for (const FName Socket : Sockets)
	{
		if (Socket.IsNone() || !MeshComp->DoesSocketExist(Socket))
		{
			continue;
		}
		if (UNiagaraComponent* const Fx = UNiagaraFunctionLibrary::SpawnSystemAttached(Effect, MeshComp, Socket, FVector::ZeroVector, FRotator::ZeroRotator,
			FVector(Scale), EAttachLocation::SnapToTarget, true, ENCPoolMethod::None))
		{
			OutSpawned.Add(Fx);
		}
	}
}

void FknightMechAttackTiming::PlaySound(USoundBase* Sound) const
{
	const USkeletalMeshComponent* const MeshComp = Mesh.Get();
	if (Sound && MeshComp)
	{
		UGameplayStatics::PlaySoundAtLocation(MeshComp, Sound, MeshComp->GetComponentLocation());
	}
}
