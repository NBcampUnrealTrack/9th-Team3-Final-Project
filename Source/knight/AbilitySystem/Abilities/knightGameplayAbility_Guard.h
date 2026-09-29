#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_Guard.generated.h"

class UAnimMontage;

/**
 * Hold to guard. The first ParryWindow seconds after pressing also count as a parry.
 *
 * While active the owner has State.Guarding (UknightDamageExecution reduces frontal hits) and, during the
 * parry window, State.Parrying (UknightCombatLibrary zeroes the hit and staggers the attacker).
 * Releasing the input ends the guard.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_Guard : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_Guard(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Seconds after the press during which an incoming hit is parried. Zero disables parrying. */
	UPROPERTY(EditDefaultsOnly, Category="Guard", meta=(ClampMin=0, Units="s"))
	float ParryWindow = 0.2f;

	/** Optional looping guard pose. The ability ends on input release, not when this montage ends. */
	UPROPERTY(EditDefaultsOnly, Category="Guard")
	TObjectPtr<UAnimMontage> GuardMontage;

private:
	UFUNCTION()
	void OnParryWindowEnded();

	UFUNCTION()
	void OnGuardReleased(float TimeHeld);

	void RemoveParryTag();

	bool bParryTagApplied = false;
};
