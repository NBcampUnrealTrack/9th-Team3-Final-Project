#include "Animation/knightAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "System/knightGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

void UknightAnimInstance::InitializeWithAbilitySystem(UAbilitySystemComponent* ASC)
{
	if (ASC && !bAbilitySystemBound)
	{
		GameplayTagPropertyMap.Initialize(this, ASC);
		bAbilitySystemBound = true;
		BoundASC = ASC;
	}
}

void UknightAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	if (UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwningActor()))
	{
		InitializeWithAbilitySystem(ASC);
	}
}

void UknightAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// The ASC can be created after the anim instance initializes (spawned actors); bind on first sight.
	if (!bAbilitySystemBound)
	{
		InitializeWithAbilitySystem(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwningActor()));
	}

	bTwoHandedStance = BoundASC.IsValid() && BoundASC->HasMatchingGameplayTag(knightGameplayTags::State_Stance_TwoHanded);

	const ACharacter* const Character = Cast<ACharacter>(GetOwningActor());
	const UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (Movement)
	{
		CachedVelocity = Movement->Velocity;
		bCachedIsFalling = Movement->IsFalling();
		bCachedHasAcceleration = !Movement->GetCurrentAcceleration().IsNearlyZero();
	}

	// 공중 하체 고정: 검 뽑기 같은 몽타주는 땅 위 다리를 써서 고정하고, 공격 몽타주는 다리까지 따르게 한다.
	float LegLockTarget = 0.f;
	if (bCachedIsFalling)
	{
		UAnimMontage* const Montage = GetCurrentActiveMontage();
		LegLockTarget = (!Montage || AirLegLockMontages.Contains(Montage)) ? 1.f : 0.f;
	}
	AirLegLockAlpha = AirLegLockBlendTime > 0.f
		? FMath::FInterpConstantTo(AirLegLockAlpha, LegLockTarget, DeltaSeconds, 1.f / AirLegLockBlendTime)
		: LegLockTarget;
}

void UknightAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);
	Velocity = CachedVelocity;
	GroundSpeed = CachedVelocity.Size2D();
	bIsFalling = bCachedIsFalling;
	bHasAcceleration = bCachedHasAcceleration;
}

#if WITH_EDITOR
EDataValidationResult UknightAnimInstance::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);
	GameplayTagPropertyMap.IsDataValid(this, Context);
	return Context.GetNumErrors() > 0 ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif
