#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/knightAbilitySet.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "knightPartsComponent.generated.h"

class USkeletalMeshComponent;
class UknightPartItemDefinition;

/** Part is null when the slot was emptied. Item definitions are shared assets: listeners must not modify them. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FknightEquipmentChanged, FGameplayTag, SlotTag, UknightPartItemDefinition*, Part);

/** One equipped part: what it is, its mesh, and what it granted. */
USTRUCT()
struct FknightEquippedPart
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<const UknightPartItemDefinition> Definition;

	UPROPERTY()
	int32 UpgradeLevel = 0;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> MeshComponent;

	UPROPERTY()
	FknightAbilitySet_GrantedHandles GrantedHandles;
};

/**
 * Equips mech parts and the weapon: spawns each part's mesh on the mech (placement from the Mech Parts
 * Workbench data), and grants the part's ability set at its upgrade level. Unequipping removes both exactly.
 * The loadout comes from the save (UknightProgressionSubsystem), falling back to DefaultLoadout per slot.
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightPartsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UknightPartsComponent();

	/** Equips the saved loadout. Called by the Knight once its ability system is initialized. */
	void ApplyLoadout();

	/** Equips a part into its slot, replacing what was there. */
	UFUNCTION(BlueprintCallable, Category="Knight|Parts")
	bool EquipPart(const UknightPartItemDefinition* Part, int32 UpgradeLevel);

	UFUNCTION(BlueprintCallable, Category="Knight|Parts")
	void UnequipSlot(UPARAM(meta=(Categories="Equipment.Slot")) FGameplayTag SlotTag);

	const UknightPartItemDefinition* GetEquippedPart(FGameplayTag SlotTag) const;

	/** Meshes of every equipped part (for damage visuals, dissolves, outlines). */
	void GetPartMeshes(TArray<USkeletalMeshComponent*>& OutMeshes) const;

	UPROPERTY(BlueprintAssignable, Category="Knight|Parts")
	FknightEquipmentChanged OnEquipmentChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Parts used for slots the save has nothing for (a new game's starter mech). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Parts")
	TArray<TObjectPtr<const UknightPartItemDefinition>> DefaultLoadout;

private:
	void RemovePart(FknightEquippedPart& Part);

	UPROPERTY(Transient)
	TMap<FGameplayTag, FknightEquippedPart> EquippedParts;
};
