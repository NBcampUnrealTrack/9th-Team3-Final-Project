#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_Sprint.generated.h"

/**
 * Boost key (LeftShift) handler: hold to run, double tap to boost dash.
 *   press            -> run (UknightMovementComponent::SetWantsToRun) until the key is released
 *   second press within DoubleTapWindow of the previous one
 *                    -> stop running and activate the dash ability (DashAbilityTag, Ability.Boost.Dash);
 *                       the Knight goes back to normal speed afterwards even if the key is still held
 *
 * Bind this ability to InputTag.Boost and give the dash ability no input tag, so the key only reaches this one.
 * The dash still follows its own rules (cost, activation group, cancel windows): a double tap during an attack
 * only dashes inside Window.Cancel.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_Sprint : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_Sprint(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Two presses closer together than this (seconds, press to press) are a double tap. */
	UPROPERTY(EditDefaultsOnly, Category="Sprint", meta=(ClampMin=0.05, ClampMax=1, Units="s"))
	float DoubleTapWindow = 0.25f;

	/** Ability activated by a double tap. */
	UPROPERTY(EditDefaultsOnly, Category="Sprint", meta=(Categories="Ability"))
	FGameplayTag DashAbilityTag;

private:
	UFUNCTION()
	void OnReleased(float TimeHeld);

	void SetRunning(bool bRun) const;

	/** World time of the press that started the last run; reset after a double tap so a third press runs again. */
	double LastPressTime = -1.e9;
};
