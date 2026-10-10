#include "Character/knightMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Pawn.h"
#include "System/knightGameplayTags.h"

UknightMovementComponent::UknightMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 지상: 무겁게 출발·정지하고, 천천히 돈다.
	// 실제 지상 최대 속도는 GetMaxSpeed에서 BaseWalkSpeed/BaseRunSpeed와 스탯으로 계산한다.
	// 출발 가속은 GetMaxAcceleration에서 출발 곡선을, 방향키를 뗐을 때의 정지는 ApplyVelocityBraking에서 Stop 값을 쓴다.
	// BrakingDecelerationWalking·GroundFriction은 이동 잠금(가드·퀵스텝 경직) 중 정지와 방향 전환에 쓰인다.
	MaxWalkSpeed = BaseWalkSpeed;
	MinAnalogWalkSpeed = 20.f;
	MaxAcceleration = 1200.f;
	BrakingDecelerationWalking = 1400.f;
	GroundFriction = 6.f;
	RotationRate = FRotator(0.f, 360.f, 0.f);
	bOrientRotationToMovement = true;

	// 출발 곡선 기본값: 느리게 떼고, 중간에 붙었다가, 최고 속도 근처에서 부드럽게 닿는다 (헤더 설명 참고).
	FRichCurve* const StartCurve = StartAccelerationCurve.GetRichCurve();
	StartCurve->AddKey(0.f, 0.35f);
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

	// 달리기 잠금(수리 키트 사용 중) 동안은 달리기 키를 누르고 있어도 걷기 기준.
	const bool bRun = bWantsToRun && RunLockCount == 0;
	const float BaseSpeed = bRun ? BaseRunSpeed : BaseWalkSpeed;
	const float SpeedScale = bRun
		? GetMechAttributeValue(UknightAttributeSet::GetRunSpeedAttribute(), 1.f)
		: GetMechAttributeValue(UknightAttributeSet::GetWalkSpeedAttribute(), 1.f);
	return BaseSpeed * SpeedScale * GetWeightSpeedFactor();
}

float UknightMovementComponent::GetMaxAcceleration() const
{
	// 잠금 중에는 입력이 있어도 가속이 0 → 엔진이 제동을 걸어 미끄러지며 멈춘다. 이동 방향으로 몸도 돌지 않는다.
	if (MoveInputLockCount > 0)
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
		if (MoveInputLockCount == 0)
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
