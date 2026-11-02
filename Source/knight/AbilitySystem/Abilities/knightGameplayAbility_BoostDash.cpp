#include "AbilitySystem/Abilities/knightGameplayAbility_BoostDash.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/knightMovementComponent.h"
#include "Combat/knightGhostTrailActor.h"
#include "Combat/knightTargetLockComponent.h"
#include "Engine/World.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "knight.h"
#include "Perception/AISense_Hearing.h"
#include "System/knightGameplayTags.h"

UknightGameplayAbility_BoostDash::UknightGameplayAbility_BoostDash(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EknightAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = EknightAbilityActivationGroup::Exclusive_Replaceable;
	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_Boost_Dash));
	// (2026-10-29 사용자 결정) 구르기로 바꾸며 State.Boosting 대신 State.Rolling을 단다.
	// State.Boosting이면 카메라가 Boost 모드(축소)로 바뀐다. State.Rolling 동안은 카메라가 뒤따라오는 거리를 그대로 둔다(knightCameraMode_ThirdPerson).
	ActivationOwnedTags.AddTag(knightGameplayTags::State_Rolling);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_HitReact);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Overheated);
	DashCueTag = knightGameplayTags::GameplayCue_Boost_Dash;

	// 경직 중 대시 캔슬: 실행 중에 다시 발동되면 지금 대시를 끝내고 새로 시작한다. 언제 허용할지는 CanActivateAbility가 정한다.
	bRetriggerInstancedAbility = true;

	// 퀵스텝은 행동 에너지가 비용만큼 있어야 나간다(2026-10-09 사용자 결정). 비용 값은 BP의 StaminaCost(전에는 Cost GE).
	// (2026-10-26 사용자 결정) 퀵스텝도 다른 행동처럼 조금이라도 남아 있으면 나가고, 모자란 만큼 가불한다.
	bRequireFullStaminaCost = false;

	// 등 부스터 두 개: 등 뒤쪽(X -), 양옆(Y ±). (2026-10-12)
	BackThrusterOffsets = { FVector(-15.f, -12.f, 4.f), FVector(-15.f, 12.f, 4.f) };
}

float UknightGameplayAbility_BoostDash::GetMechAttributeValue(const FGameplayAttribute& Attribute, float Fallback) const
{
	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	return ASC && ASC->HasAttributeSetForAttribute(Attribute) ? ASC->GetNumericAttribute(Attribute) : Fallback;
}

bool UknightGameplayAbility_BoostDash::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	// (2026-10-29 사용자 결정) 퀵스텝 비활성화 중이면 발동하지 않는다.
	if (!bQuickstepEnabled)
	{
		return false;
	}
	// 퀵스텝 쿨타임 중이면 발동하지 않는다.
	const UWorld* const World = GetWorld();
	if (World && World->GetTimeSeconds() < CooldownEndTime)
	{
		return false;
	}
	// (2026-10-15) 방향키 없이 누르면 나가지 않는다(백스텝 없음).
	if (bRequireMoveInput)
	{
		const ACharacter* const Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
		const UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
		if (!Movement || Movement->GetLastInputVector().IsNearlyZero())
		{
			return false;
		}
	}
	// (2026-10-29 사용자 결정) 구르기는 땅에서만 나간다. 점프·낙하 중에는 눌러도 나가지 않는다.
	{
		const ACharacter* const Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
		const UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
		if (!Movement || !Movement->IsMovingOnGround())
		{
			return false;
		}
	}
	// 이미 대시 중이면 경직 중일 때만 다시 발동(새 대시로 다시 시작)을 허용한다. 이동 도중에는 끊지 않는다.
	if (IsActive() && !bInRecovery)
	{
		return false;
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UknightGameplayAbility_BoostDash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	StartDash(*Character);
}

void UknightGameplayAbility_BoostDash::StartDash(ACharacter& Character)
{
	bCooldownPending = true;

	FVector Direction = Character.GetCharacterMovement()->GetLastInputVector().GetSafeNormal2D();
	const bool bHasInput = !Direction.IsNearlyZero();
	if (!bHasInput)
	{
		Direction = -Character.GetActorForwardVector().GetSafeNormal2D();
	}
	DashDirection = Direction;

	const UAbilitySystemComponent* const OwnerASC = GetAbilitySystemComponentFromActorInfo();
	const bool bLockedOn = OwnerASC && OwnerASC->HasMatchingGameplayTag(knightGameplayTags::State_LockedOn);
	if (bLockedOn)
	{
		if (const UknightTargetLockComponent* const TargetLock = Character.FindComponentByClass<UknightTargetLockComponent>())
		{
			FVector TargetLocation;
			if (TargetLock->GetLockedTargetLocation(TargetLocation))
			{
				const FVector ToTarget = (TargetLocation - Character.GetActorLocation()).GetSafeNormal2D();
				if (!ToTarget.IsNearlyZero())
				{
					Character.SetActorRotation(FRotator(0.f, ToTarget.Rotation().Yaw, 0.f));
				}
			}
		}
	}
	else if (bFaceDashDirectionWhenFree && bHasInput)
	{
		Character.SetActorRotation(FRotator(0.f, Direction.Rotation().Yaw, 0.f));
	}
	float MontageYaw = 0.f;
	UAnimMontage* const Montage = SelectDashMontage(Character, Direction, &MontageYaw);

	// (2026-10-29) 락온 중 대각선: 고른 구르기 방향(MontageYaw)이 실제 이동 방향과 맞도록 몸을 돌린다(대상 기준 최대 45°).
	if (bLockedOn && bAlignRollToDirectionWhenLockedOn && bHasInput)
	{
		Character.SetActorRotation(FRotator(0.f, Direction.Rotation().Yaw - MontageYaw, 0.f));
	}
	// (2026-10-29 사용자 결정) 구르는 동안은 방향키를 받지 않는다(락온 여부와 상관없이 구르기 시작부터 끝까지).
	// 이동 잠금으로 가속을 0으로 만들어, 비락온 중 방향키 쪽으로 몸이 도는 것(bOrientRotationToMovement)과
	// 락온 회전(knightMovementComponent)을 모두 멈춘다. 그래서 구르는 도중 애니메이션 방향이 바뀌지 않는다.
	// 잠금은 입력 방향은 남겨서, 경직 중 다시 구르기 입력은 그대로 받는다. 어빌리티가 끝나면 푼다.
	UknightMovementComponent* const RollMovement = Cast<UknightMovementComponent>(Character.GetCharacterMovement());
	if (RollMovement && !bFacingLockApplied)
	{
		RollMovement->SetMoveInputLocked(true);
		bFacingLockApplied = true;
	}

	if (InvulnerableDuration > 0.f)
	{
		if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->AddLooseGameplayTag(knightGameplayTags::State_Invulnerable);
			bInvulnerableApplied = true;
		}
		UAbilityTask_WaitDelay* const Task = UAbilityTask_WaitDelay::WaitDelay(this, InvulnerableDuration);
		Task->OnFinish.AddDynamic(this, &ThisClass::OnInvulnerabilityEnded);
		Task->ReadyForActivation();
	}

	if (Montage)
	{
		UAbilityTask_PlayMontageAndWait* const Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, Montage, GetMontagePlayRate(Montage), NAME_None, true, 0.f);
		Task->ReadyForActivation();
		PlayingDashMontage = Montage;
	}

	// 거리 = 기본 거리 x 퀵스텝 속도 배율 x 중량 계수. 시간은 고정이라 거리가 늘면 속도도 빨라진다.
	// (2026-10-21) 중량 계수는 퀵스텝 전용(적재율 50%부터 최대 15%)을 쓴다. 이동용 계수는 0%부터 최대 30%라 회피가 너무 짧아졌다.
	const UknightMovementComponent* const KnightMovement = Cast<UknightMovementComponent>(Character.GetCharacterMovement());
	const float WeightFactor = KnightMovement ? KnightMovement->GetDodgeWeightFactor() : 1.f;
	const float Distance = DashDistance * GetMechAttributeValue(UknightAttributeSet::GetDodgeSpeedAttribute(), 1.f) * WeightFactor;
	const float Strength = Distance / DashDuration;
	UAbilityTask_ApplyRootMotionConstantForce* const DashTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("BoostDash"), Direction, Strength, DashDuration, false, StrengthOverTime,
		ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, Character.GetCharacterMovement()->GetMaxSpeed(), false);
	DashTask->OnFinish.AddDynamic(this, &ThisClass::OnDashFinished);
	DashTask->ReadyForActivation();

	StartBackThrusters(Character);
	if (bSpawnGhostTrail)
	{
		SpawnGhostStep();
		if (UWorld* const World = GetWorld(); World && GhostSpawnInterval > 0.f)
		{
			World->GetTimerManager().SetTimer(GhostTrailTimer, this, &ThisClass::SpawnGhostStep, GhostSpawnInterval, true);
		}
	}

	UAbilitySystemComponent* const CueASC = DashCueTag.IsValid() ? GetAbilitySystemComponentFromActorInfo() : nullptr;
	if (UAbilitySystemComponent* const ASC = CueASC)
	{
		FGameplayCueParameters CueParams;
		CueParams.Location = Character.GetActorLocation();
		CueParams.Normal = Direction;
		CueParams.Instigator = &Character;
		ASC->ExecuteGameplayCue(DashCueTag, CueParams);
	}
	if (DashSound)
	{
		UGameplayStatics::PlaySoundAtLocation(&Character, DashSound, Character.GetActorLocation(), DashSoundVolume);
	}

	// 추진기는 시끄럽다: 아직 나이트를 못 본 몬스터도 소리를 들을 수 있다 (AI 청각).
	UAISense_Hearing::ReportNoiseEvent(&Character, Character.GetActorLocation(), NoiseLoudness, &Character, NoiseRange, TEXT("Boost"));
}

float UknightGameplayAbility_BoostDash::GetMontagePlayRate(const UAnimMontage* Montage) const
{
	// (2026-10-29) MontageMoveEndTime이 있으면 몽타주의 이동 구간(0 ~ MontageMoveEndTime)을 DashDuration에 맞춘다.
	// 나머지(일어나는 동작)는 경직이 시작될 때 OnDashFinished가 RecoveryDuration에 맞춰 배속을 바꾼다.
	if (MontageMoveEndTime > 0.f && DashDuration > 0.f)
	{
		return MontageMoveEndTime / DashDuration;
	}
	// 그 밖의 규칙: 고정 배속(MontagePlayRate). 헤더 주석 참고 — 바뀔 수 있는 항목.
	return MontagePlayRate;
}

UAnimMontage* UknightGameplayAbility_BoostDash::SelectDashMontage(const ACharacter& Character, const FVector& Direction, float* OutMontageYaw) const
{
	const FVector Forward = Character.GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = Character.GetActorRightVector().GetSafeNormal2D();
	const float ForwardDot = FVector::DotProduct(Direction, Forward);
	const float RightDot = FVector::DotProduct(Direction, Right);
	// 바라보는 방향 기준 대시 방향 각도(오른쪽 +, -180 ~ 180).
	const float RelativeYaw = FMath::RadiansToDegrees(FMath::Atan2(RightDot, ForwardDot));

	// (2026-10-29) 한손 스탠스면 한손 칸들을 쓴다. 한손 정면 칸이 비어 있으면 한손에서도 양손 칸을 쓴다.
	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	const bool bOneHanded = OneHandedDashMontage && !(ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_Stance_TwoHanded));
	UAnimMontage* const Front = bOneHanded ? OneHandedDashMontage.Get() : DashMontage.Get();
	UAnimMontage* const Back = bOneHanded ? OneHandedDashMontageBack.Get() : DashMontageBack.Get();
	UAnimMontage* const BackRight = bOneHanded ? OneHandedDashMontageBackRight.Get() : DashMontageBackRight.Get();
	UAnimMontage* const Left = bOneHanded ? OneHandedDashMontageLeft.Get() : DashMontageLeft.Get();
	UAnimMontage* const RightMontage = bOneHanded ? OneHandedDashMontageRight.Get() : DashMontageRight.Get();

	// 가장 가까운 방향: 앞 ±45° 안, 뒤 ±135° 밖, 그 사이는 옆. 정확한 대각선(45°·135°)은 앞·뒤로 친다.
	// 방향 몽타주가 비어 있으면 정면(MontageYaw 0)을 쓴다.
	UAnimMontage* Picked = Front;
	float MontageYaw = 0.f;
	if (FMath::Abs(RelativeYaw) >= 135.f)
	{
		// (2026-10-29) 뒤: 오른쪽 뒤는 BackRight(BR, 오른쪽으로 몸을 틀며 뒤로), 왼쪽 뒤는 Back(BL).
		UAnimMontage* const BackPick = RelativeYaw > 0.f && BackRight ? BackRight : Back;
		if (BackPick)
		{
			Picked = BackPick;
			MontageYaw = RelativeYaw > 0.f ? 180.f : -180.f;
		}
	}
	else if (FMath::Abs(RelativeYaw) > 45.f)
	{
		UAnimMontage* const SidePick = RelativeYaw > 0.f ? RightMontage : Left;
		if (SidePick)
		{
			Picked = SidePick;
			MontageYaw = RelativeYaw > 0.f ? 90.f : -90.f;
		}
	}
	if (OutMontageYaw)
	{
		*OutMontageYaw = MontageYaw;
	}
	return Picked;
}

void UknightGameplayAbility_BoostDash::OnDashFinished()
{
	StopGhostTrail();
	// 등 부스터는 이동이 끝나면 잠깐 뒤에 끈다. (2026-10-12)
	if (UWorld* const World = GetWorld(); World && BackThrusterLinger > 0.f)
	{
		World->GetTimerManager().SetTimer(BackThrusterStopTimer, this, &ThisClass::StopBackThrusters, BackThrusterLinger, false);
	}
	else
	{
		StopBackThrusters();
	}
	// 쿨타임은 이동이 끝난 시점부터 센다. 그래야 경직 중에 쿨타임이 끝나 대시로 캔슬할 수 있다.
	StartCooldown();
	if (RecoveryDuration <= 0.f)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
		return;
	}

	// 경직: 공격·가드가 대시를 대신할 수 있게 캔슬 창을 열고, 이동 입력으로는 가속하지 못하게 한다(미끄러지며 정지).
	bInRecovery = true;

	// (2026-10-29) 몽타주의 나머지(일어나는 동작)를 경직 시간에 맞춰 재생한다. 남은 길이를 재서 정확히 경직 끝에 맞춘다.
	if (MontageMoveEndTime > 0.f)
	{
		const ACharacter* const AvatarCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
		UAnimInstance* const AnimInstance = AvatarCharacter && AvatarCharacter->GetMesh() ? AvatarCharacter->GetMesh()->GetAnimInstance() : nullptr;
		UAnimMontage* const Montage = PlayingDashMontage.Get();
		if (AnimInstance && Montage && AnimInstance->Montage_IsPlaying(Montage))
		{
			const float Position = AnimInstance->Montage_GetPosition(Montage);
			const float Remaining = Montage->GetPlayLength() - Position;
			if (Remaining > KINDA_SMALL_NUMBER)
			{
				const float StandUpRate = Remaining / RecoveryDuration;
				AnimInstance->Montage_SetPlayRate(Montage, StandUpRate);

				// (2026-10-29 사용자 결정) 끝까지 일어나되, 몸이 다 일어선 뒤에는 방향키를 누르면 바로 출발한다.
				// 일어선 시각(몽타주 기준)이 될 때부터 매 프레임 방향키를 본다(CheckStandUpMove).
				const float* const MontageStandUp = MontageStandUpTimes.Find(Montage);
				const float StandUpTime = MontageStandUp ? *MontageStandUp : MontageStandUpTime;
				UWorld* const World = GetWorld();
				if (World && StandUpTime > 0.f && StandUpTime < Montage->GetPlayLength())
				{
					const float Delay = FMath::Max((StandUpTime - Position) / StandUpRate, KINDA_SMALL_NUMBER);
					World->GetTimerManager().SetTimer(StandUpMoveTimer, this, &ThisClass::CheckStandUpMove, 0.02f, true, Delay);
				}
			}
		}
	}
	if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AddLooseGameplayTag(knightGameplayTags::Window_Cancel);
		bCancelWindowApplied = true;
	}
	if (const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UknightMovementComponent* const KnightMovement = Cast<UknightMovementComponent>(Character->GetCharacterMovement()))
		{
			// 관성 이어가기: 대시 방향 쪽(CarryMomentumMaxAngle 안쪽)으로 방향키를 누르고 있으면 잠그지 않는다.
			// 대시가 끝날 때 속도는 걷기·달리기 최고 속도로 잘려 있어서, 잠그지 않으면 그 속도 그대로 이어서 움직인다.
			const FVector HeldInput = KnightMovement->GetLastInputVector().GetSafeNormal2D();
			const bool bCarryMomentum = CarryMomentumMaxAngle > 0.f && !HeldInput.IsNearlyZero()
				&& FVector::DotProduct(HeldInput, DashDirection) >= FMath::Cos(FMath::DegreesToRadians(CarryMomentumMaxAngle));
			if (!bCarryMomentum)
			{
				KnightMovement->SetMoveInputLocked(true);
				bMoveLockApplied = true;
			}
		}
	}

	// 경직 동안 대시 재발동 요청이 오면 CanActivateAbility가 허용하고, 엔진이 이 대시를 끝낸 뒤 새로 시작한다.
	UAbilityTask_WaitDelay* const Task = UAbilityTask_WaitDelay::WaitDelay(this, RecoveryDuration);
	Task->OnFinish.AddDynamic(this, &ThisClass::OnRecoveryFinished);
	Task->ReadyForActivation();
}

void UknightGameplayAbility_BoostDash::OnRecoveryFinished()
{
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);

	// (2026-10-29 사용자 결정) 다 일어나는 순간 방향키를 누르고 있으면 가속 구간 없이 바로 그 방향으로 걷기·달리기 속도로 출발한다.
	// EndAbility가 이동 잠금을 푼 뒤라 GetMaxSpeed가 달리기 여부까지 반영한다.
	// 구르기 몽타주는 자동 블렌드 아웃을 꺼서(bEnableAutoBlendOut) 끝까지 일어선 자세를 유지하다가, 여기서 이동과 함께
	// BlendOut(0.2초)으로 빠진다. 켜 두면 끝나기 직전에 대기 자세가 먼저 섞여 '일어남 → 잠깐 섬 → 출발'로 끊겨 보인다.
	UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (Movement && Movement->IsMovingOnGround())
	{
		const FVector HeldInput = Movement->GetLastInputVector().GetSafeNormal2D();
		if (!HeldInput.IsNearlyZero())
		{
			Movement->Velocity = HeldInput * Movement->GetMaxSpeed();
		}
	}
}

void UknightGameplayAbility_BoostDash::CheckStandUpMove()
{
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
	// 이동 잠금은 가속만 막고 입력 방향은 남기므로, 경직 중에도 방향키를 읽을 수 있다.
	if (bInRecovery && Movement && !Movement->GetLastInputVector().IsNearlyZero())
	{
		OnRecoveryFinished();
	}
}

void UknightGameplayAbility_BoostDash::ReleaseRecoveryState()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StandUpMoveTimer);
	}
	bInRecovery = false;
	if (bCancelWindowApplied)
	{
		bCancelWindowApplied = false;
		if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveLooseGameplayTag(knightGameplayTags::Window_Cancel);
		}
	}
	if (bMoveLockApplied)
	{
		bMoveLockApplied = false;
		const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
		if (UknightMovementComponent* const KnightMovement = Character ? Cast<UknightMovementComponent>(Character->GetCharacterMovement()) : nullptr)
		{
			KnightMovement->SetMoveInputLocked(false);
		}
	}
	// (2026-10-29) 락온 중 구르기에 건 회전 고정용 이동 잠금. 풀면 락온 회전이 다시 몸을 대상 쪽으로 돌린다.
	if (bFacingLockApplied)
	{
		bFacingLockApplied = false;
		const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
		if (UknightMovementComponent* const KnightMovement = Character ? Cast<UknightMovementComponent>(Character->GetCharacterMovement()) : nullptr)
		{
			KnightMovement->SetMoveInputLocked(false);
		}
	}
	PlayingDashMontage.Reset();
}

void UknightGameplayAbility_BoostDash::StartCooldown()
{
	if (!bCooldownPending)
	{
		return;
	}
	bCooldownPending = false;
	const float Cooldown = GetMechAttributeValue(UknightAttributeSet::GetDodgeCooldownAttribute(), 0.f);
	if (const UWorld* const World = GetWorld(); World && Cooldown > 0.f)
	{
		CooldownEndTime = World->GetTimeSeconds() + Cooldown;
	}
}

void UknightGameplayAbility_BoostDash::StartBackThrusters(const ACharacter& Character)
{
	StopBackThrusters();
	USkeletalMeshComponent* const Mesh = Character.GetMesh();
	UWorld* const World = GetWorld();
	if (!BackThrusterEffect || !Mesh || !World)
	{
		UE_LOG(LogknightAbility, Warning, TEXT("BoostDash back booster: skipped (Effect=%s Mesh=%s World=%s)"),
			*GetNameSafe(BackThrusterEffect), *GetNameSafe(Mesh), *GetNameSafe(World));
		return;
	}
	for (int32 Index = 0; Index < BackThrusterOffsets.Num(); ++Index)
	{
		// 어디에도 붙이지 않고 월드에 만든 뒤, 위치·회전을 매 프레임 직접 정한다.
		UNiagaraComponent* const Jet = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, BackThrusterEffect, Character.GetActorLocation(),
			Character.GetActorRotation(), FVector(BackThrusterScale), true, true, ENCPoolMethod::None);
		if (Jet)
		{
			BackThrusters.Add(Jet);
		}
	}
	UpdateBackThrusters();
	if (bDebugBackThrusters)
	{
		UE_LOG(LogknightAbility, Log, TEXT("BoostDash back booster: spawned %d / %d jets, effect %s, bone %s exists=%d"),
			BackThrusters.Num(), BackThrusterOffsets.Num(), *GetNameSafe(BackThrusterEffect), *BackThrusterBone.ToString(),
			Mesh->DoesSocketExist(BackThrusterBone) ? 1 : 0);
		for (const UNiagaraComponent* const Jet : BackThrusters)
		{
			UE_LOG(LogknightAbility, Log, TEXT("  jet %s at %s rot %s active=%d visible=%d"), *GetNameSafe(Jet),
				*Jet->GetComponentLocation().ToCompactString(), *Jet->GetComponentRotation().ToCompactString(), Jet->IsActive() ? 1 : 0, Jet->IsVisible() ? 1 : 0);
		}
	}
	World->GetTimerManager().SetTimer(BackThrusterTimer, this, &ThisClass::UpdateBackThrusters, 1.f / 60.f, true);
}

void UknightGameplayAbility_BoostDash::UpdateBackThrusters()
{
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const USkeletalMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh || BackThrusters.IsEmpty())
	{
		return;
	}
	const FVector Anchor = Mesh->DoesSocketExist(BackThrusterBone) ? Mesh->GetSocketLocation(BackThrusterBone) : Character->GetActorLocation();
	const FQuat ActorQuat = Character->GetActorQuat();
	// 대시 반대 방향으로, 조금 아래로 뿜는다. 이펙트는 로컬 -Z로 뿜므로 로컬 Z를 뿜는 방향의 반대로 둔다.
	const FVector Back = DashDirection.IsNearlyZero() ? -Character->GetActorForwardVector() : -DashDirection;
	const float Down = FMath::DegreesToRadians(BackThrusterDownAngle);
	const FVector Emit = (Back.GetSafeNormal2D() * FMath::Cos(Down) - FVector::UpVector * FMath::Sin(Down)).GetSafeNormal();
	const FRotator Rotation = FRotationMatrix::MakeFromZ(-Emit).Rotator();
	for (int32 Index = 0; Index < BackThrusters.Num(); ++Index)
	{
		if (UNiagaraComponent* const Jet = BackThrusters[Index])
		{
			const FVector Offset = BackThrusterOffsets.IsValidIndex(Index) ? BackThrusterOffsets[Index] : FVector::ZeroVector;
			Jet->SetWorldLocationAndRotation(Anchor + ActorQuat.RotateVector(Offset), Rotation);
			if (bDebugBackThrusters)
			{
				const FVector Location = Jet->GetComponentLocation();
				DrawDebugSphere(Jet->GetWorld(), Location, 6.f, 8, FColor::Yellow, false, 0.f, SDPG_Foreground);
				DrawDebugLine(Jet->GetWorld(), Location, Location + Emit * 80.f, FColor::Red, false, 0.f, SDPG_Foreground, 2.f);
			}
		}
	}
}

void UknightGameplayAbility_BoostDash::StopBackThrusters()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BackThrusterTimer);
		World->GetTimerManager().ClearTimer(BackThrusterStopTimer);
	}
	for (UNiagaraComponent* const Jet : BackThrusters)
	{
		if (Jet)
		{
			// 이미 나온 입자는 끝까지 보이고, 다 사라지면 컴포넌트가 스스로 지워진다.
			Jet->Deactivate();
		}
	}
	BackThrusters.Reset();
}

void UknightGameplayAbility_BoostDash::SpawnGhostStep()
{
	if (!bSpawnGhostTrail || !IsActive())
	{
		return;
	}
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}
	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	const bool bTwoHanded = ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_Stance_TwoHanded);
	const FLinearColor GhostColor = bTwoHanded ? TwoHandedGhostColor : OneHandedGhostColor;
	AknightGhostTrailActor::SpawnGhostFromCharacter(Character, GhostMaterial, GhostColor, GhostLifetime, GhostInitialOpacity);
}

void UknightGameplayAbility_BoostDash::StopGhostTrail()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GhostTrailTimer);
	}
}

void UknightGameplayAbility_BoostDash::OnInvulnerabilityEnded()
{
	RemoveInvulnerability();
}

void UknightGameplayAbility_BoostDash::RemoveInvulnerability()
{
	if (bInvulnerableApplied)
	{
		bInvulnerableApplied = false;
		if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveLooseGameplayTag(knightGameplayTags::State_Invulnerable);
		}
	}
}

void UknightGameplayAbility_BoostDash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	RemoveInvulnerability();
	ReleaseRecoveryState();
	StopBackThrusters();
	StopGhostTrail();

	// 이동이 끝나기 전에 끊긴 대시(피격 등)도 쿨타임은 건다. 비용 부족 등으로 시작도 못 했으면 걸지 않는다.
	StartCooldown();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
