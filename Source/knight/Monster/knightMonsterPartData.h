#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "knightMonsterPartData.generated.h"

class UGameplayEffect;

/** One hittable region of a monster (head, foreleg, tail) and its hitzone and break rules. */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightMonsterPartDefinition
{
	GENERATED_BODY()

	/** Identity under Monster.Part (for example Monster.Part.Wyvern.Head). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Part", meta=(Categories="Monster.Part"))
	FGameplayTag PartTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Part")
	FText DisplayName;

	/** Physics asset body bones that belong to this part. A hit's BoneName is matched against these. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Part")
	TArray<FName> Bones;

	/** Hitzone multiplier (1 = neutral). Weak points are simply high values; the hit cue switches at the attack's threshold. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hitzone", meta=(ClampMin=0))
	float DamageMultiplier = 1.f;

	/** Socket or bone used as a lock-on point. None means this part is not a lock-on point. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lock On")
	FName LockOnSocket;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Break")
	bool bBreakable = false;

	/** Damage this part must take (after hitzone scaling) before it breaks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Break", meta=(EditCondition="bBreakable", ClampMin=1))
	float BreakDamage = 500.f;

	/** Hitzone multiplier after the break (a broken horn often becomes softer). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Break", meta=(EditCondition="bBreakable", ClampMin=0))
	float BrokenDamageMultiplier = 1.f;

	/** Applied to the monster on break (weaken an attack, grant a tag the AI reacts to). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Break", meta=(EditCondition="bBreakable"))
	TSubclassOf<UGameplayEffect> BreakEffect;

	/** Bone hidden on break, including its children (tail cut, horn snapped). None hides nothing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Break", meta=(EditCondition="bBreakable"))
	FName HideBoneOnBreak;
};

/** Every part of one monster species. Shared by all instances of that monster. */
UCLASS(BlueprintType, Const)
class KNIGHT_API UknightMonsterPartData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Parts", meta=(TitleProperty="PartTag"))
	TArray<FknightMonsterPartDefinition> Parts;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
