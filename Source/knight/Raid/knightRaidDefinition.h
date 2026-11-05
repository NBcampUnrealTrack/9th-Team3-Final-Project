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

	/** Confirmed quest completion required even for an explicitly unlocked stage. Empty keeps the existing stage rules. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Raid")
	FName RequiredCompletedQuestId;

	/** Monster that must be defeated. Monsters placed in the map with this definition are the targets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Raid")
	TObjectPtr<const UknightMonsterDefinition> TargetMonster;

	/** An unfinished boss map stays in map-test mode until TargetMonster is assigned. Legacy objective raids may leave this off. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Raid")
	bool bRequireTargetMonster = false;

	bool IsWaitingForTargetMonster() const { return bRequireTargetMonster && !TargetMonster; }

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

	/**
	 * Track started by the first fight with a target monster (the raid is silent while exploring before that).
	 * A non-looping track, repeated after CombatMusicLoopGap. Keeps playing until the raid ends.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Presentation|Combat Music")
	TSoftObjectPtr<USoundBase> CombatMusic;

	/** Replaces CombatMusic with a crossfade once a target monster's health drops below CombatMusicLowHealthRatio. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Presentation|Combat Music")
	TSoftObjectPtr<USoundBase> CombatMusicLowHealth;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Presentation|Combat Music", meta=(ClampMin=0, ClampMax=1))
	float CombatMusicLowHealthRatio = 0.5f;

	/** Silence between the end of a combat track and its next play. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Presentation|Combat Music", meta=(ClampMin=0, Units="s"))
	float CombatMusicLoopGap = 5.f;

	/** Crossfade from CombatMusic to CombatMusicLowHealth, and the fade out when the raid ends. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Presentation|Combat Music", meta=(ClampMin=0, Units="s"))
	float CombatMusicSwitchFadeTime = 3.f;

	/**
	 * 전투 음악이 보스 BGM MetaSound(MS_BGM_*)일 때 켠다. 인트로 → 루프 → 엔딩과 Normal ↔ Enrage 전환을 MetaSound가 맡고,
	 * 여기서는 입력만 바꾼다: Intensity(0/1), IntensityFadeTime, Ending(0/1), PlayEnding(트리거).
	 * 대상 몬스터가 2페이즈(State.Monster.Phase2)이거나 분노(State.Monster.Enraged, 잔향 공명해방 포함)면 Enrage 곡.
	 * 레이드 성공 때는 끄지 않고 그 상태의 엔딩을 끝까지 튼다. 켜면 CombatMusicLoopGap은 0으로 둔다(엔딩 뒤 다시 틀지 않게).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Presentation|Combat Music")
	bool bCombatMusicDynamic = false;

	/** Normal → Enrage 크로스페이드. 포효 소리가 전환 순간을 덮는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Presentation|Combat Music", meta=(ClampMin=0, Units="s", EditCondition="bCombatMusicDynamic"))
	float CombatMusicEnrageFadeTime = 2.5f;

	/** Enrage → Normal 크로스페이드(분노가 풀릴 때). 천천히 가라앉게 길게 둔다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Presentation|Combat Music", meta=(ClampMin=0, Units="s", EditCondition="bCombatMusicDynamic"))
	float CombatMusicCalmFadeTime = 5.f;

	/** Cues loaded when the raid starts so their first use shows (GameplayCue.Hit, GameplayCue.PartBreak, the monster's cues). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Presentation", meta=(Categories="GameplayCue"))
	FGameplayTagContainer PreloadGameplayCues;
};
