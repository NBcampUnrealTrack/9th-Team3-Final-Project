#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "Combat/knightCombatTypes.h"
#include "knightGameplayAbility_MonsterAreaAttack.generated.h"

class UAnimMontage;

/**
 * A monster attack timed by the ability instead of a montage: wind-up, one sphere overlap in front of the
 * monster (slam, stomp, short breath), recovery.
 *
 * Every hostile ICombatInterface actor inside the sphere is resolved once through
 * UknightCombatLibrary::ApplyMeleeHit, so guard, parry, evasion and damage follow the same rules as weapon
 * hits. Works without a skeleton, which is what cube stand-ins in test maps need; real monsters can use it
 * for area moves, with Montage playing for looks while timing stays here.
 *
 * Debug: knight.Monster.DrawAttacks 1 draws the telegraph (orange) and the impact (red).
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_MonsterAreaAttack : public UknightGameplayAbility_MonsterAttack
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_MonsterAreaAttack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** Runs the overlap and applies hits. Override to add effects at the moment of impact. */
	virtual void OnImpact();

	/** Time from activation to impact; the player's window to read and dodge the attack. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Timing", meta=(ClampMin=0, Units="s"))
	float WindupTime = 0.8f;

	/** Time after impact before the ability ends; the player's punish window. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Timing", meta=(ClampMin=0, Units="s"))
	float RecoveryTime = 1.f;

	/** Sphere center in the monster's local space (X forward). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Area")
	FVector AreaOffset = FVector(350.f, 0.f, 0.f);

	UPROPERTY(EditDefaultsOnly, Category="Attack|Area", meta=(ClampMin=1, Units="cm"))
	float AreaRadius = 300.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FknightMeleeHitParams HitParams;

	/** Optional. Plays for presentation only; it stops when the ability ends. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation")
	TObjectPtr<UAnimMontage> Montage;

	/** Optional cue executed at the area center when the wind-up starts (ground decal, roar). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation", meta=(Categories="GameplayCue"))
	FGameplayTag TelegraphCue;

private:
	UFUNCTION()
	void HandleWindupFinished();

	UFUNCTION()
	void HandleRecoveryFinished();

	FVector GetAreaCenter() const;
};
