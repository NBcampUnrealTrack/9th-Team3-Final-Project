#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "GameplayTagContainer.h"
#include "knightGameplayAbility_CombatStance.generated.h"

class UAnimMontage;

/**
 * Passive (OnSpawn) ability that keeps the Knight's weapon stance.
 *
 * While any CombatTriggerTags is on the owner (attack combo tags, hit reaction, guard), the Knight is in
 * combat: CombatStateTag is added, so the Anim Blueprint switches to the weapon-drawn locomotion through its
 * GameplayTagPropertyMap. Once the last trigger tag goes away the stance winds down in two phases:
 *   1. Alert (AlertDuration seconds): CombatAlertTag stays on as well, the Anim Blueprint keeps the combat
 *      ready pose.
 *   2. Hold (SheatheDelay seconds): only CombatStateTag, the Knight stands relaxed with the sword in hand.
 * Then SheatheMontage plays (its Knight Attach Weapon notify moves the sword to the back) and the tag is removed.
 * Any trigger tag during either phase restarts from the alert phase.
 * When the Knight is moving faster than MaxSpeedForSheatheMontage, the sword is put away without the montage.
 *
 * Drawing is done by the attack montages themselves (Knight Attach Weapon on weapon_r at their start).
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_CombatStance : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_CombatStance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Any of these on the owner keeps the Knight in combat (parent tags match their children). */
	UPROPERTY(EditDefaultsOnly, Category="Combat Stance")
	FGameplayTagContainer CombatTriggerTags;

	/** Loose tag added while in combat. Map it to a bool in the Anim Blueprint's GameplayTagPropertyMap. */
	UPROPERTY(EditDefaultsOnly, Category="Combat Stance")
	FGameplayTag CombatStateTag;

	/** Loose tag kept during the alert phase right after combat. Map it to a bool in the Anim Blueprint. */
	UPROPERTY(EditDefaultsOnly, Category="Combat Stance")
	FGameplayTag CombatAlertTag;

	/** Seconds the combat ready pose (CombatAlertTag) is kept after the last trigger tag goes away. */
	UPROPERTY(EditDefaultsOnly, Category="Combat Stance", meta=(ClampMin=0, Units="s"))
	float AlertDuration = 2.5f;

	/** Seconds standing with the sword in hand after the alert phase, before the sword is put away. */
	UPROPERTY(EditDefaultsOnly, Category="Combat Stance", meta=(ClampMin=0, Units="s"))
	float SheatheDelay = 5.f;

	/** Should carry a Knight Attach Weapon notify (BackSocket) where the hand releases the hilt. */
	UPROPERTY(EditDefaultsOnly, Category="Combat Stance")
	TObjectPtr<UAnimMontage> SheatheMontage;

	/** Above this speed the sword is put away instantly instead of playing the full-body montage. */
	UPROPERTY(EditDefaultsOnly, Category="Combat Stance", meta=(ClampMin=0, Units="cm/s"))
	float MaxSpeedForSheatheMontage = 30.f;

	UPROPERTY(EditDefaultsOnly, Category="Combat Stance")
	FName BackSocket = TEXT("weapon_back");

	UPROPERTY(EditDefaultsOnly, Category="Combat Stance")
	FName WeaponComponentTag = TEXT("Weapon");

private:
	void OnTriggerTagChanged(const FGameplayTag Tag, int32 NewCount);
	void RefreshCombatState();
	void StartSheathe();
	void EndAlert();
	void SetAlert(bool bNewAlert);
	void ExitCombat();
	void AttachWeaponToBack() const;
	bool HasAnyTriggerTag() const;

	UFUNCTION()
	void OnSheatheMontageFinished();

	UFUNCTION()
	void OnSheatheMontageInterrupted();

	TArray<TPair<FGameplayTag, FDelegateHandle>> TagEventHandles;
	FTimerHandle SheatheTimer;
	FTimerHandle AlertTimer;
	bool bInCombat = false;
	bool bAlert = false;
	bool bSheathing = false;
};
