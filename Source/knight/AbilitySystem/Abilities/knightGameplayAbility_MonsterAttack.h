#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_MonsterAttack.generated.h"

class UAbilitySystemComponent;

UENUM(BlueprintType)
enum class EknightTargetAirborneRule : uint8
{
	Any,
	/** Anti-air moves. */
	OnlyAirborne,
	/** Ground moves that would miss a flying Knight. */
	OnlyGrounded
};

/** When a monster may choose an attack, and how likely it is among the usable ones. */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightMonsterAttackSelection
{
	GENERATED_BODY()

	/** Horizontal distance between the monster and target origins. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Selection", meta=(ClampMin=0, Units="cm"))
	float MinRange = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Selection", meta=(ClampMin=0, Units="cm"))
	float MaxRange = 600.f;

	/** Largest |yaw| between the monster's forward and the target. 180 allows any direction (tail swipes, spins). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Selection", meta=(ClampMin=0, ClampMax=180, Units="deg"))
	float MaxAngle = 60.f;

	/** Relative chance among the attacks usable right now. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Selection", meta=(ClampMin=0))
	float Weight = 1.f;

	/** Seconds after activation before this attack can be chosen again. Keeps the move set varied. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Selection", meta=(ClampMin=0, Units="s"))
	float Cooldown = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Selection")
	EknightTargetAirborneRule AirborneRule = EknightTargetAirborneRule::Any;

	/** The monster must have all of these (State.Monster.Enraged for enrage-only moves). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Selection", meta=(Categories="State"))
	FGameplayTagContainer RequiredOwnerTags;

	/** The monster must have none of these (State.Monster.Exhausted for moves too heavy when tired). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Selection", meta=(Categories="State"))
	FGameplayTagContainer BlockedOwnerTags;
};

/** What attack selection knows about the situation. Build it with Make. */
struct KNIGHT_API FknightMonsterAttackContext
{
	const UAbilitySystemComponent* MonsterASC = nullptr;
	bool bHasTarget = false;
	float Distance = 0.f;
	/** Signed yaw to the target in degrees. */
	float AngleToTarget = 0.f;
	bool bTargetAirborne = false;
	double WorldTime = 0.0;

	static FknightMonsterAttackContext Make(const AActor* Monster, const AActor* Target);
};

/**
 * Base for monster attacks chosen by the StateTree's Knight Activate Ability task. Each subclass is one
 * move; Selection decides when it is usable and how likely it is, so a move set is tuned per ability.
 *
 * Abilities are instanced per actor, so the cooldown is tracked on the instance.
 */
UCLASS(Abstract)
class KNIGHT_API UknightGameplayAbility_MonsterAttack : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_MonsterAttack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Selection weight for this situation; 0 when the attack should not be chosen. */
	virtual float GetSelectionWeight(const FknightMonsterAttackContext& Context) const;

	/** Seconds until Selection.Cooldown allows this attack again; 0 when ready. */
	float GetCooldownRemaining(double WorldTime) const;

	const FknightMonsterAttackSelection& GetSelection() const { return Selection; }

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category="Selection")
	FknightMonsterAttackSelection Selection;

private:
	double LastActivationTime = -1.e9;
};
