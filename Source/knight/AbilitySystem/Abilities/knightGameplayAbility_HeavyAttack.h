#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "Combat/knightCombatTypes.h"
#include "knightGameplayAbility_HeavyAttack.generated.h"

class UAnimMontage;

/**
 * Secondary attack key (mouse thumb button): tap for the heavy combo, hold to charge.
 *   released before HoldThreshold  -> activates the heavy combo step whose tags fit (HeavyAbilityTag, e.g.
 *                                     Ability.Attack.Heavy on GA_Knight_Heavy_1..4). If no step can start yet (an
 *                                     attack is still swinging), the tap is retried until TapBufferTime runs out,
 *                                     like the input buffer does for the light combo.
 *   held past HoldThreshold        -> charge: ChargeStartMontage, then ChargeLoopMontage repeats while held
 *                                     (its melee trace hits with LoopHitParams). Releasing plays ChargeReleaseMontage,
 *                                     whose hits use ReleaseHitParams scaled up to MaxChargeMultiplier after MaxChargeTime.
 * Charging starts only when no other attack / guard / dash is running, or inside Window.Cancel (it cancels them).
 * While charging the owner has State.Charging and other attacks are blocked.
 *
 * Bind to InputTag.Attack.Secondary. The heavy combo steps are ordinary knightGameplayAbility_MeleeAttack Blueprints
 * with no input tag; chain them with Combo tags exactly like the light combo.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_HeavyAttack : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_HeavyAttack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Holding longer than this (seconds) charges instead of a heavy combo step. */
	UPROPERTY(EditDefaultsOnly, Category="Heavy", meta=(ClampMin=0.05, Units="s"))
	float HoldThreshold = 0.3f;

	/** Tag of the heavy combo step abilities. */
	UPROPERTY(EditDefaultsOnly, Category="Heavy", meta=(Categories="Ability"))
	FGameplayTag HeavyAbilityTag;

	/** How long a tap keeps trying to start the next heavy step while the current attack is still swinging. */
	UPROPERTY(EditDefaultsOnly, Category="Heavy", meta=(ClampMin=0, Units="s"))
	float TapBufferTime = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category="Charge")
	TObjectPtr<UAnimMontage> ChargeStartMontage;

	/** Repeated while the key is held. */
	UPROPERTY(EditDefaultsOnly, Category="Charge")
	TObjectPtr<UAnimMontage> ChargeLoopMontage;

	UPROPERTY(EditDefaultsOnly, Category="Charge")
	TObjectPtr<UAnimMontage> ChargeReleaseMontage;

	/** Hits during the start / loop montages. */
	UPROPERTY(EditDefaultsOnly, Category="Charge")
	FknightMeleeHitParams LoopHitParams;

	/** Hits of the release montage at no charge; motion value and poise damage scale up with the charge time. */
	UPROPERTY(EditDefaultsOnly, Category="Charge")
	FknightMeleeHitParams ReleaseHitParams;

	/** Charge time (seconds, counted from the start of the charge) that reaches MaxChargeMultiplier. */
	UPROPERTY(EditDefaultsOnly, Category="Charge", meta=(ClampMin=0.1, Units="s"))
	float MaxChargeTime = 2.f;

	UPROPERTY(EditDefaultsOnly, Category="Charge", meta=(ClampMin=1))
	float MaxChargeMultiplier = 2.f;

	/** Warp target of the release montage, set when the button is released. */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Motion Warping")
	FName WarpTargetName = TEXT("AttackTarget");

	UPROPERTY(EditDefaultsOnly, Category="Charge|Motion Warping", meta=(ClampMin=0, Units="cm"))
	float MaxWarpDistance = 400.f;

	/** The warp stops this far in front of the target's surface (or part point), so the blade lands instead of the body. */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Motion Warping", meta=(ClampMin=0, Units="cm"))
	float WarpStopDistance = 150.f;

	/** Without a lock, steer into the closest enemy in the attack direction. */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Motion Warping")
	bool bUseSoftTarget = true;

	UPROPERTY(EditDefaultsOnly, Category="Charge|Motion Warping", meta=(ClampMin=0, Units="cm", EditCondition="bUseSoftTarget"))
	float SoftTargetRange = 500.f;

	UPROPERTY(EditDefaultsOnly, Category="Charge|Motion Warping", meta=(ClampMin=1, ClampMax=180, Units="deg", EditCondition="bUseSoftTarget"))
	float SoftTargetAngle = 75.f;

private:
	UFUNCTION()
	void OnReleased(float TimeHeld);

	UFUNCTION()
	void OnHoldThreshold();

	UFUNCTION()
	void OnChargeStartFinished();

	UFUNCTION()
	void OnChargeLoopFinished();

	UFUNCTION()
	void OnReleaseFinished();

	UFUNCTION()
	void OnMeleeHitEvent(FGameplayEventData Payload);

	void TryHeavyStep();
	bool CanStartCharge() const;
	void StartCharge();
	void PlayLoop();
	void PlayMontage(UAnimMontage* Montage, FName TaskName, bool bLoopOnEnd);
	void Finish(bool bWasCancelled);

	enum class EPhase : uint8 { Deciding, TapBuffer, Charging, Releasing };
	EPhase Phase = EPhase::Deciding;
	FknightMeleeHitParams ActiveHitParams;
	double ChargeStartTime = 0.0;
	double TapDeadline = 0.0;
	bool bReleased = false;
	bool bChargeStateApplied = false;
	FTimerHandle TapTimer;
};
