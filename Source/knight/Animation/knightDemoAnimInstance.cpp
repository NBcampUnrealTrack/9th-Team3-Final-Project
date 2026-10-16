#include "Animation/knightDemoAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/knightMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "System/knightGameplayTags.h"

namespace
{
	// 콘텐츠 태그 (Config/DefaultGameplayTags.ini)
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Combat_Demo, "State.Combat");
}

void UknightDemoAnimInstance::SetBoolVar(const FName Name, const bool bValue)
{
	if (const FBoolProperty* const Prop = FindFProperty<FBoolProperty>(GetClass(), Name))
	{
		Prop->SetPropertyValue_InContainer(this, bValue);
	}
}

void UknightDemoAnimInstance::SetFloatVar(const FName Name, const double Value)
{
	if (const FDoubleProperty* const D = FindFProperty<FDoubleProperty>(GetClass(), Name))
	{
		D->SetPropertyValue_InContainer(this, Value);
	}
	else if (const FFloatProperty* const F = FindFProperty<FFloatProperty>(GetClass(), Name))
	{
		F->SetPropertyValue_InContainer(this, static_cast<float>(Value));
	}
}

double UknightDemoAnimInstance::GetFloatVar(const FName Name, const double Fallback) const
{
	if (const FDoubleProperty* const D = FindFProperty<FDoubleProperty>(GetClass(), Name))
	{
		return D->GetPropertyValue_InContainer(this);
	}
	if (const FFloatProperty* const F = FindFProperty<FFloatProperty>(GetClass(), Name))
	{
		return F->GetPropertyValue_InContainer(this);
	}
	return Fallback;
}

void UknightDemoAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const ACharacter* const Character = Cast<ACharacter>(GetOwningActor());
	const UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Character || !Movement)
	{
		return;
	}
	const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character);
	const auto HasTag = [ASC](const FGameplayTag& Tag) { return ASC && Tag.IsValid() && ASC->HasMatchingGameplayTag(Tag); };

	// 왼손이 칼날을 잡는 몽타주(패링 반격 등) 중에는 양손 IK를 끈다 → 한손·양손 스탠스 모두 같은 자세.
	if (bTwoHandedStance)
	{
		for (const UAnimMontage* const Montage : MontagesWithoutTwoHandIK)
		{
			if (Montage && Montage_IsPlaying(Montage))
			{
				bTwoHandedStance = false;
				break;
			}
		}
	}

	// --- 데모 이벤트 그래프의 속도 계산 (중량 계수 보정 및 걷기/달리기 구간 분리) ---
	const FVector Vel = Movement->Velocity;
	const UknightMovementComponent* const KnightMovement = Cast<UknightMovementComponent>(Movement);
	const bool bRunning = KnightMovement ? KnightMovement->IsRunning() : false;
	const float WeightFactor = KnightMovement ? FMath::Max(KnightMovement->GetWeightSpeedFactor(), 0.1f) : 1.f;
	const double UnweightedSpeed2D = Vel.Size2D() / WeightFactor;

	const double SpeedX = FVector::DotProduct(Vel, Character->GetActorForwardVector()) / WeightFactor;
	const double SpeedY = FVector::DotProduct(Vel, Character->GetActorRightVector()) / WeightFactor;
	const bool bAccelerating = !Movement->GetCurrentAcceleration().IsNearlyZero();
	const double TargetBattleSpeed = bRunning ? FMath::Min(UnweightedSpeed2D, 800.0) : FMath::Min(UnweightedSpeed2D, 500.0);
	const double SpeedLength = FMath::FInterpTo(GetFloatVar(TEXT("Speed(Length)")), TargetBattleSpeed, DeltaSeconds, SpeedLengthInterpSpeed);

	SetBoolVar(TEXT("IsInAir"), Movement->IsFalling());
	// 비전투 이동 속도: 공중에서는 0, 평상시 이동은 540(걷기 모션), Shift 달리기는 810(달리기 모션).
	const float TargetRelaxedSpeed = Movement->IsFalling()
		? 0.f
		: static_cast<float>(bRunning ? FMath::Min(UnweightedSpeed2D, 810.0) : FMath::Min(UnweightedSpeed2D, 540.0));
	RelaxedLocomotionSpeed = FMath::FInterpTo(RelaxedLocomotionSpeed, TargetRelaxedSpeed, DeltaSeconds, RelaxedSpeedInterpSpeed);
	SetBoolVar(TEXT("isAccelerating"), bAccelerating);
	SetFloatVar(TEXT("Speed_X"), SpeedX);
	SetFloatVar(TEXT("Speed_Y"), SpeedY);
	SetFloatVar(TEXT("Speed(Length)"), SpeedLength);
	if (bAccelerating)
	{
		const double Degree = FMath::RadiansToDegrees(FMath::Atan2(SpeedY, SpeedX));
		SetFloatVar(TEXT("Speed(Degree)"), Degree);
		if (SpeedLength > LastSpeedDegreeMinSpeed)
		{
			SetFloatVar(TEXT("LastSpeed(Degree)"), Degree);
		}
	}

	// 입력 방향(캐릭터 기준). 데모: InputDirection(Degree) = Atan2(X, Y), 입력이 없으면 180.
	const FVector LocalInput = Character->GetActorTransform().InverseTransformVectorNoScale(Movement->GetLastInputVector());
	const double InputX = LocalInput.Y;
	const double InputY = LocalInput.X;
	SetFloatVar(TEXT("InputDirection_X"), InputX);
	SetFloatVar(TEXT("InputDirection_Y"), InputY);
	SetFloatVar(TEXT("InputDirection(Degree)"), (FMath::IsNearlyZero(InputX) && FMath::IsNearlyZero(InputY))
		? 180.0 : FMath::RadiansToDegrees(FMath::Atan2(InputX, InputY)));

	// --- 상태 ---
	SetBoolVar(TEXT("isRun"), bRunning);
	SetBoolVar(TEXT("isWalk"), false);
	SetBoolVar(TEXT("isOneHanded"), !HasTag(knightGameplayTags::State_Stance_TwoHanded));
	// 락온 중에도 몸이 대상을 바라보며 좌우·후방 스텝(Speed_X/Y/Degree 블렌드 스페이스)으로 이동하도록 전투 이동 자세를 쓴다.
	SetBoolVar(TEXT("isBattleState"), HasTag(TAG_State_Combat_Demo) || HasTag(knightGameplayTags::State_LockedOn));
	SetBoolVar(TEXT("isLocomotion"), !Movement->IsFalling() && !IsAnyMontagePlaying());
	SetFloatVar(TEXT("BattleStanceTimer"), 0.0);

	const bool bGuard = HasTag(knightGameplayTags::State_Guarding) && !bRunning;
	SetFloatVar(TEXT("Shield_Blend"), FMath::FInterpTo(GetFloatVar(TEXT("Shield_Blend")), bGuard ? 0.999 : 0.001, DeltaSeconds, ShieldBlendInterpSpeed));

	// 전투 동작은 GAS 몽타주가 맡는다 → 상태 머신의 전투 상태로는 가지 않는다.
	for (const TCHAR* const Name : { TEXT("isAttack"), TEXT("isAttack_Secondary"), TEXT("isAttackInput"), TEXT("isAttackCombo"),
		TEXT("isDodge"), TEXT("isRoll"), TEXT("isHurt") })
	{
		SetBoolVar(Name, false);
	}
}
