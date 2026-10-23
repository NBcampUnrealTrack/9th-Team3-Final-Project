#include "Character/knightMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "Camera/CameraShakeBase.h"
#include "Combat/knightTargetLockComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "System/knightGameplayTags.h"

namespace
{
	// 달리기 중. 달리기 공격 조건으로 쓴다(태그는 Config/Tags에 이미 있음).
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Sprinting, "State.Sprinting");
	// 콘텐츠 태그 (Config/DefaultGameplayTags.ini). 칼을 든 전투 상태.
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Combat_Movement, "State.Combat");

	// 락온 중 뒤로 이동(백스텝)할 때는 Shift를 눌러도 달리지 않고 걷는다.
	bool IsMovingBackwardWhileLockedOn(const UCharacterMovementComponent& Movement)
	{
		const ACharacter* const Character = Movement.GetCharacterOwner();
		const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character);
		if (!Character || !ASC || !ASC->HasMatchingGameplayTag(knightGameplayTags::State_LockedOn))
		{
			return false;
		}
		const FVector Forward = Character->GetActorForwardVector().GetSafeNormal2D();
		const FVector InputDir = Movement.GetCurrentAcceleration().GetSafeNormal2D();
		const FVector MoveDir = !InputDir.IsNearlyZero() ? InputDir : Movement.Velocity.GetSafeNormal2D();
		return !MoveDir.IsNearlyZero() && FVector::DotProduct(MoveDir, Forward) < -0.1f;
	}
}

UknightMovementComponent::UknightMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 지상: 무겁게 출발·정지하고, 천천히 돈다.
	// 실제 지상 최대 속도는 GetMaxSpeed에서 BaseWalkSpeed/BaseRunSpeed와 스탯으로 계산한다.
	// 출발 가속은 GetMaxAcceleration에서 출발 곡선을, 방향키를 뗐을 때의 정지는 ApplyVelocityBraking에서 Stop 값을 쓴다.
	// BrakingDecelerationWalking·GroundFriction은 이동 잠금(가드·퀵스텝 경직) 중 정지와 방향 전환에 쓰인다.
	MaxWalkSpeed = BaseWalkSpeed;
	MinAnalogWalkSpeed = 20.f;
	// (2026-10-14) 1200 → 1100: 속도 -10%와 함께 최고 속도까지 걸리는 시간을 원래보다 약 20% 늘림(시간 ∝ 최고 속도 / 가속). 바뀔 수 있는 값.
	// 처음엔 900이었는데, 같은 날 출발 곡선도 더 느리게 바뀌어(10-13, 아래) 둘이 겹쳐 +50%가 돼서 1100으로 맞췄다(사용자 결정).
	MaxAcceleration = 1100.f;
	BrakingDecelerationWalking = 1400.f;
	GroundFriction = 6.f;
	RotationRate = FRotator(0.f, 360.f, 0.f);
	bOrientRotationToMovement = true;

	// 출발 곡선 기본값: 느리게 떼고, 중간에 붙었다가, 최고 속도 근처에서 부드럽게 닿는다 (헤더 설명 참고).
	FRichCurve* const StartCurve = StartAccelerationCurve.GetRichCurve();
	// (2026-10-13) 로봇 느낌: 서 있다가 출발할 때 처음을 더 느리게(0.35 → 0.15, 0.2에서 0.45). 모터가 돌기 시작하는 느낌.
	StartCurve->AddKey(0.f, 0.15f);
	StartCurve->AddKey(0.2f, 0.45f);
	StartCurve->AddKey(0.5f, 1.f);
	StartCurve->AddKey(1.f, 0.6f);

	// 공중: 빨리 떨어지고, 방향 조절이 거의 안 된다.
	// 실제 점프 힘은 DoJump에서 BaseJumpZVelocity와 스탯으로 계산한다.
	GravityScale = 1.6f;
	JumpZVelocity = BaseJumpZVelocity;
	AirControl = 0.2f;
	BrakingDecelerationFalling = 800.f;

	// 부스트 비행.
	MaxFlySpeed = 1200.f;
	BrakingDecelerationFlying = 900.f;
}

bool UknightMovementComponent::StartFlight()
{
	if (!UpdatedComponent || IsInFlight())
	{
		return false;
	}
	SetMovementMode(MOVE_Flying);
	return IsInFlight();
}

void UknightMovementComponent::StopFlight()
{
	if (IsInFlight())
	{
		SetMovementMode(MOVE_Falling);
	}
}

void UknightMovementComponent::AddFlightVerticalInput(float Scale)
{
	if (IsInFlight() && PawnOwner && !FMath::IsNearlyZero(Scale))
	{
		PawnOwner->AddMovementInput(FVector::UpVector, Scale);
	}
}

float UknightMovementComponent::GetMechAttributeValue(const FGameplayAttribute& Attribute, float Fallback) const
{
	const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	return ASC && ASC->HasAttributeSetForAttribute(Attribute) ? ASC->GetNumericAttribute(Attribute) : Fallback;
}

float UknightMovementComponent::GetLoadRatio() const
{
	const float Weight = GetMechAttributeValue(UknightAttributeSet::GetWeightAttribute(), 0.f);
	const float LoadCapacity = GetMechAttributeValue(UknightAttributeSet::GetLoadCapacityAttribute(), 1.f);

	// 적재 한도를 넘는 장착은 막을 예정이지만, 혹시 넘더라도 계수가 바닥 아래로 내려가지 않게 0~1로 자른다.
	return LoadCapacity > 0.f ? FMath::Clamp(Weight / LoadCapacity, 0.f, 1.f) : 0.f;
}

float UknightMovementComponent::GetWeightSpeedFactor() const
{
	const float LoadRatio = GetLoadRatio();
	if (WeightSpeedCurve)
	{
		return FMath::Max(0.f, WeightSpeedCurve->GetFloatValue(LoadRatio));
	}
	return FMath::Lerp(1.f, MinWeightSpeedFactor, LoadRatio);
}

float UknightMovementComponent::GetDodgeWeightFactor() const
{
	// 시작 적재율까지는 0, 적재율 1에서 1이 되는 비율로 바꿔서 1.0 → MinDodgeWeightFactor 직선에 넣는다.
	const float Alpha = FMath::GetRangePct(DodgeWeightStartRatio, 1.f, GetLoadRatio());
	return FMath::Lerp(1.f, MinDodgeWeightFactor, FMath::Clamp(Alpha, 0.f, 1.f));
}

float UknightMovementComponent::GetMaxSpeed() const
{
	// 지상 이동과 낙하(점프 포함)는 스탯을 따른다. 엔진도 낙하 중 수평 속도에 걷기 속도를 쓰므로,
	// 여기서 빼면 공중에 있는 동안 파츠·중량 효과가 사라진다. 비행 등 나머지는 엔진 기본 계산을 쓴다.
	if (MovementMode != MOVE_Walking && MovementMode != MOVE_NavWalking && MovementMode != MOVE_Falling)
	{
		return Super::GetMaxSpeed();
	}

	// 달리기 잠금(수리 키트 사용 중) 또는 락온 후진 중에는 달리기 키를 누르고 있어도 걷기 기준.
	const bool bRun = bWantsToRun && RunLockCount == 0 && !IsMovingBackwardWhileLockedOn(*this);
	const float BaseSpeed = bRun ? BaseRunSpeed : BaseWalkSpeed;
	const float SpeedScale = bRun
		? GetMechAttributeValue(UknightAttributeSet::GetRunSpeedAttribute(), 1.f)
		: GetMechAttributeValue(UknightAttributeSet::GetWalkSpeedAttribute(), 1.f);
	return BaseSpeed * SpeedScale * GetWeightSpeedFactor();
}

bool UknightMovementComponent::IsRunning() const
{
	return bWantsToRun && RunLockCount == 0 && MoveInputLockCount == 0 && IsMovingOnGround()
		&& !GetCurrentAcceleration().IsNearlyZero() && !IsMovingBackwardWhileLockedOn(*this);
}

float UknightMovementComponent::GetMaxAcceleration() const
{
	// 잠금 중에는 입력이 있어도 가속이 0 → 엔진이 제동을 걸어 미끄러지며 멈춘다. 이동 방향으로 몸도 돌지 않는다.
	// 착지 경직(bLandingRecovery, 2026-10-12)도 이동 잠금처럼 가속을 막는다.
	if (MoveInputLockCount > 0 || bLandingRecovery)
	{
		return 0.f;
	}

	float MaxAccel = Super::GetMaxAcceleration();
	if (IsMovingOnGround())
	{
		// 출발 Easing: 지금 속도가 최고 속도의 몇 %인지로 곡선 배율을 읽는다. 곡선이 비어 있으면 1(일정한 가속).
		// 배율이 0이면 출발할 수 없으므로 0.05 아래로는 내리지 않는다.
		const float TopSpeed = GetMaxSpeed();
		const float SpeedRatio = TopSpeed > 0.f ? FMath::Clamp(static_cast<float>(Velocity.Size2D()) / TopSpeed, 0.f, 1.f) : 1.f;
		MaxAccel *= FMath::Max(0.05f, StartAccelerationCurve.GetRichCurveConst()->Eval(SpeedRatio, 1.f));
	}
	if (IsMovingOnGround() || IsFalling())
	{
		// 최고 속도(GetMaxSpeed)와 같은 중량 계수를 곱해, 무거워도 최고 속도까지 걸리는 시간은 가벼울 때와 같게 한다.
		MaxAccel *= GetWeightSpeedFactor();
	}
	return MaxAccel;
}

void UknightMovementComponent::ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration)
{
	if (IsMovingOnGround())
	{
		// 정지 Easing: 방향키를 뗐을 때(달리기 → 걷기처럼 최고 속도를 넘었을 때 포함) 부드러운 정지 값을 쓴다.
		// 이동 잠금(가드·퀵스텝 경직) 중에는 엔진이 넘겨준 값(GroundFriction·BrakingDecelerationWalking)으로 빨리 멈춘다.
		if (MoveInputLockCount == 0 && !bLandingRecovery)
		{
			Friction = StopBrakingFriction;
			BrakingDeceleration = StopBrakingDeceleration;
		}
		// 일정한 감속에만 중량 계수를 곱한다. 마찰은 속도에 비례해서 이미 같은 비율로 줄어드므로,
		// 무거워도 멈추는 데 걸리는 시간은 가벼울 때와 같다(거리만 짧아짐).
		BrakingDeceleration *= GetWeightSpeedFactor();
	}
	Super::ApplyVelocityBraking(DeltaTime, Friction, BrakingDeceleration);
}

void UknightMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	// Super가 착지 처리를 하면서 수직 속도를 없애므로, 미리 떨어지던 속도를 읽어 둔다.
	const float ImpactSpeed = FMath::Max(0.f, -Velocity.Z);
	Super::ProcessLanded(Hit, remainingTime, Iterations);
	HandleLandingImpact(ImpactSpeed);
}

void UknightMovementComponent::HandleLandingImpact(float ImpactSpeed)
{
	if (ImpactSpeed < LandingShakeMinSpeed || !CharacterOwner)
	{
		return;
	}
	const float MaxSpeed = FMath::Max(LandingShakeMaxSpeed, LandingShakeMinSpeed + 1.f);
	const float Alpha = FMath::Clamp((ImpactSpeed - LandingShakeMinSpeed) / (MaxSpeed - LandingShakeMinSpeed), 0.f, 1.f);

	if (LandingShake)
	{
		if (APlayerController* const PC = Cast<APlayerController>(CharacterOwner->GetController()))
		{
			PC->ClientStartCameraShake(LandingShake, FMath::Lerp(LandingShakeMinScale, LandingShakeMaxScale, Alpha));
		}
	}

	// (2026-10-22) 착지 소리: 발 밑(캡슐 바닥)에서, 세게 떨어질수록 크게.
	if (LandingSound)
	{
		const FVector Feet = CharacterOwner->GetActorLocation() - FVector(0.f, 0.f, CharacterOwner->GetSimpleCollisionHalfHeight());
		UGameplayStatics::PlaySoundAtLocation(CharacterOwner, LandingSound, Feet, FMath::Lerp(LandingSoundMinVolume, LandingSoundMaxVolume, Alpha));
	}

	// 세게 떨어지면 무게를 받아내느라 잠깐 출발하지 못한다. 퀵스텝·가드 잠금(MoveInputLockCount)과는 따로 둔다.
	UWorld* const World = GetWorld();
	if (World && LandingRecoveryMinSpeed > 0.f && LandingRecoveryMaxTime > 0.f && ImpactSpeed >= LandingRecoveryMinSpeed)
	{
		const float RecoveryAlpha = FMath::Clamp((ImpactSpeed - LandingRecoveryMinSpeed) / FMath::Max(MaxSpeed - LandingRecoveryMinSpeed, 1.f), 0.f, 1.f);
		const float Time = FMath::Max(0.05f, LandingRecoveryMaxTime * RecoveryAlpha);
		bLandingRecovery = true;
		World->GetTimerManager().SetTimer(LandingRecoveryTimer, this, &ThisClass::EndLandingRecovery, Time, false);
	}
	UE_LOG(LogTemp, Verbose, TEXT("Knight landing impact %.0f cm/s"), ImpactSpeed);
}

void UknightMovementComponent::EndLandingRecovery()
{
	bLandingRecovery = false;
}

bool UknightMovementComponent::DoJump(bool bReplayingMoves, float DeltaTime)
{
	// 점프 직전에 스탯으로 점프 힘을 정한다. 중량은 점프에 영향을 주지 않는다.
	JumpZVelocity = BaseJumpZVelocity * GetMechAttributeValue(UknightAttributeSet::GetJumpSpeedAttribute(), 1.f);
	return Super::DoJump(bReplayingMoves, DeltaTime);
}

void UknightMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	SyncMovementTags();
}

bool UknightMovementComponent::ShouldHaveSprintTag() const
{
	if (!bWantsToRun || RunLockCount > 0 || !IsMovingOnGround() || IsMovingBackwardWhileLockedOn(*this))
	{
		return false;
	}
	// 걷기 최대 속도(스탯·중량 반영)보다 빠를 때만. 제자리에서 Shift만 누르고 있으면 일반 공격이 나간다.
	const float WalkMax = BaseWalkSpeed * GetMechAttributeValue(UknightAttributeSet::GetWalkSpeedAttribute(), 1.f) * GetWeightSpeedFactor();
	return Velocity.SizeSquared2D() > FMath::Square(WalkMax * SprintTagSpeedRatio);
}

void UknightMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);

	// 락온 중에는 이동 방향(bOrientRotationToMovement)으로 몸을 돌리지 않고, 항상 락온(패링) 대상을 바라보게 고정·회전한다.
	// 비락온 중 차지 공격(State.Charging) 중에도 이동 키 방향으로 몸이 돌지 않고 화면 중앙(카메라 방향)을 바라본다.
	FVector TargetLocation = FVector::ZeroVector;
	const UknightTargetLockComponent* const TargetLock = CharacterOwner ? CharacterOwner->FindComponentByClass<UknightTargetLockComponent>() : nullptr;
	UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	// (2026-10-20) 칼을 든 전투 상태(State.Combat)일 때만 락온 대상을 보며 옆걸음한다. 칼이 등에 있으면 애님BP가 비전투 이동
	// (앞으로만 걷는 블렌드스페이스)이라, 몸을 대상에 고정하면 옆·뒤로 미끄러진다. 그때는 이동 방향을 본다(카메라 락온은 그대로).
	const bool bLockedOn = TargetLock && TargetLock->GetLockedTargetLocation(TargetLocation)
		&& ASC && ASC->HasMatchingGameplayTag(TAG_State_Combat_Movement);
	const bool bChargingUnlocked = !bLockedOn && ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_Charging);
	const bool bGuardingUnlocked = !bLockedOn && ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_Guarding);
	bOrientRotationToMovement = !bLockedOn && !bChargingUnlocked && !bGuardingUnlocked;
	if ((bLockedOn || bChargingUnlocked) && UpdatedComponent && MoveInputLockCount == 0 && !bLandingRecovery && !HasAnimRootMotion())
	{
		const bool bBlocked = ASC && (ASC->HasMatchingGameplayTag(knightGameplayTags::State_Dead)
			|| ASC->HasMatchingGameplayTag(knightGameplayTags::State_HitReact));
		if (!bBlocked)
		{
			constexpr float LockOnTurnRate = 540.f;
			float DesiredYaw = UpdatedComponent->GetComponentRotation().Yaw;
			bool bHasDesiredYaw = false;
			if (bLockedOn)
			{
				const FVector ToTarget = (TargetLocation - CharacterOwner->GetActorLocation()).GetSafeNormal2D();
				if (!ToTarget.IsNearlyZero())
				{
					DesiredYaw = ToTarget.Rotation().Yaw;
					bHasDesiredYaw = true;
				}
			}
			else if (const AController* const Controller = CharacterOwner ? CharacterOwner->GetController() : nullptr)
			{
				DesiredYaw = Controller->GetControlRotation().Yaw;
				bHasDesiredYaw = true;
			}
			if (bHasDesiredYaw)
			{
				const FRotator CurrentRotation = UpdatedComponent->GetComponentRotation();
				FRotator NewRotation = CurrentRotation;
				NewRotation.Yaw = FMath::FixedTurn(CurrentRotation.Yaw, DesiredYaw, LockOnTurnRate * DeltaSeconds);
				NewRotation.Pitch = 0.f;
				NewRotation.Roll = 0.f;
				if (!NewRotation.Equals(CurrentRotation, 1e-3f))
				{
					MoveUpdatedComponent(FVector::ZeroVector, NewRotation, false);
				}
			}
		}
	}

	const bool bSprint = ShouldHaveSprintTag();
	if (bSprint != bSprintTagged)
	{
		if (ASC)
		{
			ASC->SetLooseGameplayTagCount(TAG_State_Sprinting, bSprint ? 1 : 0);
			bSprintTagged = bSprint;
		}
	}
}

void UknightMovementComponent::SyncMovementTags() const
{
	UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	if (!ASC)
	{
		return;
	}
	ASC->SetLooseGameplayTagCount(knightGameplayTags::State_Flying, IsInFlight() ? 1 : 0);
	ASC->SetLooseGameplayTagCount(knightGameplayTags::State_Airborne, (IsInFlight() || IsFalling()) ? 1 : 0);
}
