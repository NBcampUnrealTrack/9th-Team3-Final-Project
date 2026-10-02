#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "knightInventoryTypes.h"
#include "knightRaidDefinition.generated.h"

class USoundBase;
class UknightItemDefinition;
class UknightMonsterDefinition;

/**
 * Values written to the music's state parameter (UknightAudioSettings::MusicStateParameter) as the raid
 * progresses. The MetaSound maps them to layers or sections.
 */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightRaidMusicStates
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Music")
	int32 Explore = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Music")
	int32 Combat = 1;

	/** Combat while the target monster is enraged. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Music")
	int32 Enraged = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Music")
	int32 Success = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Music")
	int32 Failed = 4;
};

/** One possible reward line. Each line is rolled independently on a successful raid. */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightRaidReward
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reward")
	TObjectPtr<const UknightItemDefinition> Item;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reward", meta=(ClampMin=1))
	int32 MinCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reward", meta=(ClampMin=1))
	int32 MaxCount = 1;

	/** 0..1 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reward", meta=(ClampMin=0, ClampMax=1))
	float Chance = 1.f;

	/** Only rolled if this part was broken during the raid (part-break rewards). Empty for normal rewards. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reward", meta=(Categories="Monster.Part"))
	FGameplayTag RequiredBrokenPart;
};

/**
 * Rules of one raid (hunt). Referenced from the team's stage catalog entry (FKnightStageDefinition::RaidDefinition),
 * so the menu picks a stage and the raid GameMode reads its rules on arrival.
 */
UCLASS(BlueprintType, Const)
class KNIGHT_API UknightRaidDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Rolls every reward line against the broken parts. Returns merged stacks. */
	void RollRewards(const FGameplayTagContainer& BrokenParts, TArray<FknightItemStack>& OutRewards) const;

	/** The catalog owns the raid ID, display name and map. Clearing this stage unlocks it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Raid")
	FName RequiredClearedStageId;

	/** Monster that must be defeated. Monsters placed in the map with this definition are the targets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Raid")
	TObjectPtr<const UknightMonsterDefinition> TargetMonster;

	/** Zero means no time limit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Raid", meta=(ClampMin=0, Units="s"))
	float TimeLimit = 1800.f;

	/** Length of the drop-in phase before exploring starts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Raid", meta=(ClampMin=0, Units="s"))
	float DeployDuration = 3.f;

	/** Countdown after a success before returning to the main menu (time to carve and look around). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Raid", meta=(ClampMin=0, Units="s"))
	float SuccessReturnDelay = 20.f;

	/** Delay after a failure before returning. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Raid", meta=(ClampMin=0, Units="s"))
	float FailureReturnDelay = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rewards", meta=(TitleProperty="Item"))
	TArray<FknightRaidReward> Rewards;

	/** Looping raid music, usually a MetaSound reading the state parameter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Presentation")
	TSoftObjectPtr<USoundBase> Music;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Presentation")
	FknightRaidMusicStates MusicStates;

	/** Cues loaded when the raid starts so their first use shows (GameplayCue.Hit, GameplayCue.PartBreak, the monster's cues). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Presentation", meta=(Categories="GameplayCue"))
	FGameplayTagContainer PreloadGameplayCues;
};
