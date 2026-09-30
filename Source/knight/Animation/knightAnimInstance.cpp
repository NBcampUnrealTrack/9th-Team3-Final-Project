#include "Animation/knightAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

void UknightAnimInstance::InitializeWithAbilitySystem(UAbilitySystemComponent* ASC)
{
	if (ASC && !bAbilitySystemBound)
	{
		GameplayTagPropertyMap.Initialize(this, ASC);
		bAbilitySystemBound = true;
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

	const ACharacter* const Character = Cast<ACharacter>(GetOwningActor());
	const UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (Movement)
	{
		CachedVelocity = Movement->Velocity;
		bCachedIsFalling = Movement->IsFalling();
		bCachedHasAcceleration = !Movement->GetCurrentAcceleration().IsNearlyZero();
	}
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
