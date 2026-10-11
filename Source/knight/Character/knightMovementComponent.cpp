#include "Character/knightMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "Camera/CameraShakeBase.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "System/knightGameplayTags.h"

namespace
{
	// 달리기 중. 달리기 공격 조건으로 쓴다(태그는 Config/Tags에 이미 있음).
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Sprinting, "State.Sprinting");
}

UknightMovementComponent::UknightMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 지상: 무겁게 출발·정지하고, 천천히 돈다.
	// 실제 지상 최대 속도는 GetMaxSpeed에서 BaseWalkSpeed/BaseRunSpeed와 스탯으로 계산한다.
	MaxWalkSpeed = BaseWalkSpeed;
	MinAnalogWalkSpeed = 20.f;
	MaxAcceleration = 1200.f;
	BrakingDecelerationWalking = 1400.f;
	GroundFriction = 6.f;
	RotationRate = FRotator(0.f, 360.f, 0.f);
	bOrientRotationToMovement = true;

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

float UknightMovementComponent::GetWeightSpeedFactor() const
{
	const float Weight = GetMechAttributeValue(UknightAttributeSet::GetWeightAttribute(), 0.f);
	const float LoadCapacity = GetMechAttributeValue(UknightAttributeSet::GetLoadCapacityAttribute(), 1.f);

	// 적재 한도를 넘는 장착은 막을 예정이지만, 혹시 넘더라도 계수가 바닥 아래로 내려가지 않게 0~1로 자른다.
	const float LoadRatio = LoadCapacity > 0.f ? FMath::Clamp(Weight / LoadCapacity, 0.f, 1.f) : 0.f;
	if (WeightSpeedCurve)
	{
		return FMath::Max(0.f, WeightSpeedCurve->GetFloatValue(LoadRatio));
	}
	return FMath::Lerp(1.f, MinWeightSpeedFactor, LoadRatio);
}

float UknightMovementComponent::GetMaxSpeed() const
{
	// 지상 이동과 낙하(점프 포함)는 스탯을 따른다. 엔진도 낙하 중 수평 속도에 걷기 속도를 쓰므로,
	// 여기서 빼면 공중에 있는 동안 파츠·중량 효과가 사라진다. 비행 등 나머지는 엔진 기본 계산을 쓴다.
	if (MovementMode != MOVE_Walking && MovementMode != MOVE_NavWalking && MovementMode != MOVE_Falling)
	{
		return Super::GetMaxSpeed();
	}

	const float BaseSpeed = bWantsToRun ? BaseRunSpeed : BaseWalkSpeed;
	const float SpeedScale = bWantsToRun
		? GetMechAttributeValue(UknightAttributeSet::GetRunSpeedAttribute(), 1.f)
		: GetMechAttributeValue(UknightAttributeSet::GetWalkSpeedAttribute(), 1.f);
	return BaseSpeed * SpeedScale * GetWeightSpeedFactor();
}

float UknightMovementComponent::GetMaxAcceleration() const
{
	// 잠금 중에는 입력이 있어도 가속이 0 → 엔진이 제동을 걸어 미끄러지며 멈춘다. 이동 방향으로 몸도 돌지 않는다.
	return (bMoveInputLocked || bLandingRecovery) ? 0.f : Super::GetMaxAcceleration();
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

	// 세게 떨어지면 무게를 받아내느라 잠깐 출발하지 못한다. 퀵스텝 잠금(bMoveInputLocked)과는 따로 둔다.
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
	if (!bWantsToRun || !IsMovingOnGround())
	{
		return false;
	}
	// 걷기 최대 속도(스탯·중량 반영)보다 빠를 때만. 제자리에서 Shift만 눌렀거나 달리기 토글이 켜져 있어도 멈춰 있으면 일반 공격이 나간다.
	const float WalkMax = BaseWalkSpeed * GetMechAttributeValue(UknightAttributeSet::GetWalkSpeedAttribute(), 1.f) * GetWeightSpeedFactor();
	return Velocity.SizeSquared2D() > FMath::Square(WalkMax * SprintTagSpeedRatio);
}

void UknightMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);

	const bool bSprint = ShouldHaveSprintTag();
	if (bSprint != bSprintTagged)
	{
		if (UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
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
