#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/DataAsset.h"
#include "Item/knightItemDefinitions.h"
#include "knightEquipmentData.generated.h"

/** Stat tier applied once when the equipped count reaches its threshold. */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightSetTier
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Set", meta=(ClampMin=2, ClampMax=5))
	int32 RequiredCount = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Set", meta=(MultiLine=true))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Set")
	TArray<FknightPartStatModifier> StatModifiers;
};

UCLASS(BlueprintType, Const)
class KNIGHT_API UknightEquipmentSetDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Set", meta=(Categories="Equipment.Set"))
	FGameplayTag SetTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Set")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Set")
	FknightSetTier TwoPiece;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Set")
	FknightSetTier FourPiece;

	UknightEquipmentSetDefinition() { FourPiece.RequiredCount = 4; }
};

UCLASS(BlueprintType, Const)
class KNIGHT_API UknightCraftRecipeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Craft")
	TObjectPtr<const UknightPartItemDefinition> Result;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Craft")
	TArray<FknightItemCost> Costs;

	/** Existing stage clear record gates the recipe. None means available immediately. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Craft")
	FName RequiredClearedStageId;
};

/** Registry for the maintenance menu. Owned equipment still comes from the active save. */
UCLASS(BlueprintType, Const)
class KNIGHT_API UknightMaintenanceCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Maintenance")
	TArray<TObjectPtr<const UknightCraftRecipeDefinition>> Recipes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Maintenance")
	TArray<TObjectPtr<const UknightEquipmentSetDefinition>> Sets;

	/** Character class supplies the same starter loadout used by combat. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Maintenance")
	TSoftClassPtr<class ACharacter> PlayerCharacterClass;

	/**
	 * (2026-10-20) 기본 장착 에셋. 나이트 Parts 컴포넌트의 Default Loadout Asset과 같은 에셋을 넣는다.
	 * 넣으면 ReadLoadout이 PlayerCharacterClass를 읽지 않는다. PlayerCharacterClass를 읽으면 BP_Knight와 딸린 에셋
	 * 1,000여 개를 그 자리에서 로드해서 정비소 첫 진입이 4~5초 멈췄다. 비어 있으면 예전처럼 PlayerCharacterClass를 쓴다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Maintenance")
	TObjectPtr<const UknightLoadoutDefinition> DefaultLoadout;
};

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Knight Equipment"))
class KNIGHT_API UknightEquipmentSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(Config, EditAnywhere, Category="Maintenance")
	TSoftObjectPtr<UknightMaintenanceCatalog> Catalog;

	static const UknightMaintenanceCatalog* LoadCatalog();
};
