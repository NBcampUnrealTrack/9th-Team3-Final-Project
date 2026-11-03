#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "Quest/KnightQuestTypes.h"
#include "Raid/knightRaidDefinition.h"
#include "KnightQuestData.generated.h"

class AknightMonsterCharacterBase;
class UGameInstance;
class UknightMonsterDefinition;
class UKnightCodexData;

UENUM(BlueprintType)
enum class EKnightQuestObjective : uint8
{
	DefeatBoss
};

UENUM(BlueprintType)
enum class EKnightQuestBreakPart : uint8
{
	Head,
	Tail,
	Other
};

/** Editorial part label plus the existing raid reward row. The tag is filled once that boss has real part tags. */
USTRUCT(BlueprintType)
struct KNIGHT_API FKnightQuestPartBreakReward
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reward")
	EKnightQuestBreakPart Part = EKnightQuestBreakPart::Head;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reward")
	FknightRaidReward Reward;
};

/** A map-only region when no existing StageCatalog region represents the quest. This never enables travel. */
USTRUCT(BlueprintType)
struct KNIGHT_API FKnightQuestMapRegion
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map")
	FName RegionId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map")
	FText DisplayName;

	/** Same longitude/latitude coordinates as FKnightStageDefinition::GlobeLocation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map")
	FVector2D GlobeLocation = FVector2D::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map", meta=(ClampMin=2, ClampMax=45, Units="deg"))
	float GlobeRegionRadius = 14.f;
};

/** Editorial placement only. QuestId, not StageId, identifies each normal/hard marker. */
USTRUCT(BlueprintType)
struct KNIGHT_API FKnightQuestMapPlacement
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map")
	FName QuestId;

	/** Existing StageCatalog StageId or an AdditionalMapRegions RegionId; does not affect quest travel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map")
	FName RegionId;

	/** Longitude/latitude offset from the region centre. Moves with map pan and zoom. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map")
	FVector2D MarkerOffset = FVector2D::ZeroVector;
};

/** Static quest configuration. Completion, payout and first-boss flags belong to a save slot, not this asset. */
UCLASS(BlueprintType, Const)
class KNIGHT_API UKnightQuestDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable key used by RecordQuestCompleted and prerequisite checks. Do not rename after release. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest")
	FName QuestId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest", meta=(MultiLine=true))
	FText Description;

	/** FKnightStageDefinition::StageId, never a map path. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Target")
	FName StageId;

	/** Same stable key as FKnightCodexBossEntry::BossId. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Target")
	FName BossId;

	/** Must match the stage raid definition's TargetMonster before the quest may be selected. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Target")
	TObjectPtr<const UknightMonsterDefinition> BossDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Target")
	EKnightQuestDifficulty Difficulty = EKnightQuestDifficulty::Normal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Target")
	EKnightQuestObjective Objective = EKnightQuestObjective::DefeatBoss;

	/** A hard quest also needs an explicitly configured enhanced combat class and future spawn routing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Target")
	TSoftClassPtr<AknightMonsterCharacterBase> HardBossClass;

	/** Leave empty until the intended quest order is approved. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Unlock")
	TArray<FName> PrerequisiteQuestIds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Unlock")
	bool bRepeatable = true;

	/** Disabled entries remain visible to data tools but cannot be launched. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Unlock")
	bool bContentEnabled = false;

	/** Exposes an unfinished monster map as exploration only; never creates a quest reward run. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Unlock")
	bool bMapExplorationOnly = false;

	/** Temporary economy value. No currency is awarded in this phase. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Rewards", meta=(ClampMin=0))
	int32 MoneyReward = 0;

	/** Each FknightRaidReward line is an independent roll on a successful clear. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Rewards", meta=(TitleProperty="Item"))
	TArray<FknightRaidReward> ClearRewards;

	/** Each reward needs a real RequiredBrokenPart before the quest can be enabled. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Rewards", meta=(TitleProperty="Part"))
	TArray<FKnightQuestPartBreakReward> PartBreakRewards;

	/** Proposed shared socket expansion on the first clear of BossId, across both difficulties. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Rewards", meta=(ClampMin=0))
	int32 FirstBossSocketSlots = 1;

	/** Checks local data only; no reward roll or player progress is changed. */
	bool HasValidConfiguration(FText& OutReason) const;
};

/** Ordered list for the phase-four quest menu. Duplicate or empty QuestIds are excluded from lookup. */
UCLASS(BlueprintType, Const)
class KNIGHT_API UKnightQuestCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** BossId and monster reference must agree with the existing codex row. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest")
	TSoftObjectPtr<UKnightCodexData> CodexData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest", meta=(TitleProperty="QuestId"))
	TArray<TObjectPtr<UKnightQuestDefinition>> Quests;

	/** Display-only regions absent from StageCatalog. Never used as a raid stage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Map", meta=(TitleProperty="RegionId"))
	TArray<FKnightQuestMapRegion> AdditionalMapRegions;

	/** Quest markers on existing or additional map regions; independent of QuestDefinition::StageId. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Map", meta=(TitleProperty="QuestId"))
	TArray<FKnightQuestMapPlacement> MapPlacements;

	UFUNCTION(BlueprintCallable, Category="Knight|Quest")
	void GetQuestDefinitions(TArray<UKnightQuestDefinition*>& OutQuests) const;

	/** Returns null for an empty, missing or duplicated ID. */
	UFUNCTION(BlueprintCallable, Category="Knight|Quest")
	UKnightQuestDefinition* FindQuest(FName QuestId) const;

	/** Data, stage, boss, save-slot and prerequisite gate for phase-four selection. Does not start a raid. */
	UFUNCTION(BlueprintCallable, Category="Knight|Quest")
	bool CanSelectQuest(FName QuestId, UGameInstance* GameInstance, FText& OutReason) const;
};

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Knight Quests"))
class KNIGHT_API UKnightQuestSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category="Quest")
	TSoftObjectPtr<UKnightQuestCatalog> Catalog;

	static const UKnightQuestCatalog* LoadCatalog();
};
