#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_BoostDash.generated.h"

class ACharacter;
class UAnimMontage;
class UCurveFloat;

/**
 * Boost dash / evasive boost. Moves the Knight with a constant-force root motion source in the current
 * movement input direction (backward with no input), with State.Invulnerable for the first
 * InvulnerableDuration seconds.
 *
 * Gauge cost goes on the ability's Cost Gameplay Effect (a BoostGauge modifier), so a dash with too
 * little gauge fails activation with Ability.ActivateFail.Cost.
 *
 * Facing and pose: without lock-on, a dash with movement input first turns the Knight to the dash direction
 * and plays DashMontage (forward). With lock-on (State.LockedOn), or a backstep with no input, the Knight keeps
 * facing and the montage is picked by the dash direction relative to it (Forward / Back / Left / Right).
 * Unset directional montages fall back to DashMontage.
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

	/** Optional thruster pose (forward). Root motion in it is ignored; the root motion source drives the move. */
	UPROPERTY(EditDefaultsOnly, Category="Dash")
	TObjectPtr<UAnimMontage> DashMontage;

	/** Dash pose when moving backward relative to the facing (lock-on, or a backstep with no input). */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Directional")
	TObjectPtr<UAnimMontage> DashMontageBack;

	UPROPERTY(EditDefaultsOnly, Category="Dash|Directional")
	TObjectPtr<UAnimMontage> DashMontageLeft;

	UPROPERTY(EditDefaultsOnly, Category="Dash|Directional")
	TObjectPtr<UAnimMontage> DashMontageRight;

	/** Without lock-on, turn the Knight to the dash direction at the start so the forward pose matches. */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Directional")
	bool bFaceDashDirectionWhenFree = true;

	/** AI hearing loudness of the dash. */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Noise", meta=(ClampMin=0))
	float NoiseLoudness = 1.f;

	/** Maximum hearing distance of the dash. Zero uses the listener's hearing range. */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Noise", meta=(ClampMin=0, Units="cm"))
	float NoiseRange = 0.f;

private:
	UAnimMontage* SelectDashMontage(const ACharacter& Character, const FVector& Direction) const;

	UFUNCTION()
	void OnDashFinished();

	UFUNCTION()
	void OnInvulnerabilityEnded();

	void RemoveInvulnerability();

	bool bInvulnerableApplied = false;
};
