#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MechPartAttachment.h"
#include "knightItemDefinition.h"
#include "knightItemDefinitions.generated.h"

class USkeletalMesh;
class UknightAbilitySet;
class UknightMonsterDefinition;

/** Items and amounts consumed by one upgrade step. */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightItemCost
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cost")
	TObjectPtr<const UknightItemDefinition> Item;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cost", meta=(ClampMin=1))
	int32 Count = 1;
};

USTRUCT(BlueprintType)
struct KNIGHT_API FknightUpgradeStep
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Upgrade", meta=(TitleProperty="Item"))
	TArray<FknightItemCost> Costs;
};

/** Monster material (horn, scale, core). Obtained from raid rewards, spent on upgrades. */
UCLASS(BlueprintType, Const)
class KNIGHT_API UknightMaterialItemDefinition : public UknightItemDefinition
{
	GENERATED_BODY()

public:
	UknightMaterialItemDefinition();

	/** Monster that drops it, for the item description. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Material")
	TObjectPtr<const UknightMonsterDefinition> SourceMonster;
};

/**
 * Mech part: a mesh placed by the Mech Parts Workbench, and the stats and abilities it grants.
 * Upgrading raises the level passed to the ability set, so stat effects scale through curve tables.
 */
UCLASS(BlueprintType, Const)
class KNIGHT_API UknightPartItemDefinition : public UknightItemDefinition
{
	GENERATED_BODY()

public:
	UknightPartItemDefinition();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** Highest upgrade level (the number of upgrade steps). */
	int32 GetMaxUpgradeLevel() const { return UpgradeSteps.Num(); }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Part", meta=(Categories="Equipment.Slot"))
	FGameplayTag SlotTag;

	/**
	 * 파츠 중량 (2026-10-07). 장착한 파츠 중량의 합이 메카의 Weight 스탯이 되어 이동·퀵스텝 속도를 낮추고,
	 * 합이 적재 한도(LoadCapacity 스탯)를 넘는 장착은 거부된다 (UknightPartsComponent::CanEquipPart).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Part", meta=(ClampMin=0))
	float Weight = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Part")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	/** Written by the Mech Parts Workbench. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Part")
	FMechPartAttachment Attachment;

	/** Tags added to the spawned mesh component. "Weapon" lets melee trace notifies find the weapon mesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Part")
	TArray<FName> MeshComponentTags;

	/** Stat effects and abilities while equipped. Granted at level 1 + upgrade level. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Part")
	TObjectPtr<const UknightAbilitySet> AbilitySet;

	/** UpgradeSteps[N] is the cost of going from level N to N + 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Upgrade")
	TArray<FknightUpgradeStep> UpgradeSteps;
};

/** The greatsword and its variants. A part in the weapon slot whose mesh carries the "Weapon" tag. */
UCLASS(BlueprintType, Const)
class KNIGHT_API UknightWeaponItemDefinition : public UknightPartItemDefinition
{
	GENERATED_BODY()

public:
	UknightWeaponItemDefinition();
};
