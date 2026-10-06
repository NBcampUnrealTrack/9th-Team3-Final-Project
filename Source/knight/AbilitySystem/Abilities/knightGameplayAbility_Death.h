#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_Death.generated.h"

class UAnimMontage;

/**
 * Death presentation. AknightCharacter::HandleOutOfHealth adds State.Dead, sends Event.Death (this ability's
 * trigger) and then cancels every ability except Ability.Death, so this is the last thing that runs.
 * The raid result is reported by the character through the GameMode (HandlePlayerDeath -> NotifyPlayerDefeated);
 * this ability only plays the animation.
 *
 * The montage should have auto blend out disabled so the final pose holds. The ability stays active until the
 * avatar is destroyed (map travel, respawn) and never stops the montage itself.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_Death : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_Death(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** Played for an ordinary death. */
	UPROPERTY(EditDefaultsOnly, Category="Death")
	TObjectPtr<UAnimMontage> DeathMontage;

	/** Played when the Knight dies while knocked down (State.HitReact.Knockdown). Falls back to DeathMontage. */
	UPROPERTY(EditDefaultsOnly, Category="Death")
	TObjectPtr<UAnimMontage> KnockdownDeathMontage;
};
