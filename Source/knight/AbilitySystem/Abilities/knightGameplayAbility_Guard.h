#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_Guard.generated.h"

class UAnimMontage;
class UGameplayEffect;

/**
 * Hold to guard. The first ParryWindow seconds after pressing also count as a parry.
 *
 * While active the owner has State.Guarding (UknightDamageExecution reduces frontal hits) and, during the
 * parry window, State.Parrying (UknightCombatLibrary zeroes the hit and staggers the attacker).
 * Releasing the input ends the guard.
 *
 * Parry counter: when a parry lands, UknightCombatLibrary sends ParrySucceededEventTag (Event.ParrySucceeded) to
 * the defender. The guard then gives the owner ParryCounterTag (State.ParryCounter, which the counter attack
 * requires) for ParryCounterWindow seconds, applies the optional ParryCounterEffect, and ends, so the attack key
 * can start the counter.
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

	/** Event sent to the defender when a parry lands. Empty disables the counter. */
	UPROPERTY(EditDefaultsOnly, Category="Guard|Parry Counter", meta=(Categories="Event"))
	FGameplayTag ParrySucceededEventTag;

	/** Loose tag given to the owner for ParryCounterWindow seconds after a successful parry. */
	UPROPERTY(EditDefaultsOnly, Category="Guard|Parry Counter", meta=(Categories="State"))
	FGameplayTag ParryCounterTag;

	UPROPERTY(EditDefaultsOnly, Category="Guard|Parry Counter", meta=(ClampMin=0, Units="s"))
	float ParryCounterWindow = 0.8f;

	/** Optional extra effect applied to self on a successful parry (gauge refill, buff). */
	UPROPERTY(EditDefaultsOnly, Category="Guard|Parry Counter")
	TSubclassOf<UGameplayEffect> ParryCounterEffect;

private:
	UFUNCTION()
	void OnParrySucceeded(FGameplayEventData Payload);

	UFUNCTION()
	void OnParryWindowEnded();

	UFUNCTION()
	void OnGuardReleased(float TimeHeld);

	void RemoveParryTag();
	void WaitForRelease();

	bool bParryTagApplied = false;
};
