#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "Combat/knightCombatTypes.h"
#include "knightGameplayAbility_MeleeAttack.generated.h"

class UAnimMontage;

/**
 * One melee attack: plays a montage, and turns every Event.Melee.Hit from the weapon trace into a
 * resolved hit through UknightCombatLibrary::ApplyMeleeHit.
 *
 * The montage carries the timing: Knight Melee Trace notifies for the active frames, and Knight Gameplay
 * Tag Window notifies for Window.Combo and Window.Cancel. A combo is a chain of these abilities; the next
 * step activates from the input buffer when the combo window opens and replaces this one.
 *
 * Chaining steps that share one InputTag: give step N an ActivationOwnedTag (for example Combo.Greatsword.1,
 * defined in Config/Tags), make step N+1 require it (ActivationRequiredTags), and make the first step
 * block on the whole Combo.Greatsword family (ActivationBlockedTags). Only the correct next step can then
 * activate for the press.
 *
 * With a lock-on target, the WarpTargetName warp target is set before the montage starts, so root motion
 * with a Motion Warping notify steers into the target.
 *
 * Movement cancels the recovery: while Window.Cancel is open, any movement input stops the montage
 * (RecoveryCancelBlendOut) and ends the attack, so the Knight walks off instead of finishing the follow-through.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_MeleeAttack : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_MeleeAttack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Called after each resolved hit. Override for on-hit effects (gauge gain, combo counters). */
	virtual void OnHitResolved(const FHitResult& Hit, EknightHitOutcome Outcome) {}

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, Category="Attack", meta=(ClampMin=0.01))
	float PlayRate = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FName StartSection = NAME_None;

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FknightMeleeHitParams HitParams;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Motion Warping")
	FName WarpTargetName = TEXT("AttackTarget");

	/** How far the warp may pull the Knight toward the locked target. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Motion Warping", meta=(ClampMin=0, Units="cm"))
	float MaxWarpDistance = 300.f;

	/** Movement input during Window.Cancel stops the montage and ends the attack. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Recovery")
	bool bCancelRecoveryOnMove = true;

	/**
	 * Also cancel while Window.Combo is open. True: movement right after the swing walks off at once, but holding
	 * a direction while mashing attack can break the combo. False: movement only cancels after the combo window.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Recovery", meta=(EditCondition="bCancelRecoveryOnMove"))
	bool bMoveCancelInComboWindow = true;

	/** Blend out used when movement cancels the recovery. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Recovery", meta=(ClampMin=0, Units="s", EditCondition="bCancelRecoveryOnMove"))
	float RecoveryCancelBlendOut = 0.25f;

private:
	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMeleeHitEvent(FGameplayEventData Payload);

	void CheckMoveCancel();

	FTimerHandle MoveCancelTimer;
};
