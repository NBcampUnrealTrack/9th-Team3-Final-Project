#include "Combat/knightGroggyFeedbackComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/knightAnimNotifyState_MeleeTrace.h"
#include "Animation/knightAnimNotify_MeleeArea.h"
#include "Combat/knightHitReactionComponent.h"
#include "Combat/knightHitStopComponent.h"
#include "Combat/knightTargetLockComponent.h"
#include "Camera/knightPlayerCameraManager.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "AbilitySystem/Executions/knightDamageExecution.h"
#include "Containers/Ticker.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GenericTeamAgentInterface.h"
#include "GameplayEffect.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/CombatInterface.h"
#include "Interfaces/TargetableInterface.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Raid/knightRaidGameState.h"
#include "knightCharacter.h"
#include "System/knightGameplayTags.h"
#include "TimerManager.h"
#include "knight.h"

namespace
{
	bool IsInHitStop(const AActor* Actor)
	{
		const UknightHitStopComponent* const HitStop = Actor ? Actor->FindComponentByClass<UknightHitStopComponent>() : nullptr;
		return HitStop && HitStop->IsInHitStop();
	}

	float EaseInOut(float Alpha)
	{
		const float T = FMath::Clamp(Alpha, 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	void RestoreAppliedSpeed(AActor* Actor, float AppliedSpeed, float BaseSpeed, bool bApplied)
	{
		if (!Actor || !bApplied)
		{
			return;
		}
		if (UknightHitStopComponent* const HitStop = Actor->FindComponentByClass<UknightHitStopComponent>(); HitStop && HitStop->IsInHitStop())
		{
			HitStop->ReplaceRestoreTimeDilation(AppliedSpeed, BaseSpeed);
		}
		else if (FMath::IsNearlyEqual(Actor->CustomTimeDilation, AppliedSpeed))
		{
			Actor->CustomTimeDilation = BaseSpeed;
		}
	}

	float FindClosestMonsterBodyDistance2D(const AknightMonsterCharacterBase* Boss, const FVector& PlayerLoc, const FVector& TargetPoint, FVector* OutClosestPoint = nullptr)
	{
		if (OutClosestPoint)
		{
			*OutClosestPoint = TargetPoint;
		}
		float BestDist2D = FVector::Dist2D(PlayerLoc, TargetPoint);
		if (const USkeletalMeshComponent* const BossMesh = Boss ? Boss->GetMesh() : nullptr)
		{
			const FVector ProbePoint(PlayerLoc.X, PlayerLoc.Y, PlayerLoc.Z + 35.f);
			for (const FBodyInstance* const BodyInst : BossMesh->Bodies)
			{
				if (!BodyInst || !BodyInst->IsValidBodyInstance())
				{
					continue;
				}
				FVector BodyClosest = FVector::ZeroVector;
				if (BodyInst->GetDistanceToBody(ProbePoint, BodyClosest) >= 0.f)
				{
					const float Dist2D = FVector::Dist2D(PlayerLoc, BodyClosest);
					if (Dist2D < BestDist2D)
					{
						BestDist2D = Dist2D;
						if (OutClosestPoint)
						{
							*OutClosestPoint = BodyClosest;
						}
					}
				}
			}
		}
		return BestDist2D;
	}
}

UknightGroggyFeedbackComponent::UknightGroggyFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	GroggyCueTag = knightGameplayTags::GameplayCue_Hit_Groggy;
	ReleaseCueTag = knightGameplayTags::GameplayCue_Hit_Groggy_Release;
}

void UknightGroggyFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();
	PoiseBrokenHandle = UknightHitReactionComponent::OnAnyPoiseBroken.AddUObject(this, &ThisClass::HandleAnyPoiseBroken);
}

void UknightGroggyFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UknightHitReactionComponent::OnAnyPoiseBroken.Remove(PoiseBrokenHandle);
	PoiseBrokenHandle.Reset();
	CancelKnockdownPresentation();
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReleaseDelayTimer);
	}
	FinishRelease();
	if (FeedbackTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(FeedbackTicker);
		FeedbackTicker.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void UknightGroggyFeedbackComponent::WatchAttack(UGameplayAbility* Attack, UAnimMontage* Montage, const FknightMeleeHitParams& HitParams)
{
	if (!Attack || !Montage || !Attack->IsActive() || Montage->bLoop)
	{
		return;
	}
	if (WatchedAttack.IsValid() && WatchedAttack.Get() != Attack)
	{
		EndPredictionForAttack(WatchedAttack.Get());
	}
	else if (bPredictionActive && PredictionAttack.Get() == Attack)
	{
		BeginPredictionReturn();
	}
	WatchedAttack = Attack;
	WatchedMontage = Montage;
	WatchedHitParams = HitParams;
	PredictedTargetsThisAttack.Reset();
	bWatchedPredictionAttempted = TryStartPredictedKnockdown(Attack, HitParams);
	EnsureFeedbackTicker();
}

bool UknightGroggyFeedbackComponent::TryStartPredictedKnockdown(UGameplayAbility* Attack, const FknightMeleeHitParams& HitParams)
{
	AknightCharacter* const Player = Cast<AknightCharacter>(GetOwner());
	APlayerController* const PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
	UWorld* const World = GetWorld();
	if (!World || !Attack || !Attack->IsActive() || !Player || Player->IsDead() || !HitParams.DamageEffect
		|| bCameraBeatActive || bReleasing || !PC || !PC->IsLocalController()
		|| PC->GetViewTarget() != Player || World->IsPaused() || PredictionMaxTime <= 0.f
		|| IsInHitStop(Player))
	{
		return false;
	}
	if (const AknightRaidGameState* const Raid = World->GetGameState<AknightRaidGameState>())
	{
		const EknightRaidPhase Phase = Raid->GetRaidPhase();
		if (Phase != EknightRaidPhase::Explore && Phase != EknightRaidPhase::Combat)
		{
			return false;
		}
	}
	// Only effects using the same execution formula can be predicted from current attributes.
	const UGameplayEffect* const Effect = HitParams.DamageEffect->GetDefaultObject<UGameplayEffect>();
	bool bKnownPoiseFormula = false;
	for (const FGameplayEffectExecutionDefinition& Execution : Effect->Executions)
	{
		bKnownPoiseFormula |= Execution.CalculationClass && Execution.CalculationClass->IsChildOf(UknightDamageExecution::StaticClass());
	}
	if (!bKnownPoiseFormula || HitParams.PoiseDamage <= 0.f)
	{
		return false;
	}

	UknightTargetLockComponent* const Lock = Player->FindComponentByClass<UknightTargetLockComponent>();
	if (!Lock)
	{
		return false;
	}
	const bool bLocked = Lock->IsLockedOn();
	FVector TargetPoint = FVector::ZeroVector;
	AActor* Candidate = nullptr;
	FGameplayTag PartTag;
	if (bLocked)
	{
		Candidate = Lock->GetLockedTarget();
		PartTag = Lock->GetLockedPartTag();
		if (!Lock->GetLockedTargetLocation(TargetPoint))
		{
			return false;
		}
	}
	else if (!Lock->FindSoftTarget(Player->GetActorForwardVector(), PredictionRange + 150.f, PredictionAngle + 15.f,
		TargetPoint, Candidate, PartTag))
	{
		return false;
	}
	AknightMonsterCharacterBase* const Boss = Cast<AknightMonsterCharacterBase>(Candidate);
	UAbilitySystemComponent* const SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player);
	UAbilitySystemComponent* const TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Boss);
	const UknightCoreAttributeSet* const SourceCore = SourceASC ? SourceASC->GetSet<UknightCoreAttributeSet>() : nullptr;
	const UknightCoreAttributeSet* const TargetCore = TargetASC ? TargetASC->GetSet<UknightCoreAttributeSet>() : nullptr;
	if (!Boss || PredictedTargetsThisAttack.Contains(Boss) || Boss->IsDead() || IsInHitStop(Boss)
		|| !SourceCore || !TargetCore || TargetCore->GetPoise() <= 0.f
		|| FGenericTeamId::GetAttitude(Player, Boss) != ETeamAttitude::Hostile
		|| !ICombatInterface::Execute_CanReceiveHitBP(Boss, Player)
		|| SourceASC->HasMatchingGameplayTag(knightGameplayTags::State_Dead)
		|| SourceASC->HasMatchingGameplayTag(knightGameplayTags::State_HitReact)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Dead)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Invulnerable)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Guarding)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Parrying)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_HitReact_Knockdown)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::Window_MeleeTrace))
	{
		return false;
	}

	const FVector PlayerLoc = Player->GetActorLocation();
	const FVector PlayerForward2D = Player->GetActorForwardVector().GetSafeNormal2D();
	const FVector ViewForward2D = PC->GetControlRotation().Vector().GetSafeNormal2D();

	FVector ClosestBodyPoint = TargetPoint;
	const float PointDistance2D = FVector::Dist2D(PlayerLoc, TargetPoint);
	const float SurfaceDistance2D = FindClosestMonsterBodyDistance2D(Boss, PlayerLoc, TargetPoint, &ClosestBodyPoint);
	const bool bIsLungingOrHeavyAttack = HitParams.PoiseDamage >= 20.f;
	const float MaxSurfaceRange = bIsLungingOrHeavyAttack ? 140.f : 70.f;
	const float MaxPointRange = bIsLungingOrHeavyAttack ? 440.f : 310.f;
	if (SurfaceDistance2D > MaxSurfaceRange || PointDistance2D > MaxPointRange)
	{
		return false;
	}

	FVector ToTarget2D = ClosestBodyPoint - PlayerLoc;
	ToTarget2D.Z = 0.f;
	const float TargetDist2D = ToTarget2D.Size();
	const FVector DirToTarget = TargetDist2D > KINDA_SMALL_NUMBER ? (ToTarget2D / TargetDist2D) : PlayerForward2D;
	const float CosAngle = FMath::Max(
		FVector::DotProduct(DirToTarget, PlayerForward2D),
		bLocked ? FVector::DotProduct(DirToTarget, ViewForward2D) : -1.f);
	const float AllowedAngle = bLocked ? FMath::Max(PredictionAngle, 65.f) : PredictionAngle;
	if (SurfaceDistance2D > 45.f && CosAngle < FMath::Cos(FMath::DegreesToRadians(AllowedAngle)))
	{
		return false;
	}

	knightDamageMath::FInputs In;
	In.BasePoiseDamage = HitParams.PoiseDamage;
	In.StaggerPower = SourceASC->GetNumericAttribute(UknightCoreAttributeSet::GetStaggerPowerAttribute());
	In.PoiseDamageTakenMultiplier = TargetASC->GetNumericAttribute(UknightCoreAttributeSet::GetPoiseDamageTakenMultiplierAttribute());
	const float ExpectedPoiseDamage = knightDamageMath::Compute(In).PoiseDamage;
	if (ExpectedPoiseDamage + KINDA_SMALL_NUMBER < TargetCore->GetPoise())
	{
		return false;
	}

	PredictedBoss = Boss;
	PredictedTargetsThisAttack.Add(Boss);
	PredictionAttack = Attack;
	bPredictionWasLocked = bLocked;
	bPredictionActive = true;
	bPredictionHitPending = false;
	bCameraBeatActive = true;
	bOwnerSpeedApplied = false;
	bVictimSpeedApplied = false;
	OwnerBaseSpeed = Player->CustomTimeDilation;
	VictimBaseSpeed = Boss->CustomTimeDilation;
	PredictionStartTime = FPlatformTime::Seconds();
	Player->OnDeathStarted.AddUniqueDynamic(this, &ThisClass::HandlePlayerDied);
	Boss->OnMonsterDefeated.AddUniqueDynamic(this, &ThisClass::HandleBossDefeated);
	ZoomCameraManager = Cast<AknightPlayerCameraManager>(PC->PlayerCameraManager);
	if (AknightPlayerCameraManager* const CameraManager = ZoomCameraManager.Get())
	{
		CameraManager->StartKnockdownZoom(Player, PredictionZoomDegrees, FMath::Min(PredictionZoomEnterTime, 0.22f), PredictionStartTime);
	}
	EnsureFeedbackTicker();
	UE_LOG(LogknightCombat, Log, TEXT("Predicted knockdown %s -> %s: poise %.1f, expected %.1f (surfaceDist %.0f, pointDist %.0f)"),
		*GetNameSafe(Player), *GetNameSafe(Boss), TargetCore->GetPoise(), ExpectedPoiseDamage, SurfaceDistance2D, PointDistance2D);
	return true;
}

void UknightGroggyFeedbackComponent::EndPredictionForAttack(const UGameplayAbility* Attack)
{
	if (WatchedAttack.Get() == Attack)
	{
		WatchedAttack.Reset();
		WatchedMontage.Reset();
		PredictedTargetsThisAttack.Reset();
	}
	if (bPredictionActive && PredictionAttack.Get() == Attack)
	{
		BeginPredictionReturn();
	}
}

void UknightGroggyFeedbackComponent::ResolvePredictedMeleeContact(AActor* HitTarget, bool bAccepted, const UGameplayAbility* SourceAbility)
{
	if (!bPredictionActive)
	{
		return;
	}
	const bool bMatchingTarget = HitTarget && HitTarget == PredictedBoss.Get() && SourceAbility == PredictionAttack.Get();
	if (bMatchingTarget && bAccepted)
	{
		// End pre-hit local slow before the damage effect / hit-stop snapshots CustomTimeDilation,
		// but keep the camera zoom held through FreezeTime so the impact freezes at peak close-up.
		FinishPredictionSlow();
		bPredictionActive = false;
		PredictionAttack.Reset();
		bPredictionHitPending = true;
		PresentationEndTime = FPlatformTime::Seconds() + FMath::Max(FreezeTime, 0.f) + FMath::Max(PredictionZoomReturnTime, 0.f) + 0.5;
		return;
	}
	BeginPredictionReturn();
}

void UknightGroggyFeedbackComponent::CompletePredictedMeleeHit(AActor* HitTarget, bool bNewKnockdown)
{
	if (bPredictionHitPending && HitTarget == PredictedBoss.Get())
	{
		UE_LOG(LogknightCombat, Verbose, TEXT("Predicted hit on %s: new knockdown %s"),
			*GetNameSafe(HitTarget), bNewKnockdown ? TEXT("yes") : TEXT("no"));
		bPredictionHitPending = false;
		const UWorld* const World = GetWorld();
		const bool bFreezeWaiting = World && World->GetTimerManager().IsTimerActive(ReleaseDelayTimer);
		if (!bNewKnockdown && !bFreezeWaiting)
		{
			if (AknightPlayerCameraManager* const CameraManager = ZoomCameraManager.Get())
			{
				CameraManager->ReturnKnockdownZoom(PredictionZoomReturnTime);
			}
			PresentationEndTime = FPlatformTime::Seconds() + FMath::Max(PredictionZoomReturnTime, 0.f);
		}
	}
}

void UknightGroggyFeedbackComponent::FinishPredictionSlow()
{
	RestoreAppliedSpeed(GetOwner(), OwnerAppliedSpeed, OwnerBaseSpeed, bOwnerSpeedApplied);
	RestoreAppliedSpeed(PredictedBoss.Get(), VictimAppliedSpeed, VictimBaseSpeed, bVictimSpeedApplied);
	bOwnerSpeedApplied = false;
	bVictimSpeedApplied = false;
}

void UknightGroggyFeedbackComponent::BeginPredictionReturn()
{
	if (!bPredictionActive)
	{
		return;
	}
	FinishPredictionSlow();
	bPredictionActive = false;
	PredictionAttack.Reset();
	if (AknightPlayerCameraManager* const CameraManager = ZoomCameraManager.Get())
	{
		CameraManager->ReturnKnockdownZoom(PredictionZoomReturnTime);
	}
	PresentationEndTime = FPlatformTime::Seconds() + FMath::Max(PredictionZoomReturnTime, 0.f);
}

void UknightGroggyFeedbackComponent::FinishPredictionVisual()
{
	if (!bCameraBeatActive)
	{
		return;
	}
	FinishPredictionSlow();
	if (AknightCharacter* const Player = Cast<AknightCharacter>(GetOwner()))
	{
		Player->OnDeathStarted.RemoveDynamic(this, &ThisClass::HandlePlayerDied);
	}
	if (AknightMonsterCharacterBase* const Boss = PredictedBoss.Get())
	{
		Boss->OnMonsterDefeated.RemoveDynamic(this, &ThisClass::HandleBossDefeated);
	}
	if (AknightPlayerCameraManager* const CameraManager = ZoomCameraManager.Get())
	{
		CameraManager->CancelKnockdownZoom();
	}
	PredictedBoss.Reset();
	PredictionAttack.Reset();
	ZoomCameraManager.Reset();
	bPredictionActive = false;
	bPredictionHitPending = false;
	bCameraBeatActive = false;
}

void UknightGroggyFeedbackComponent::CancelKnockdownPresentation()
{
	WatchedAttack.Reset();
	WatchedMontage.Reset();
	PredictedTargetsThisAttack.Reset();
	FinishPredictionVisual();
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReleaseDelayTimer);
	}
	FinishRelease();
	bSkipGenericReleaseSlow = false;
}

void UknightGroggyFeedbackComponent::HandlePlayerDied(AknightCharacter* DeadCharacter)
{
	CancelKnockdownPresentation();
}

void UknightGroggyFeedbackComponent::HandleBossDefeated(AknightMonsterCharacterBase* Boss, AActor* Killer)
{
	CancelKnockdownPresentation();
}

void UknightGroggyFeedbackComponent::HandleAnyPoiseBroken(AActor* InVictim, AActor* Instigator, const FGameplayEffectContextHandle& Context)
{
	AActor* const Owner = GetOwner();
	UWorld* const World = GetWorld();
	if (!Owner || !World || !InVictim || Instigator != Owner || InVictim == Owner)
	{
		return;
	}
	const bool bPredictedContact = bPredictionHitPending && PredictedBoss.Get() == InVictim;
	if (!bPredictedContact)
	{
		CancelKnockdownPresentation();
	}
	// When the groggy attack already played slow-motion from the start of the attack and released at impact,
	// skip the post-hit slow-motion so the enemy collapses into groggy cleanly right after impact.
	bSkipGenericReleaseSlow = bPredictedContact;
	const float EffectiveFreezeTime = bPredictedContact ? FMath::Min(FreezeTime, 0.08f) : FreezeTime;
	// 앞 그로기의 슬로모션이 아직 진행 중이면 정리하고 새로 시작한다.
	World->GetTimerManager().ClearTimer(ReleaseDelayTimer);
	FinishRelease();
	Victim = InVictim;

	// 1. 멈춤: 히트스탑 컴포넌트로 양쪽을 거의 정지(같은 순간의 일반 타격·패링 히트스탑보다 길면 늘어남).
	for (AActor* const Actor : { Owner, InVictim })
	{
		if (UknightHitStopComponent* const HitStop = Actor->FindComponentByClass<UknightHitStopComponent>())
		{
			HitStop->ApplyHitStop(EffectiveFreezeTime);
		}
	}

	// 타격 큐 위치: 타격 지점이 있으면 거기(일반 공격·스킬), 없으면(패링 그로기) 상대 몸에서 나를 향한 표면 쪽.
	CueParams = FGameplayCueParameters();
	CueParams.Instigator = Owner;
	CueParams.EffectCauser = Owner;
	if (const FHitResult* const Hit = Context.GetHitResult())
	{
		CueParams.Location = Hit->ImpactPoint;
		CueParams.Normal = Hit->ImpactNormal;
	}
	else
	{
		float Radius = 0.f;
		float HalfHeight = 0.f;
		InVictim->GetSimpleCollisionCylinder(Radius, HalfHeight);
		const FVector ToOwner = (Owner->GetActorLocation() - InVictim->GetActorLocation()).GetSafeNormal2D();
		CueParams.Location = InVictim->GetActorLocation() + ToOwner * Radius;
		CueParams.Normal = ToOwner;
	}
	UAbilitySystemComponent* const VictimASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InVictim);
	if (VictimASC && GroggyCueTag.IsValid())
	{
		VictimASC->ExecuteGameplayCue(GroggyCueTag, CueParams);
	}

	// 2·3. FreezeTime 뒤 '쾅'과 슬로모션. 기존 히트스톱 지연 뒤 슬로우 길이는 실시간 티커로 잰다.
	if (EffectiveFreezeTime > 0.f)
	{
		World->GetTimerManager().SetTimer(ReleaseDelayTimer, this, &ThisClass::StartRelease, EffectiveFreezeTime, false);
	}
	else
	{
		StartRelease();
	}

	UE_LOG(LogknightCombat, Log, TEXT("%s: groggy feedback on %s (freeze %.2fs + slow release %.2fs from x%.2f, %s)."), *GetNameSafe(Owner), *GetNameSafe(InVictim),
		EffectiveFreezeTime, bSkipGenericReleaseSlow ? 0.f : SlowReleaseTime, SlowReleaseStartSpeed, Context.GetHitResult() ? TEXT("hit point") : TEXT("no hit point (parry)"));
}

void UknightGroggyFeedbackComponent::StartRelease()
{
	AActor* const Owner = GetOwner();
	AActor* const VictimActor = Victim.Get();
	UWorld* const World = GetWorld();
	if (!Owner || !World)
	{
		return;
	}
	if (bCameraBeatActive && (!PredictedBoss.IsValid() || PredictedBoss->IsDead()))
	{
		CancelKnockdownPresentation();
		return;
	}
	if (bCameraBeatActive)
	{
		if (AknightPlayerCameraManager* const CameraManager = ZoomCameraManager.Get())
		{
			CameraManager->ReturnKnockdownZoom(PredictionZoomReturnTime);
		}
		PresentationEndTime = FPlatformTime::Seconds() + FMath::Max(PredictionZoomReturnTime, 0.f);
	}

	if (UAbilitySystemComponent* const VictimASC = VictimActor ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(VictimActor) : nullptr)
	{
		if (ReleaseCueTag.IsValid())
		{
			VictimASC->ExecuteGameplayCue(ReleaseCueTag, CueParams);
		}
	}
	if (bSkipGenericReleaseSlow)
	{
		bSkipGenericReleaseSlow = false;
		return;
	}

	// 원래 속도: 히트스탑이 아직 걸려 있으면(같은 순간에 끝나는 타이머) 그쪽이 1배속으로 돌려놓으므로 1로 본다.
	OwnerBaseSpeed = IsInHitStop(Owner) ? 1.f : Owner->CustomTimeDilation;
	VictimBaseSpeed = !VictimActor || IsInHitStop(VictimActor) ? 1.f : VictimActor->CustomTimeDilation;
	ReleaseStartTime = FPlatformTime::Seconds();
	bOwnerSpeedApplied = false;
	bVictimSpeedApplied = false;
	bReleasing = true;
	EnsureFeedbackTicker();
	TickRelease();
}

void UknightGroggyFeedbackComponent::TickRelease()
{
	if (!bReleasing || !GetWorld())
	{
		return;
	}
	const float Duration = SlowReleaseTime;
	const float Alpha = Duration > 0.f
		? FMath::Clamp(static_cast<float>(FPlatformTime::Seconds() - ReleaseStartTime) / Duration, 0.f, 1.f) : 1.f;
	const float Speed = FMath::Lerp(SlowReleaseStartSpeed, 1.f, Alpha * Alpha);

	bool bWaitingForHitStop = false;
	auto ApplySpeed = [Speed, &bWaitingForHitStop](AActor* Actor, float BaseSpeed, float& LastApplied, bool& bApplied)
	{
		if (!Actor)
		{
			return;
		}
		// 다른 타격의 히트스탑이 걸려 있으면 그 동안은 히트스탑 컴포넌트에 맡긴다(끝나면 다시 이어서 바꾼다).
		if (IsInHitStop(Actor))
		{
			bWaitingForHitStop = true;
			return;
		}
		LastApplied = BaseSpeed * Speed;
		Actor->CustomTimeDilation = LastApplied;
		bApplied = true;
	};
	ApplySpeed(GetOwner(), OwnerBaseSpeed, OwnerAppliedSpeed, bOwnerSpeedApplied);
	ApplySpeed(Victim.Get(), VictimBaseSpeed, VictimAppliedSpeed, bVictimSpeedApplied);
	if (Alpha >= 1.f && !bWaitingForHitStop)
	{
		FinishRelease();
	}
}

void UknightGroggyFeedbackComponent::EnsureFeedbackTicker()
{
	if (!FeedbackTicker.IsValid())
	{
		FeedbackTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickFeedback));
	}
}

void UknightGroggyFeedbackComponent::TickWatchedAttack()
{
	UGameplayAbility* const Attack = WatchedAttack.Get();
	UAnimMontage* const Montage = WatchedMontage.Get();
	const AknightCharacter* const Player = Cast<AknightCharacter>(GetOwner());
	UAnimInstance* const Anim = Player && Player->GetMesh() ? Player->GetMesh()->GetAnimInstance() : nullptr;
	if (!Attack || !Attack->IsActive() || !Montage || !Anim || !Anim->Montage_IsPlaying(Montage))
	{
		EndPredictionForAttack(Attack);
		return;
	}
	if (bWatchedPredictionAttempted || bCameraBeatActive)
	{
		return;
	}
	const float Position = Anim->Montage_GetPosition(Montage);
	const float BaseRate = Anim->Montage_GetPlayRate(Montage) * FMath::Max(Player->CustomTimeDilation, 0.01f);
	if (BaseRate <= 0.f)
	{
		return;
	}
	// Allow starting early in the attack motion if AttackWarp brings the player into range right after WatchAttack.
	constexpr float SwingLeadTime = 1.2f;

	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		const bool bIsMeleeTrace = Event.NotifyStateClass && Event.NotifyStateClass->IsA<UknightAnimNotifyState_MeleeTrace>();
		const bool bIsMeleeArea = Event.Notify && Event.Notify->IsA<UknightAnimNotify_MeleeArea>();
		if (!bIsMeleeTrace && !bIsMeleeArea)
		{
			continue;
		}
		const float StartRemaining = (Event.GetTriggerTime() - Position) / BaseRate;
		if (StartRemaining <= SwingLeadTime && StartRemaining > 0.08f
			&& TryStartPredictedKnockdown(Attack, WatchedHitParams))
		{
			bWatchedPredictionAttempted = true;
			return;
		}
	}
}

void UknightGroggyFeedbackComponent::TickPrediction()
{
	AknightCharacter* const Player = Cast<AknightCharacter>(GetOwner());
	AknightMonsterCharacterBase* const Boss = PredictedBoss.Get();
	UGameplayAbility* const Attack = PredictionAttack.Get();
	UAnimMontage* const Montage = WatchedMontage.Get();
	UAnimInstance* const Anim = Player && Player->GetMesh() ? Player->GetMesh()->GetAnimInstance() : nullptr;
	UknightTargetLockComponent* const Lock = Player ? Player->FindComponentByClass<UknightTargetLockComponent>() : nullptr;
	UAbilitySystemComponent* const SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player);
	UAbilitySystemComponent* const TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Boss);
	const double MaxPredictionDuration = FMath::Max<double>(PredictionMaxTime, 1.5);
	if (!Player || !Boss || !Attack || !Attack->IsActive() || !Montage || !Anim || !Anim->Montage_IsPlaying(Montage)
		|| !Lock || !SourceASC || !TargetASC
		|| IsInHitStop(Player) || IsInHitStop(Boss)
		|| SourceASC->HasMatchingGameplayTag(knightGameplayTags::State_Dead)
		|| SourceASC->HasMatchingGameplayTag(knightGameplayTags::State_HitReact)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Dead)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Invulnerable)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Guarding)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Parrying)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_HitReact_Knockdown)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::Window_MeleeTrace)
		|| FPlatformTime::Seconds() - PredictionStartTime >= MaxPredictionDuration)
	{
		BeginPredictionReturn();
		return;
	}

	const float Position = Anim->Montage_GetPosition(Montage);
	float FirstHitTriggerTime = MAX_FLT;
	bool bHasActiveOrPendingSwingWindow = false;
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		const bool bIsMeleeTrace = Event.NotifyStateClass && Event.NotifyStateClass->IsA<UknightAnimNotifyState_MeleeTrace>();
		const bool bIsMeleeArea = Event.Notify && Event.Notify->IsA<UknightAnimNotify_MeleeArea>();
		if (!bIsMeleeTrace && !bIsMeleeArea)
		{
			continue;
		}
		FirstHitTriggerTime = FMath::Min(FirstHitTriggerTime, Event.GetTriggerTime());
		const float WindowEnd = bIsMeleeTrace ? (Event.GetEndTriggerTime() + 0.02f) : (Event.GetTriggerTime() + 0.04f);
		if (Position <= WindowEnd)
		{
			bHasActiveOrPendingSwingWindow = true;
		}
	}
	if (!bHasActiveOrPendingSwingWindow)
	{
		BeginPredictionReturn();
		return;
	}

	const FVector PlayerLoc = Player->GetActorLocation();
	const float SurfaceDist2D = FindClosestMonsterBodyDistance2D(Boss, PlayerLoc, Boss->GetTargetLocation(Player));
	if (SurfaceDist2D > FMath::Max(PredictionRange + 80.f, 410.f))
	{
		BeginPredictionReturn();
		return;
	}
	if (bPredictionWasLocked && Lock->IsLockedOn() && Lock->GetLockedTarget() != Boss)
	{
		BeginPredictionReturn();
		return;
	}

	// Start in slow-motion right when the attack begins, then smoothly accelerate/release back to 1.0x speed
	// as the montage reaches the hit moment (FirstHitTriggerTime), so the slow-motion releases right as the enemy is hit.
	const double Elapsed = FPlatformTime::Seconds() - PredictionStartTime;
	const float FastEnterDuration = 0.04f;
	const float EnterAlpha = FMath::Clamp(static_cast<float>(Elapsed / FastEnterDuration), 0.f, 1.f);
	const float SlowSpeed = FMath::Clamp(PredictionSlowSpeed, 0.02f, 1.f);
	const float EnteredSpeed = FMath::Lerp(1.f, SlowSpeed, EaseInOut(EnterAlpha));

	const float HitTime = (FirstHitTriggerTime < MAX_FLT && FirstHitTriggerTime > 0.05f) ? FirstHitTriggerTime : 0.30f;
	const float AttackProgress = FMath::Clamp(Position / HitTime, 0.f, 1.f);
	const float ReleaseAlpha = FMath::Clamp((AttackProgress - 0.50f) / 0.48f, 0.f, 1.f);
	const float Speed = FMath::Lerp(EnteredSpeed, 1.f, EaseInOut(ReleaseAlpha));

	auto ApplySpeed = [Speed](AActor* Actor, float BaseSpeed, float& LastApplied, bool& bApplied)
	{
		if (bApplied && !FMath::IsNearlyEqual(Actor->CustomTimeDilation, LastApplied))
		{
			// Another system took ownership of this actor's dilation; do not overwrite it.
			bApplied = false;
			return;
		}
		if (!bApplied && !FMath::IsNearlyEqual(Actor->CustomTimeDilation, BaseSpeed))
		{
			return;
		}
		LastApplied = BaseSpeed * Speed;
		Actor->CustomTimeDilation = LastApplied;
		bApplied = true;
	};
	ApplySpeed(Player, OwnerBaseSpeed, OwnerAppliedSpeed, bOwnerSpeedApplied);
	ApplySpeed(Boss, VictimBaseSpeed, VictimAppliedSpeed, bVictimSpeedApplied);
}

bool UknightGroggyFeedbackComponent::TickFeedback(float DeltaRealTime)
{
	if (WatchedAttack.IsValid())
	{
		TickWatchedAttack();
	}
	if (bCameraBeatActive)
	{
		const AknightCharacter* const Player = Cast<AknightCharacter>(GetOwner());
		const AknightMonsterCharacterBase* const Boss = PredictedBoss.Get();
		const UWorld* const World = GetWorld();
		const APlayerController* const PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
		const AknightRaidGameState* const Raid = World ? World->GetGameState<AknightRaidGameState>() : nullptr;
		const EknightRaidPhase Phase = Raid ? Raid->GetRaidPhase() : EknightRaidPhase::Combat;
		if (!World || World->IsPaused() || !Player || Player->IsDead() || !Boss || Boss->IsDead()
			|| !PC || !PC->IsLocalController() || PC->GetViewTarget() != Player
			|| (Phase != EknightRaidPhase::Explore && Phase != EknightRaidPhase::Combat))
		{
			CancelKnockdownPresentation();
		}
	}
	if (bPredictionActive)
	{
		TickPrediction();
	}
	if (bReleasing)
	{
		TickRelease();
	}
	if (bCameraBeatActive && !bPredictionActive && FPlatformTime::Seconds() >= PresentationEndTime)
	{
		FinishPredictionVisual();
	}
	if (!bReleasing && !bCameraBeatActive && !WatchedAttack.IsValid())
	{
		FeedbackTicker.Reset();
		return false;
	}
	return true;
}

void UknightGroggyFeedbackComponent::FinishRelease()
{
	if (!bReleasing)
	{
		return;
	}
	bReleasing = false;
	RestoreAppliedSpeed(GetOwner(), OwnerAppliedSpeed, OwnerBaseSpeed, bOwnerSpeedApplied);
	RestoreAppliedSpeed(Victim.Get(), VictimAppliedSpeed, VictimBaseSpeed, bVictimSpeedApplied);
	bOwnerSpeedApplied = false;
	bVictimSpeedApplied = false;
}
