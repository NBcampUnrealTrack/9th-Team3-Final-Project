#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Quest/KnightQuestTypes.h"
#include "knightInventoryTypes.h"
#include "knightRaidTypes.generated.h"

/** Where a raid is. Explore and Combat alternate as the monster finds and loses the Knight. */
UENUM(BlueprintType)
enum class EknightRaidPhase : uint8
{
	None,
	/** Dropped into the area; input can be locked for the landing sequence. */
	Deploy,
	/** Searching for the monster. */
	Explore,
	/** The target monster is fighting the Knight. */
	Combat,
	/** Target defeated; the result countdown runs before returning. */
	Success,
	Failed,
	/** Travel back to the main menu has been requested. */
	Returning
};

/** 마지막 대상 몬스터를 처치한 뒤 처치 연출의 단계(UknightRaidFinishPresentation). HUD와 음성이 따라 움직인다. */
UENUM(BlueprintType)
enum class EknightRaidFinishStage : uint8
{
	/** 연출이 없다(처치 전, 또는 연출을 끈 레이드). */
	None,
	/** 막타 히트스톱과 여러 각도의 슬로모션. HUD를 숨긴다. */
	Slow,
	/** 원래 속도로 돌아오며 플레이어 시점으로 돌아온다. */
	Return,
	/** 몬스터가 쓰러져 땅에 닿았다. 먼지가 가라앉기를 기다린다. */
	Impact,
	/** 토벌 확인 배너와 음성. */
	Banner,
	/** 연출이 끝났다. 결과 화면을 연다. */
	Done
};

UENUM(BlueprintType)
enum class EknightRaidFailReason : uint8
{
	None,
	KnightDestroyed,
	TimeUp,
	Abandoned
};

/** Outcome handed to the main menu through UknightRaidSubsystem. */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightRaidResult
{
	GENERATED_BODY()

	/** Unique across launches and persisted with reward application. */
	UPROPERTY(BlueprintReadOnly, Category="Raid")
	FGuid RunId;

	UPROPERTY(BlueprintReadOnly, Category="Raid")
	FName StageId;

	/** Captured at launch, empty for direct stage PIE and development-stage runs. */
	UPROPERTY(BlueprintReadOnly, Category="Raid")
	FKnightQuestRunContext QuestContext;

	UPROPERTY(BlueprintReadOnly, Category="Raid")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category="Raid")
	EknightRaidFailReason FailReason = EknightRaidFailReason::None;

	/** Seconds from the end of deployment to the result. */
	UPROPERTY(BlueprintReadOnly, Category="Raid")
	float ElapsedTime = 0.f;

	/** Parts broken during the raid. Part-break rewards require these. */
	UPROPERTY(BlueprintReadOnly, Category="Raid")
	FGameplayTagContainer BrokenParts;

	/** Items won. Added to the item box by UknightProgressionSubsystem. */
	UPROPERTY(BlueprintReadOnly, Category="Raid")
	TArray<FknightItemStack> Rewards;

	/** Actual quest payout for this run, set once by progression before showing the result screen. */
	UPROPERTY(BlueprintReadOnly, Category="Raid")
	int64 MoneyEarned = 0;

	UPROPERTY(BlueprintReadOnly, Category="Raid")
	int32 FirstBossSocketSlotsEarned = 0;
};
