#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Interfaces/TargetableInterface.h"
#include "knightMonsterPartComponent.generated.h"

class UAbilitySystemComponent;
class UknightMonsterPartData;
class UPrimitiveComponent;
struct FGameplayEffectSpec;
struct FknightMonsterPartDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FknightMonsterPartBroken, FGameplayTag, PartTag, AActor*, BreakInstigator);

/** Runtime state of one part. */
USTRUCT(BlueprintType)
struct FknightMonsterPartState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Part")
	FGameplayTag PartTag;

	/** Damage taken so far. Compared against BreakDamage for breakable parts. */
	UPROPERTY(BlueprintReadOnly, Category="Part")
	float DamageTaken = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Part")
	bool bBroken = false;
};

/**
 * Monster parts: resolves hits to parts, provides hitzone multipliers, tracks part damage and breaks parts.
 *
 * A hit resolves to a part by the hit bone (skeletal monsters) or by the hit component's tags (separate
 * primitives, such as cube stand-ins in test maps).
 *
 * Part damage is the Health the monster actually lost from a hit, attributed to the part found from the
 * hit result in the damage effect's context. On break: the part's BreakEffect is applied to the monster,
 * HideBoneOnBreak (or the part's components) is hidden, GameplayCue.PartBreak runs at the part (the part tag is in the cue's
 * AggregatedSourceTags), Event.PartBroken is sent to the monster's ASC and OnPartBroken is broadcast.
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightMonsterPartComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UknightMonsterPartComponent();

	/** Builds runtime state and binds to the owner's Core attribute set. Called by the monster once its ASC is ready. */
	void InitializeParts(const UknightMonsterPartData* InPartData, UAbilitySystemComponent* InASC);

	FGameplayTag ResolvePart(const FHitResult& Hit) const;
	float GetDamageMultiplier(const FGameplayTag& PartTag) const;

	UFUNCTION(BlueprintPure, Category="Knight|Monster")
	bool IsPartBroken(FGameplayTag PartTag) const;

	UFUNCTION(BlueprintPure, Category="Knight|Monster")
	const TArray<FknightMonsterPartState>& GetPartStates() const { return PartStates; }

	/** Lock-on points of parts that have a LockOnSocket and are not hidden by a break. */
	void GetTargetPoints(TArray<FknightTargetPoint>& OutPoints) const;

	/** Adds damage to a part directly (scripted events, debug). Normal hits go through the Core attribute set. */
	UFUNCTION(BlueprintCallable, Category="Knight|Monster")
	void ApplyPartDamage(FGameplayTag PartTag, float Damage, AActor* DamageInstigator);

	UPROPERTY(BlueprintAssignable, Category="Knight|Monster")
	FknightMonsterPartBroken OnPartBroken;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleDamaged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* EffectSpec,
		float Magnitude, float OldValue, float NewValue);
	void BreakPart(int32 PartIndex, AActor* BreakInstigator);
	int32 FindPartIndex(const FGameplayTag& PartTag) const;
	const FknightMonsterPartDefinition* GetDefinition(int32 PartIndex) const;
	/** Owner components carrying one of the part's ComponentTags. */
	void GetPartComponents(int32 PartIndex, TArray<UPrimitiveComponent*>& OutComponents) const;

	UPROPERTY()
	TObjectPtr<const UknightMonsterPartData> PartData;

	UPROPERTY()
	TArray<FknightMonsterPartState> PartStates;

	TMap<FName, int32> BoneToPart;
	TMap<FName, int32> ComponentTagToPart;
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
};
