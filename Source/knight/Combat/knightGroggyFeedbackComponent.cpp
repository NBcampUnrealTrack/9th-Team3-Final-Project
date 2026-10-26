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
#include "Kismet/GameplayStatics.h"
#include "MotionWarpingComponent.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Raid/knightRaidGameState.h"
#include "knightCharacter.h"
#include "System/knightGameplayTags.h"
#include "TimerManager.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "knight.h"

namespace
{
	// 공격이 돌진할 자리(모션 워핑 목표). MeleeAttack·강공격이 WatchAttack 전에 정한다(락온 중일 때만).
	const FName PredictionWarpTargetName(TEXT("AttackTarget"));

	/** Position 뒤에 오는 첫 판정(근접 트레이스 시작 또는 범위 판정) 시각. 없으면 MAX_FLT. */
	float FindNextHitTime(const UAnimMontage* Montage, float Position)
	{
		float Best = MAX_FLT;
		if (!Montage)
		{
			return Best;
		}
		for (const FAnimNotifyEvent& Event : Montage->Notifies)
		{
			const bool bIsMeleeTrace = Event.NotifyStateClass && Event.NotifyStateClass->IsA<UknightAnimNotifyState_MeleeTrace>();
			const bool bIsMeleeArea = Event.Notify && Event.Notify->IsA<UknightAnimNotify_MeleeArea>();
			const float Trigger = Event.GetTriggerTime();
			if ((bIsMeleeTrace || bIsMeleeArea) && Trigger > Position)
			{
				Best = FMath::Min(Best, Trigger);
			}
		}
		return Best;
	}

	bool IsInHitStop(const AActor* Actor)
	{
		const UknightHitStopComponent* const HitStop = Actor ? Actor->FindComponentByClass<UknightHitStopComponent>() : nullptr;
		return HitStop && HitStop->IsInHitStop();
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
	// Hard default reference keeps the presentation material in the cooked dependency graph.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PullMaterial(TEXT("/Game/Knight/Camera/PostProcess/M_PP_GroggyPull.M_PP_GroggyPull"));
	RadialPullMaterial = PullMaterial.Object;
}

void UknightGroggyFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeRadialPull();
	PoiseBrokenHandle = UknightHitReactionComponent::OnAnyPoiseBroken.AddUObject(this, &ThisClass::HandleAnyPoiseBroken);
}

void UknightGroggyFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UknightHitReactionComponent::OnAnyPoiseBroken.Remove(PoiseBrokenHandle);
	PoiseBrokenHandle.Reset();
	CancelKnockdownPresentation();
	ShutdownRadialPull();
	SetPredictionAudio(false);
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

void UknightGroggyFeedbackComponent::WatchAttack(UGameplayAbility* Attack, UAnimMontage* Montage, const FknightMeleeHitParams& HitParams,
	const TSet<TWeakObjectPtr<AActor>>* AlreadyHitTargets)
{
	if (!Attack || !Montage || !Attack->IsActive() || Montage->bLoop)
	{
		return;
	}
	// (2026-10-20) 돌진 중에 시작한 예측 슬로를 베기 모션이 이어받는다. 슬로는 유지한 채 진행률만 베기 기준으로 바꾼다.
	if (bPredictionActive && bPredictionLead && PredictionAttack.Get() == Attack)
	{
		bLungeWatch = false;
		bPredictionLead = false;
		WatchedAttack = Attack;
		WatchedMontage = Montage;
		WatchedHitParams = HitParams;
		bWatchedPredictionAttempted = true;
		PredictionStartPosition = 0.f;
		PredictionHitTime = FindNextHitTime(Montage, 0.f);
		EnsureFeedbackTicker();
		return;
	}
	bLungeWatch = false;
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
	// (2026-10-20) 이미 맞힌 대상은 이번 몽타주에서 맞지 않으니 예측한 것으로 쳐서 건너뛴다.
	if (AlreadyHitTargets)
	{
		PredictedTargetsThisAttack.Append(*AlreadyHitTargets);
	}
	bWatchedPredictionAttempted = TryStartPredictedKnockdown(Attack, HitParams);
	EnsureFeedbackTicker();
}

void UknightGroggyFeedbackComponent::WatchLungeDash(UGameplayAbility* Attack, UAnimMontage* StrikeMontage, float StrikePlayRate,
	const FknightMeleeHitParams& HitParams, const FVector& Destination, float DashDuration)
{
	if (!Attack || !Attack->IsActive() || !StrikeMontage || StrikePlayRate <= 0.f)
	{
		return;
	}
	if (WatchedAttack.IsValid() && WatchedAttack.Get() != Attack)
	{
		EndPredictionForAttack(WatchedAttack.Get());
	}
	const float StrikeHitTime = FindNextHitTime(StrikeMontage, 0.f);
	if (StrikeHitTime == MAX_FLT)
	{
		return;
	}
	PredictedTargetsThisAttack.Reset();
	LungeAttack = Attack;
	LungeHitParams = HitParams;
	LungeDestination = Destination;
	LungeDashRemaining = FMath::Max(DashDuration, 0.f);
	LungeStrikeLead = StrikeHitTime / StrikePlayRate;
	bLungeWatch = true;
	EnsureFeedbackTicker();
	TickLungeWatch(0.f);
}

void UknightGroggyFeedbackComponent::TickLungeWatch(float DeltaRealTime)
{
	UGameplayAbility* const Attack = LungeAttack.Get();
	const AActor* const Owner = GetOwner();
	if (!bLungeWatch || !Attack || !Attack->IsActive() || !Owner)
	{
		bLungeWatch = false;
		return;
	}
	// 돌진은 루트 모션 힘이라 액터 시간(CustomTimeDilation)을 따른다.
	LungeDashRemaining = FMath::Max(0.f, LungeDashRemaining - DeltaRealTime * FMath::Max(Owner->CustomTimeDilation, 0.f));
	if (bCameraBeatActive || bReleasing)
	{
		return;
	}
	const float Remaining = LungeDashRemaining + LungeStrikeLead;
	// (2026-10-20 사용자 요청) 돌진 시작부터 슬로: 남은 시간 제한 없이 조건이 맞으면 바로 시작한다.
	const bool bInLeadWindow = bLungeSlowFromDashStart || Remaining <= FMath::Max(LungePredictionLeadTime, 0.1f);
	if (bInLeadWindow && Remaining > 0.08f
		&& TryStartPredictedKnockdown(Attack, LungeHitParams, Remaining, &LungeDestination))
	{
		bPredictionLead = true;
		bLungeWatch = false;
	}
}

bool UknightGroggyFeedbackComponent::TryStartPredictedKnockdown(UGameplayAbility* Attack, const FknightMeleeHitParams& HitParams,
	float LeadRemaining, const FVector* LeadDestination)
{
	const bool bLeadMode = LeadRemaining >= 0.f;
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
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_HitReact_Knockdown))
	{
		return false;
	}

	// (2026-10-20) 슬로는 첫 판정 PredictionLeadTime초(정상 속도 기준) 전부터만 시작한다. 더 일찍 시작하면 슬로가 길어져
	// 안전 제한(최대 시간)에 걸려 맞기 전에 풀렸고, 그러면 타격은 일반 그로기 연출(끝에만 슬로)으로 처리됐다.
	float MontagePosition = 0.f;
	float NextHitTime = MAX_FLT;
	float RemainingToHit = LeadRemaining;
	if (!bLeadMode)
	{
		UAnimInstance* const PlayerAnim = Player->GetMesh() ? Player->GetMesh()->GetAnimInstance() : nullptr;
		UAnimMontage* const AttackMontage = WatchedMontage.Get();
		if (!PlayerAnim || !AttackMontage || !PlayerAnim->Montage_IsPlaying(AttackMontage))
		{
			return false;
		}
		MontagePosition = PlayerAnim->Montage_GetPosition(AttackMontage);
		const float MontageRate = PlayerAnim->Montage_GetPlayRate(AttackMontage) * FMath::Max(Player->CustomTimeDilation, 0.01f);
		NextHitTime = FindNextHitTime(AttackMontage, MontagePosition);
		if (NextHitTime == MAX_FLT || MontageRate <= 0.f)
		{
			return false;
		}
		RemainingToHit = (NextHitTime - MontagePosition) / MontageRate;
		if (RemainingToHit > FMath::Max(PredictionLeadTime, 0.1f) || RemainingToHit < 0.025f)
		{
			return false;
		}
	}

	// (2026-10-20) 락온 공격은 모션 워핑으로 대상 앞까지 다가간다. 거리는 지금 자리 대신 도착할 자리(워핑 목표)에서 재고,
	// 그 자리는 대상 표면에서 WarpStopDistance(기본 150cm) 떨어져 있으니 표면 거리 허용치를 그만큼 늘린다.
	// 예전에는 현재 자리 기준 70cm라, 다가가는 동안에는 실패하다가 늦게(휘두르기 끝무렵) 시작하거나 아예 시작 못 했다.
	FVector PlayerLoc = Player->GetActorLocation();
	bool bUsingWarpTarget = false;
	if (LeadDestination)
	{
		// 돌진 베기: 도착 자리(대상 표면에서 StopDistance 앞)에서 잰다.
		PlayerLoc = FVector(LeadDestination->X, LeadDestination->Y, PlayerLoc.Z);
		bUsingWarpTarget = true;
	}
	else if (bLocked)
	{
		if (const UMotionWarpingComponent* const Warping = Player->FindComponentByClass<UMotionWarpingComponent>())
		{
			if (const FMotionWarpingTarget* const Warp = Warping->FindWarpTarget(PredictionWarpTargetName))
			{
				const FVector WarpLoc = Warp->GetLocation();
				if (FVector::Dist2D(WarpLoc, PlayerLoc) <= 700.f)
				{
					PlayerLoc = FVector(WarpLoc.X, WarpLoc.Y, PlayerLoc.Z);
					bUsingWarpTarget = true;
				}
			}
		}
	}
	const FVector PlayerForward2D = Player->GetActorForwardVector().GetSafeNormal2D();
	const FVector ViewForward2D = PC->GetControlRotation().Vector().GetSafeNormal2D();

	FVector ClosestBodyPoint = TargetPoint;
	// (돌진 베기는 StopDistance가 더 길 수 있어 아래 표면 허용치에 더해 준다.)
	const float LeadSlack = bLeadMode ? 120.f : 0.f;
	const float PointDistance2D = FVector::Dist2D(PlayerLoc, TargetPoint);
	const float SurfaceDistance2D = FindClosestMonsterBodyDistance2D(Boss, PlayerLoc, TargetPoint, &ClosestBodyPoint);
	const bool bIsLungingOrHeavyAttack = HitParams.PoiseDamage >= 20.f;
	const float WarpSlack = bUsingWarpTarget ? 160.f : 0.f;
	// 대검의 실제 타격 사거리(칼끝 리치 약 250~320cm)에 맞춰 표면·포인트 허용 거리를 넉넉히 잡는다.
	const float MaxSurfaceRange = (bIsLungingOrHeavyAttack ? 260.f : 185.f) + WarpSlack + LeadSlack;
	const float MaxPointRange = (bIsLungingOrHeavyAttack ? 540.f : 420.f) + LeadSlack;
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
	PredictionStartPosition = MontagePosition;
	PredictionHitTime = NextHitTime;
	bPredictionLead = bLeadMode;
	// (2026-10-20) 배속과 안전 제한을 이번 슬로에 맞게 정한다. 제한 = 맞기까지 슬로로 늘어난 시간 + 여유.
	ActivePredictionSlowSpeed = FMath::Clamp(bLeadMode ? LungeSlowSpeed : PredictionSlowSpeed, 0.02f, 1.f);
	ActivePredictionMaxDuration = FMath::Max<double>(PredictionMaxTime, RemainingToHit / FMath::Max(ActivePredictionSlowSpeed, 0.1f) + 0.6);
	Player->OnDeathStarted.AddUniqueDynamic(this, &ThisClass::HandlePlayerDied);
	Boss->OnMonsterDefeated.AddUniqueDynamic(this, &ThisClass::HandleBossDefeated);
	ZoomCameraManager = Cast<AknightPlayerCameraManager>(PC->PlayerCameraManager);
	if (AknightPlayerCameraManager* const CameraManager = ZoomCameraManager.Get())
	{
		CameraManager->StartKnockdownZoom(Player, PredictionZoomDegrees, FMath::Min(PredictionZoomEnterTime, 0.22f), PredictionStartTime);
	}
	// (2026-10-20) 슬로 동안 소리를 먹먹하게. 맞는 순간(ResolvePredictedMeleeContact) 바로 되돌린다.
	SetPredictionAudio(true);
	EnsureFeedbackTicker();
	UE_LOG(LogknightCombat, Log, TEXT("Predicted knockdown %s -> %s: poise %.1f, expected %.1f (surfaceDist %.0f, pointDist %.0f, %s, %.2fs to hit)"),
		*GetNameSafe(Player), *GetNameSafe(Boss), TargetCore->GetPoise(), ExpectedPoiseDamage, SurfaceDistance2D, PointDistance2D,
		bLeadMode ? TEXT("lunge dash") : (bUsingWarpTarget ? TEXT("from warp target") : TEXT("from player")), RemainingToHit);
	PlayRadialPull(Boss, TargetPoint);
	OnPresentationEvent.Broadcast(false);
	UE_LOG(LogknightCombat, Verbose, TEXT("Predicted knockdown %s -> %s: poise %.1f, expected %.1f"),
		*GetNameSafe(Player), *GetNameSafe(Boss), TargetCore->GetPoise(), ExpectedPoiseDamage);
	return true;
}

void UknightGroggyFeedbackComponent::EndPredictionForAttack(const UGameplayAbility* Attack)
{
	if (LungeAttack.Get() == Attack)
	{
		bLungeWatch = false;
		LungeAttack.Reset();
	}
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
		SetPredictionAudio(false);
		bPredictionActive = false;
		PredictionAttack.Reset();
		bPredictionHitPending = true;
		// (2026-10-20) 맞는 순간 줌을 한 번 더 당긴다(멈춤 동안 유지, 풀릴 때 함께 돌아감).
		if (AknightPlayerCameraManager* const CameraManager = ZoomCameraManager.Get())
		{
			CameraManager->PunchKnockdownZoom(ImpactZoomPunchDegrees, ImpactZoomPunchTime);
		}
		PresentationEndTime = FPlatformTime::Seconds() + FMath::Max(PredictedImpactFreezeTime, 0.f) + FMath::Max(PredictionZoomReturnTime, 0.f) + 0.5;
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
	CancelRadialPull();
	bPredictionLead = false;
	FinishPredictionSlow();
	SetPredictionAudio(false);
	bPredictionActive = false;
	PredictionAttack.Reset();
	if (AknightPlayerCameraManager* const CameraManager = ZoomCameraManager.Get())
	{
		CameraManager->ReturnKnockdownZoom(PredictionZoomReturnTime);
	}
	PresentationEndTime = FPlatformTime::Seconds() + FMath::Max(PredictionZoomReturnTime, 0.f);
}

void UknightGroggyFeedbackComponent::SetPredictionAudio(const bool bMuffled)
{
	if (bMuffled == bPredictionAudioPushed)
	{
		return;
	}
	if (bMuffled && PredictionMuffledClasses.IsEmpty())
	{
		return;
	}
	if (bMuffled && !PredictionSoundMix)
	{
		// 튜닝값(클래스·주파수·볼륨)만 컴포넌트에 두고, 믹스는 처음 쓸 때 런타임에 만든다.
		PredictionSoundMix = NewObject<USoundMix>(this, TEXT("GroggyPredictionMix"));
		PredictionSoundMix->FadeInTime = FMath::Max(PredictionSlowEnterTime, 0.02f);
		PredictionSoundMix->FadeOutTime = 0.03f;
		for (USoundClass* const SoundClass : PredictionMuffledClasses)
		{
			if (!SoundClass)
			{
				continue;
			}
			FSoundClassAdjuster Adjuster;
			Adjuster.SoundClassObject = SoundClass;
			Adjuster.VolumeAdjuster = PredictionVolumeScale;
			Adjuster.LowPassFilterFrequency = PredictionLowPassFrequency;
			Adjuster.bApplyToChildren = true;
			PredictionSoundMix->SoundClassEffects.Add(Adjuster);
		}
	}
	if (!PredictionSoundMix)
	{
		return;
	}
	if (bMuffled)
	{
		UGameplayStatics::PushSoundMixModifier(this, PredictionSoundMix);
	}
	else
	{
		UGameplayStatics::PopSoundMixModifier(this, PredictionSoundMix);
	}
	bPredictionAudioPushed = bMuffled;
}

void UknightGroggyFeedbackComponent::FinishPredictionVisual()
{
	if (!bCameraBeatActive)
	{
		return;
	}
	FinishPredictionSlow();
	SetPredictionAudio(false);
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
	CancelRadialPull();
	bLungeWatch = false;
	bPredictionLead = false;
	LungeAttack.Reset();
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

bool UknightGroggyFeedbackComponent::IsPresentationActive() const
{
	const UWorld* const World = GetWorld();
	return bCameraBeatActive || bReleasing || (World && World->GetTimerManager().IsTimerActive(ReleaseDelayTimer));
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
	// (2026-10-20) 예측 슬로로 들어온 타격은 멈춤을 PredictedImpactFreezeTime(기본 0.14초)만큼. 예전엔 0.08초 고정.
	const float EffectiveFreezeTime = bPredictedContact ? PredictedImpactFreezeTime : FreezeTime;
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

	// 예측 슬로우 없이 터진 그로기(퍼펙트 가드 패링, 칼끝 사거리 타격 등)에서도 시네마틱 줌 + 포스트프로세스 연출이 빠지지 않게 건다.
	if (!bPredictedContact)
	{
		if (AknightCharacter* const Player = Cast<AknightCharacter>(Owner))
		{
			if (APlayerController* const PC = Cast<APlayerController>(Player->GetController()))
			{
				if (PC->IsLocalController() && PC->GetViewTarget() == Player)
				{
					ZoomCameraManager = Cast<AknightPlayerCameraManager>(PC->PlayerCameraManager);
					if (AknightPlayerCameraManager* const CameraManager = ZoomCameraManager.Get())
					{
						const double Now = FPlatformTime::Seconds();
						CameraManager->StartKnockdownZoom(Player, PredictionZoomDegrees, 0.06f, Now);
						CameraManager->PunchKnockdownZoom(ImpactZoomPunchDegrees, ImpactZoomPunchTime);
						PredictedBoss = Cast<AknightMonsterCharacterBase>(InVictim);
						bCameraBeatActive = true;
						PresentationEndTime = Now + EffectiveFreezeTime + SlowReleaseTime + FMath::Max(PredictionZoomReturnTime, 0.f) + 0.2;
						EnsureFeedbackTicker();
					}
				}
			}
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

	PlayRadialPull(InVictim, CueParams.Location);
	OnPresentationEvent.Broadcast(true);

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
	bReleaseVictimOnly = false;
	ActiveReleaseTime = SlowReleaseTime;
	ActiveReleaseStartSpeed = SlowReleaseStartSpeed;
	if (bSkipGenericReleaseSlow)
	{
		bSkipGenericReleaseSlow = false;
		// (2026-10-20) 예측 슬로 뒤에는 나이트는 바로 정상 속도, 보스만 잠깐 느리게 쓰러지는 여운.
		if (VictimFollowThroughTime <= 0.f || !VictimActor)
		{
			return;
		}
		bReleaseVictimOnly = true;
		ActiveReleaseTime = VictimFollowThroughTime;
		ActiveReleaseStartSpeed = VictimFollowThroughStartSpeed;
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
	const float Duration = ActiveReleaseTime;
	const float Alpha = Duration > 0.f
		? FMath::Clamp(static_cast<float>(FPlatformTime::Seconds() - ReleaseStartTime) / Duration, 0.f, 1.f) : 1.f;
	const float Speed = FMath::Lerp(ActiveReleaseStartSpeed, 1.f, Alpha * Alpha);

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
	if (!bReleaseVictimOnly)
	{
		ApplySpeed(GetOwner(), OwnerBaseSpeed, OwnerAppliedSpeed, bOwnerSpeedApplied);
	}
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
	// (2026-10-20) 예전에는 1.2초 전부터 시작해서 슬로가 안전 제한에 걸려 맞기 전에 풀렸다. 시작 시점은 TryStartPredictedKnockdown이 PredictionLeadTime으로 거른다.
	const float SwingLeadTime = FMath::Max(PredictionLeadTime, 0.1f);

	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		const bool bIsMeleeTrace = Event.NotifyStateClass && Event.NotifyStateClass->IsA<UknightAnimNotifyState_MeleeTrace>();
		const bool bIsMeleeArea = Event.Notify && Event.Notify->IsA<UknightAnimNotify_MeleeArea>();
		if (!bIsMeleeTrace && !bIsMeleeArea)
		{
			continue;
		}
		const float StartRemaining = (Event.GetTriggerTime() - Position) / BaseRate;
		if (StartRemaining <= SwingLeadTime && StartRemaining > 0.025f
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
	// (2026-10-20) 안전 제한: 맞기까지 슬로로 걸리는 시간 + 여유. 예전 1.5초 고정은 슬로가 길어지면 맞기 전에 풀려 버렸다.
	const double MaxPredictionDuration = ActivePredictionMaxDuration;
	const bool bMontageMissing = !bPredictionLead && (!Montage || !Anim || !Anim->Montage_IsPlaying(Montage));
	if (!Player || !Boss || !Attack || !Attack->IsActive() || bMontageMissing
		|| !Lock || !SourceASC || !TargetASC
		|| IsInHitStop(Player) || IsInHitStop(Boss)
		|| SourceASC->HasMatchingGameplayTag(knightGameplayTags::State_Dead)
		|| SourceASC->HasMatchingGameplayTag(knightGameplayTags::State_HitReact)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Dead)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Invulnerable)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Guarding)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Parrying)
		|| TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_HitReact_Knockdown)
		|| FPlatformTime::Seconds() - PredictionStartTime >= MaxPredictionDuration)
	{
		BeginPredictionReturn();
		return;
	}

	// (2026-10-20) 돌진 중(아직 베기 전): 슬로로 들어가서 유지만 한다. 베기가 시작되면 WatchAttack이 모션 기준으로 바꾼다.
	if (bPredictionLead)
	{
		if (bPredictionWasLocked && Lock->IsLockedOn() && Lock->GetLockedTarget() != Boss)
		{
			BeginPredictionReturn();
			return;
		}
		const double LeadElapsed = FPlatformTime::Seconds() - PredictionStartTime;
		const float LeadEnter = FMath::Clamp(static_cast<float>(LeadElapsed / FMath::Max(PredictionSlowEnterTime, 0.01f)), 0.f, 1.f);
		const float LeadSpeed = FMath::Lerp(1.f, ActivePredictionSlowSpeed, 1.f - FMath::Square(1.f - LeadEnter));
		auto ApplyLeadSpeed = [LeadSpeed](AActor* Actor, float BaseSpeed, float& LastApplied, bool& bApplied)
		{
			if (bApplied && !FMath::IsNearlyEqual(Actor->CustomTimeDilation, LastApplied))
			{
				bApplied = false;
				return;
			}
			if (!bApplied && !FMath::IsNearlyEqual(Actor->CustomTimeDilation, BaseSpeed))
			{
				return;
			}
			LastApplied = BaseSpeed * LeadSpeed;
			Actor->CustomTimeDilation = LastApplied;
			bApplied = true;
		};
		ApplyLeadSpeed(Player, OwnerBaseSpeed, OwnerAppliedSpeed, bOwnerSpeedApplied);
		ApplyLeadSpeed(Boss, VictimBaseSpeed, VictimAppliedSpeed, bVictimSpeedApplied);
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
	// (2026-10-20) 들어갈 때: PredictionSlowEnterTime 동안 감속 곡선(처음 빠르고 끝에 부드럽게). 예전에는 0.04초에 뚝.
	// 풀 때: 첫 판정까지 PredictionWhipStartProgress만큼은 슬로를 유지하고, 남은 구간에서 가속 곡선으로 확 빨라져 정상 속도로 맞힌다(채찍).
	const double Elapsed = FPlatformTime::Seconds() - PredictionStartTime;
	const float EnterDuration = FMath::Max(PredictionSlowEnterTime, 0.01f);
	const float EnterAlpha = FMath::Clamp(static_cast<float>(Elapsed / EnterDuration), 0.f, 1.f);
	const float EnterEase = 1.f - FMath::Square(1.f - EnterAlpha);
	const float SlowSpeed = ActivePredictionSlowSpeed;
	const float EnteredSpeed = FMath::Lerp(1.f, SlowSpeed, EnterEase);

	// (2026-10-20) 진행률은 모션 처음이 아니라 슬로가 시작된 자리에서 맞는 자리(그때 잰 다음 판정)까지로 잰다.
	// 그래야 늦게 시작해도 들어가기 → 유지 → 채찍이 다 나온다(예전에는 모션 처음 기준이라 늦게 시작하면 바로 풀렸다).
	const float HitTime = PredictionHitTime < MAX_FLT ? PredictionHitTime
		: ((FirstHitTriggerTime < MAX_FLT && FirstHitTriggerTime > 0.05f) ? FirstHitTriggerTime : 0.30f);
	const float Span = FMath::Max(HitTime - PredictionStartPosition, 0.01f);
	const float AttackProgress = FMath::Clamp((Position - PredictionStartPosition) / Span, 0.f, 1.f);
	const float WhipStart = FMath::Clamp(PredictionWhipStartProgress, 0.f, 0.98f);
	const float ReleaseAlpha = FMath::Clamp((AttackProgress - WhipStart) / FMath::Max(0.99f - WhipStart, 0.01f), 0.f, 1.f);
	const float Speed = FMath::Lerp(EnteredSpeed, 1.f, ReleaseAlpha * ReleaseAlpha);

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
	if (bLungeWatch)
	{
		TickLungeWatch(DeltaRealTime);
	}
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
	if (!bReleasing && !bCameraBeatActive && !WatchedAttack.IsValid() && !bLungeWatch)
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
