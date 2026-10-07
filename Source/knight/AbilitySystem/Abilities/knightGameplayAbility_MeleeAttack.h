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
 * The WarpTargetName warp target is set before the montage starts (the locked target, or without a lock the
 * closest enemy in the attack direction), so root motion inside a Motion Warping notify window steers into it.
 *
 * Drawing: with DrawMontage set and the weapon not in the hand (WeaponHandSocket), DrawMontage plays instead (its
 * Knight Attach Weapon notify moves the sword to the hand). With bAttackAfterDraw the attack follows at
 * DrawAttackStartTime; otherwise the press only draws, and moving once the sword is in hand ends the draw early.
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

	/** Played first when the weapon is not in the hand (sword on the back). Leave empty to draw instantly. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Draw")
	TObjectPtr<UAnimMontage> DrawMontage;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Draw", meta=(ClampMin=0.01))
	float DrawPlayRate = 1.f;

	/** False: the first press with the sword on the back only draws it. True: the attack follows the draw. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Draw")
	bool bAttackAfterDraw = false;

	/** Montage time (s) at which the attack takes over from the draw, so the settle at its end is skipped. 0: when it blends out. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Draw", meta=(ClampMin=0, Units="s"))
	float DrawAttackStartTime = 0.f;

	/** The weapon counts as drawn while it is attached to this socket. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Draw")
	FName WeaponHandSocket = TEXT("weapon_r");

	UPROPERTY(EditDefaultsOnly, Category="Attack|Draw")
	FName WeaponComponentTag = TEXT("Weapon");

	UPROPERTY(EditDefaultsOnly, Category="Attack|Motion Warping")
	FName WarpTargetName = TEXT("AttackTarget");

	/** How far the warp may pull the Knight toward the locked target. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Motion Warping", meta=(ClampMin=0, Units="cm"))
	float MaxWarpDistance = 300.f;

	/** The lunge toward the warp location takes at most this long and ends with the warp window (the first trace). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Motion Warping", meta=(ClampMin=0.05, Units="s"))
	float MaxLungeTime = 0.35f;

	/** The warp stops this far in front of the target's surface (or part point), so the blade lands instead of the body. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Motion Warping", meta=(ClampMin=0, Units="cm"))
	float WarpStopDistance = 150.f;

	/** Without a lock, steer into the closest enemy in the attack direction. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Motion Warping")
	bool bUseSoftTarget = true;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Motion Warping", meta=(ClampMin=0, Units="cm", EditCondition="bUseSoftTarget"))
	float SoftTargetRange = 500.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Motion Warping", meta=(ClampMin=1, ClampMax=180, Units="deg", EditCondition="bUseSoftTarget"))
	float SoftTargetAngle = 75.f;

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

	UFUNCTION()
	void OnDrawFinished();

	UFUNCTION()
	void OnDrawInterrupted();

	void StartAttack();
	void CheckDrawMoveCancel();
	bool IsWeaponInHand() const;
	void CheckMoveCancel();

	FTimerHandle MoveCancelTimer;
	FTimerHandle DrawTimer;
	bool bAttackStarted = false;
};
