#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Scene.h"
#include "GameplayTagContainer.h"
#include "knightInventoryTypes.h"
#include "knightRaidDefinition.generated.h"

class UNiagaraSystem;
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

/**
 * 처치 연출 슬로모션의 카메라 한 컷. 몬스터 몸(메시 경계) 중심을 바라본다. 방향은 몬스터 정면 기준이다(Yaw 0 = 몬스터 얼굴 앞).
 * 컷은 끊김 없이 이어진다: 한 번 쓰러지는 동작을 각도만 바꿔 보여 준다.
 */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightRaidFinishShot
{
	GENERATED_BODY()

	/** 몬스터 정면에서 돈 각도(도). 90 = 몬스터 오른쪽 옆. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shot", meta=(Units="deg"))
	float Yaw = 0.f;

	/** 위에서 내려다보는 각도(도). 음수면 아래에서 올려다본다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shot", meta=(Units="deg"))
	float Pitch = 10.f;

	/** 몸 중심에서 카메라까지 거리 = 몸 경계 반지름 × 이 값. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shot", meta=(ClampMin=0.2))
	float Distance = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shot", meta=(ClampMin=10, ClampMax=120, Units="deg"))
	float FieldOfView = 50.f;

	/** 컷 동안 몬스터 둘레를 도는 각도(도). 느린 카메라 이동. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shot", meta=(Units="deg"))
	float DriftYaw = 10.f;

	/** 컷 동안 다가가는 비율(0.1 = 거리의 10%만큼 다가감). 음수면 멀어진다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shot")
	float DriftIn = 0.08f;

	/** 바라보는 높이. 0 = 몸 경계 중심, 1 = 경계 꼭대기, -1 = 바닥. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shot", meta=(ClampMin=-1, ClampMax=1))
	float FocusHeight = 0.f;
};

/**
 * 마지막 대상 몬스터를 처치했을 때의 연출(UknightRaidFinishPresentation).
 * 막타 히트스톱 → 여러 각도의 슬로모션(Shots, SlowTime) → 원래 속도와 플레이어 시점으로 복귀(ReturnTime) → 몬스터가 땅에 닿는 순간 쿵
 * (ImpactTime) → 정적(SettleTime) → 토벌 확인 배너(BannerTime) → 여운(LingerTime) → 결과 화면.
 * 시간은 모두 실제 초다. ImpactTime만 몬스터가 죽은 뒤의 게임 시간(사망 몽타주 위치)이다.
 */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightRaidFinishSettings
{
	GENERATED_BODY()

	FknightRaidFinishSettings();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finish")
	bool bEnabled = true;

	/** 막타 순간 시간이 거의 멈추는 길이(실제 초). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finish|Hit", meta=(ClampMin=0, Units="s"))
	float HitStopTime = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finish|Hit", meta=(ClampMin=0, ClampMax=1))
	float HitStopDilation = 0.03f;

	/** 막타 순간 흰 섬광의 세기(0..1)와 사라지는 시간(실제 초). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finish|Hit", meta=(ClampMin=0, ClampMax=1))
	float FlashAlpha = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finish|Hit", meta=(ClampMin=0, Units="s"))
	float FlashTime = 0.35f;

	/**
	 * 몬스터가 죽은 뒤 몸이 땅에 닿는 게임 시간(사망 몽타주 위치, 초). 공중에서 죽어 떨어지면 떨어지는 시간과 비교해 늦은 쪽을 쓴다.
	 * 슬로모션 배율은 이 순간이 복귀 뒤 원래 속도에서 오도록 정해진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finish|Timing", meta=(ClampMin=0, Units="s"))
	float ImpactTime = 2.2f;

	/** 슬로모션 길이(실제 초). Shots가 이 시간을 똑같이 나눈다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finish|Timing", meta=(ClampMin=0.5, Units="s"))
	float SlowTime = 4.5f;

	/** 슬로모션 배율의 범위. 쓰러지는 시간이 짧거나 길어 범위를 벗어나면 슬로모션 길이가 줄거나 땅에 닿는 순간이 늦어진다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finish|Timing", meta=(ClampMin=0.02, ClampMax=1))
	float MinSlowDilation = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finish|Timing", meta=(ClampMin=0.02, ClampMax=1))
	float MaxSlowDilation = 0.5f;

	/** 원래 속도와 플레이어 시점으로 돌아오는 시간(실제 초). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finish|Timing", meta=(ClampMin=0.1, Units="s"))
	float ReturnTime = 1.0f;

	/** 복귀가 끝난 뒤 땅에 닿기까지 원래 속도로 보여 주는 시간(게임 초). 플레이어 눈앞에서 쓰러지는 것을 보는 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finish|Timing", meta=(ClampMin=0, Units="s"))
	float NormalSpeedLead = 0.4f;

	/** 쿵 뒤 배너까지의 정적(실제 초). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finish|Timing", meta=(ClampMin=0, Units="s"))
	float SettleTime = 2.5f;

	/** 배너가 떠 있는 시간과 그 뒤 결과 화면까지의 여운(실제 초). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finish|Timing", meta=(ClampMin=0, Units="s"))
	float BannerTime = 3.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finish|Timing", meta=(ClampMin=0, Units="s"))
	float LingerTime = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finish|Camera", meta=(TitleProperty="Yaw"))
	TArray<FknightRaidFinishShot> Shots;

	/** 슬로모션 카메라의 화면비. 화면보다 넓으면 위아래가 검은 띠로 가려진다(레터박스). 0이면 끈다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finish|Camera", meta=(ClampMin=0))
	float LetterboxAspectRatio = 2.39f;

	/** 슬로모션 카메라의 화면 효과(채도·비네트·색수차·그레인). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finish|Camera")
	FPostProcessSettings SlowPostProcess;

	/** 슬로모션 동안 효과음(SC_Knight_SFX와 하위)의 음량과 음높이. 소리가 무겁게 늘어진다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finish|Sound", meta=(ClampMin=0, ClampMax=1))
	float SlowSFXVolume = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finish|Sound", meta=(ClampMin=0.2, ClampMax=2))
	float SlowSFXPitch = 0.7f;

	/** 쿵: 카메라 흔들림 세기(무거운 충격 흔들림의 배수), 몬스터 발치의 이펙트와 소리. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finish|Impact", meta=(ClampMin=0))
	float ImpactShake = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finish|Impact")
	TSoftObjectPtr<UNiagaraSystem> ImpactEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finish|Impact", meta=(ClampMin=0))
	float ImpactEffectScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finish|Impact")
	TSoftObjectPtr<USoundBase> ImpactSound;
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

	/** 마지막 대상 몬스터를 처치했을 때의 슬로모션·카메라·토벌 확인 연출. 끄면 처치 즉시 결과 화면이 열린다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Presentation|Finish")
	FknightRaidFinishSettings FinishPresentation;
};
