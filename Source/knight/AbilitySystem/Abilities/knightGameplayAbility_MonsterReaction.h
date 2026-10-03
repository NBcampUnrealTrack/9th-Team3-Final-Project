#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_MonsterReaction.generated.h"

class UAnimMontage;

/**
 * A timed monster reaction that interrupts attacks: hold (down, roar, flinch), then recover.
 * One class, several Blueprint subclasses that differ by trigger and timing:
 *   Down          Triggers: Event.PoiseBroken                     StateTag State.HitReact.Knockdown, bRestorePoise
 *   Break flinch  Triggers: Event.PartBroken                      StateTag State.HitReact.Stagger
 *   Enrage roar   Triggers: State.Monster.Enraged (When Tag Is Added)  StateTag State.SuperArmor
 *
 * While it runs, State.Monster.Reacting and StateTag are on the monster and attacks are cancelled and
 * blocked. The StateTree should wait in a reaction state while bIsReacting (Knight Monster Target) is true;
 * this ability only stops the current move, it does not hold the StateTree.
 *
 * Timing: HoldMontage plays at once (give it a looping section for a down loop) and the hold lasts
 * HoldTime; RecoverMontage then plays, or RecoverTime passes when there is none. Both montages are
 * optional, so stand-ins without a skeleton react on timing alone.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_MonsterReaction : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_MonsterReaction(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Loose tag held for the whole reaction. The StateTree and attack blocking read it. */
	UPROPERTY(EditDefaultsOnly, Category="Reaction", meta=(Categories="State"))
	FGameplayTag StateTag;

	UPROPERTY(EditDefaultsOnly, Category="Reaction", meta=(ClampMin=0, Units="s"))
	float HoldTime = 4.f;

	/** Used when RecoverMontage is not set. */
	UPROPERTY(EditDefaultsOnly, Category="Reaction", meta=(ClampMin=0, Units="s"))
	float RecoverTime = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Reaction|Presentation")
	TObjectPtr<UAnimMontage> HoldMontage;

	UPROPERTY(EditDefaultsOnly, Category="Reaction|Presentation")
	TObjectPtr<UAnimMontage> RecoverMontage;

	/** Refill Poise when the reaction ends normally (a down). */
	UPROPERTY(EditDefaultsOnly, Category="Reaction|Poise")
	bool bRestorePoise = false;

	/** MaxPoise grows by this fraction each time, so later downs need more pressure (Monster Hunter style). */
	UPROPERTY(EditDefaultsOnly, Category="Reaction|Poise", meta=(EditCondition="bRestorePoise", ClampMin=0, ClampMax=2))
	float MaxPoiseGrowth = 0.25f;

private:
	UFUNCTION()
	void HandleHoldFinished();

	UFUNCTION()
	void HandleRecoverFinished();

	void RestorePoise() const;

	bool bStateTagApplied = false;
};
