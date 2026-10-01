#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayEffectTypes.h"
#include "knightAnimInstance.generated.h"

class UAbilitySystemComponent;

/**
 * Base Anim Instance for the Knight and monsters.
 *
 * GameplayTagPropertyMap keeps Anim Blueprint variables in sync with the owner's ASC tags
 * (for example State.Flying -> bIsFlying, State.Monster.Enraged -> bIsEnraged), so the Anim Blueprint
 * never polls gameplay state itself. Common locomotion values are filled natively on the worker thread.
 */
UCLASS()
class KNIGHT_API UknightAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Binds the tag map to an ASC. Called automatically on init when the owner implements IAbilitySystemInterface. */
	virtual void InitializeWithAbilitySystem(UAbilitySystemComponent* ASC);

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** Tag to Blueprint property bindings. Properties must be bool, int32, float or double. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|GameplayTags")
	FGameplayTagBlueprintPropertyMap GameplayTagPropertyMap;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Locomotion")
	float GroundSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Locomotion")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Locomotion")
	bool bIsFalling = false;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Locomotion")
	bool bHasAcceleration = false;

private:
	// Game thread copies read by the thread-safe update.
	FVector CachedVelocity = FVector::ZeroVector;
	bool bCachedIsFalling = false;
	bool bCachedHasAcceleration = false;
	bool bAbilitySystemBound = false;
};
