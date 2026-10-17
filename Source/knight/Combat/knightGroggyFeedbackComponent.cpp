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
	if (!Attack || !Montage || !Attack->IsActive())
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
	bWatchedPredictionAttempted = false;
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
	else if (!Lock->FindSoftTarget(Player->GetActorForwardVector(), PredictionRange, PredictionAngle,
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
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Dead)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Invulnerable)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Guarding)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Parrying)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_SuperArmor)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Reacting)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_HitReact_Knockdown))
	{
		return false;
	}
	FVector ToPoint = TargetPoint - Player->GetActorLocation();
	ToPoint.Z = 0.f;
	const float CenterDistance = ToPoint.Size();
	const float Radius = PartTag.IsValid() ? 0.f : ITargetableInterface::Execute_QueryTargetRadius(Boss);
	const float SurfaceDistance = FMath::Max(0.f, CenterDistance - Radius);
	const float CosAngle = CenterDistance > KINDA_SMALL_NUMBER
		? FVector::DotProduct(ToPoint / CenterDistance, Player->GetActorForwardVector().GetSafeNormal2D()) : 1.f;
	if (SurfaceDistance > PredictionRange || (SurfaceDistance > 50.f
		&& CosAngle < FMath::Cos(FMath::DegreesToRadians(PredictionAngle))))
	{
		return false;
	}
	if (bLocked)
	{
		FCollisionQueryParams SightParams(SCENE_QUERY_STAT(knightGroggyPredictionSight), false, Player);
		SightParams.AddIgnoredActor(Boss);
		FHitResult Obstruction;
		if (World->LineTraceSingleByChannel(Obstruction, Player->GetActorLocation(), TargetPoint, ECC_Visibility, SightParams))
		{
			return false;
		}
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
		CameraManager->StartKnockdownZoom(Player, PredictionZoomDegrees, PredictionZoomEnterTime, PredictionStartTime);
	}
	EnsureFeedbackTicker();
	UE_LOG(LogknightCombat, Verbose, TEXT("Predicted knockdown %s -> %s: poise %.1f, expected %.1f"),
		*GetNameSafe(Player), *GetNameSafe(Boss), TargetCore->GetPoise(), ExpectedPoiseDamage);
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
	BeginPredictionReturn();
	if (bMatchingTarget && bAccepted)
	{
		bPredictionHitPending = true;
	}
}

void UknightGroggyFeedbackComponent::CompletePredictedMeleeHit(AActor* HitTarget, bool bNewKnockdown)
{
	if (bPredictionHitPending && HitTarget == PredictedBoss.Get())
	{
		UE_LOG(LogknightCombat, Verbose, TEXT("Predicted hit on %s: new knockdown %s"),
			*GetNameSafe(HitTarget), bNewKnockdown ? TEXT("yes") : TEXT("no"));
		if (Victim.Get() == HitTarget)
		{
			// A poise-break event can arrive before the post-effect knockdown verdict.
			// Keep its existing release behavior when the prediction was wrong.
			bSkipGenericReleaseSlow = bNewKnockdown;
		}
		bPredictionHitPending = false;
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
	bSkipGenericReleaseSlow = bPredictedContact;
	// 앞 그로기의 슬로모션이 아직 진행 중이면 정리하고 새로 시작한다.
	World->GetTimerManager().ClearTimer(ReleaseDelayTimer);
	FinishRelease();
	Victim = InVictim;

	// 1. 멈춤: 히트스탑 컴포넌트로 양쪽을 거의 정지(같은 순간의 일반 타격·패링 히트스탑보다 길면 늘어남).
	for (AActor* const Actor : { Owner, InVictim })
	{
		if (UknightHitStopComponent* const HitStop = Actor->FindComponentByClass<UknightHitStopComponent>())
		{
			HitStop->ApplyHitStop(FreezeTime);
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
	if (FreezeTime > 0.f)
	{
		World->GetTimerManager().SetTimer(ReleaseDelayTimer, this, &ThisClass::StartRelease, FreezeTime, false);
	}
	else
	{
		StartRelease();
	}

	UE_LOG(LogknightCombat, Log, TEXT("%s: groggy feedback on %s (freeze %.2fs + slow release %.2fs from x%.2f, %s)."), *GetNameSafe(Owner), *GetNameSafe(InVictim),
		FreezeTime, SlowReleaseTime, SlowReleaseStartSpeed, Context.GetHitResult() ? TEXT("hit point") : TEXT("no hit point (parry)"));
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
	const float Rate = Anim->Montage_GetPlayRate(Montage) * FMath::Max(Player->CustomTimeDilation, 0.01f);
	if (Rate <= 0.f)
	{
		return;
	}
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		if (!(Event.NotifyStateClass && Event.NotifyStateClass->IsA<UknightAnimNotifyState_MeleeTrace>())
			&& !(Event.Notify && Event.Notify->IsA<UknightAnimNotify_MeleeArea>()))
		{
			continue;
		}
		const float Remaining = (Event.GetTriggerTime() - Position) / Rate;
		if (Remaining > 0.f && Remaining <= PredictionLeadTime
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
	UknightTargetLockComponent* const Lock = Player ? Player->FindComponentByClass<UknightTargetLockComponent>() : nullptr;
	UAbilitySystemComponent* const TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Boss);
	if (!Player || !Boss || !Attack || !Attack->IsActive() || !Lock || !TargetASC
		|| IsInHitStop(Player) || IsInHitStop(Boss)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Dead)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Invulnerable)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Guarding)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Parrying)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_SuperArmor)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_HitReact_Knockdown)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Reacting)
		|| FPlatformTime::Seconds() - PredictionStartTime >= PredictionMaxTime)
	{
		BeginPredictionReturn();
		return;
	}
	if (bPredictionWasLocked)
	{
		FVector TargetPoint;
		if (Lock->GetLockedTarget() != Boss || !Lock->GetLockedTargetLocation(TargetPoint))
		{
			BeginPredictionReturn();
			return;
		}
		FVector ToPoint = TargetPoint - Player->GetActorLocation();
		ToPoint.Z = 0.f;
		const float CenterDistance = ToPoint.Size();
		const float Radius = Lock->GetLockedPartTag().IsValid() ? 0.f : ITargetableInterface::Execute_QueryTargetRadius(Boss);
		const float SurfaceDistance = FMath::Max(0.f, CenterDistance - Radius);
		const float CosAngle = CenterDistance > KINDA_SMALL_NUMBER
			? FVector::DotProduct(ToPoint / CenterDistance, Player->GetActorForwardVector().GetSafeNormal2D()) : 1.f;
		if (SurfaceDistance > PredictionRange || (SurfaceDistance > 50.f
			&& CosAngle < FMath::Cos(FMath::DegreesToRadians(PredictionAngle))))
		{
			BeginPredictionReturn();
			return;
		}
	}
	else
	{
		FVector TargetPoint;
		AActor* CurrentTarget = nullptr;
		FGameplayTag PartTag;
		if (Lock->IsLockedOn() || !Lock->FindSoftTarget(Player->GetActorForwardVector(), PredictionRange,
			PredictionAngle, TargetPoint, CurrentTarget, PartTag) || CurrentTarget != Boss)
		{
			BeginPredictionReturn();
			return;
		}
	}

	const double Elapsed = FPlatformTime::Seconds() - PredictionStartTime;
	const float Alpha = PredictionSlowEnterTime > 0.f
		? FMath::Clamp(static_cast<float>(Elapsed / PredictionSlowEnterTime), 0.f, 1.f) : 1.f;
	const float Speed = FMath::Lerp(1.f, FMath::Clamp(PredictionSlowSpeed, 0.02f, 1.f), EaseInOut(Alpha));
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
