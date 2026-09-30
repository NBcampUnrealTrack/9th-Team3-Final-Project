#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_BoostDash.generated.h"

class UAnimMontage;
class UCurveFloat;

/**
 * Boost dash / evasive boost. Moves the Knight with a constant-force root motion source in the current
 * movement input direction (backward with no input), with State.Invulnerable for the first
 * InvulnerableDuration seconds.
 *
 * Gauge cost goes on the ability's Cost Gameplay Effect (a BoostGauge modifier), so a dash with too
 * little gauge fails activation with Ability.ActivateFail.Cost.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_BoostDash : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_BoostDash(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(ClampMin=0, Units="cm"))
	float DashDistance = 900.f;

	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(ClampMin=0.05, Units="s"))
	float DashDuration = 0.35f;

	/** Optional 0..1 curve over the dash; shapes the burst without changing the distance much. */
	UPROPERTY(EditDefaultsOnly, Category="Dash")
	TObjectPtr<UCurveFloat> StrengthOverTime;

	/** Evasion frames from the start of the dash. */
	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(ClampMin=0, Units="s"))
	float InvulnerableDuration = 0.2f;

	/** Optional thruster pose. Root motion in it is ignored; the root motion source drives the move. */
	UPROPERTY(EditDefaultsOnly, Category="Dash")
	TObjectPtr<UAnimMontage> DashMontage;

private:
	UFUNCTION()
	void OnDashFinished();

	UFUNCTION()
	void OnInvulnerabilityEnded();

	void RemoveInvulnerability();

	bool bInvulnerableApplied = false;
};
