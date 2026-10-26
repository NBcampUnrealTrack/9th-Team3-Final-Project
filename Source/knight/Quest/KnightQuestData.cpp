#include "Quest/KnightQuestData.h"
#include "Quest/KnightProgressionIds.h"

#include "Engine/GameInstance.h"
#include "Codex/KnightCodexData.h"
#include "GameFlow/KnightGameFlowSubsystem.h"
#include "GameFlow/KnightStageCatalog.h"
#include "Monster/knightMonsterDefinition.h"
#include "Progression/knightProgressionSubsystem.h"
#include "knightItemDefinition.h"

#define LOCTEXT_NAMESPACE "KnightQuestData"

bool UKnightQuestDefinition::HasValidConfiguration(FText& OutReason) const
{
	if (QuestId.IsNone() || BossId.IsNone() || StageId.IsNone() || DisplayName.IsEmpty()
		|| !BossDefinition || Objective != EKnightQuestObjective::DefeatBoss)
	{
		OutReason = LOCTEXT("MissingTarget", "퀘스트 ID, 스테이지 또는 보스 연결이 미완성입니다.");
		return false;
	}
	if (Difficulty == EKnightQuestDifficulty::Hard && HardBossClass.IsNull())
	{
		OutReason = LOCTEXT("MissingHardBoss", "어려움 보스 개체가 지정되지 않았습니다.");
		return false;
	}
	if (MoneyReward < 0 || FirstBossSocketSlots < 0 || ClearRewards.IsEmpty())
	{
		OutReason = LOCTEXT("MissingRewards", "클리어 보상 설정이 미완성입니다.");
		return false;
	}
	for (const FName Prerequisite : PrerequisiteQuestIds)
	{
		if (Prerequisite.IsNone() || KnightProgressionIds::Quest(Prerequisite) == KnightProgressionIds::Quest(QuestId))
		{
			OutReason = LOCTEXT("BadPrerequisite", "선행 퀘스트 ID가 올바르지 않습니다.");
			return false;
		}
	}
	auto ValidReward = [](const FknightRaidReward& Reward, bool bPartBreak)
	{
		return Reward.Item && Reward.Item->GetPrimaryAssetId().IsValid()
			&& Reward.MinCount > 0 && Reward.MaxCount >= Reward.MinCount
			&& FMath::IsFinite(Reward.Chance) && Reward.Chance >= 0.f && Reward.Chance <= 1.f
			&& (Reward.RequiredBrokenPart.IsValid() == bPartBreak);
	};
	for (const FknightRaidReward& Reward : ClearRewards)
	{
		if (!ValidReward(Reward, false))
		{
			OutReason = LOCTEXT("BadClearReward", "일반 클리어 보상의 아이템, 수량 또는 확률이 올바르지 않습니다.");
			return false;
		}
	}
	for (const FKnightQuestPartBreakReward& Entry : PartBreakRewards)
	{
		if (!ValidReward(Entry.Reward, true))
		{
			OutReason = LOCTEXT("BadPartReward", "부위 파괴 보상의 태그, 아이템, 수량 또는 확률이 올바르지 않습니다.");
			return false;
		}
	}
	OutReason = FText::GetEmpty();
	return true;
}

void UKnightQuestCatalog::GetQuestDefinitions(TArray<UKnightQuestDefinition*>& OutQuests) const
{
	OutQuests.Reset();
	TMap<FName, int32> Counts;
	for (const UKnightQuestDefinition* Quest : Quests)
	{
		if (Quest && !Quest->QuestId.IsNone()) ++Counts.FindOrAdd(KnightProgressionIds::Quest(Quest->QuestId));
	}
	for (UKnightQuestDefinition* Quest : Quests)
	{
		if (Quest && !Quest->QuestId.IsNone() && Counts.FindRef(KnightProgressionIds::Quest(Quest->QuestId)) == 1) OutQuests.Add(Quest);
	}
}

UKnightQuestDefinition* UKnightQuestCatalog::FindQuest(FName QuestId) const
{
	QuestId = KnightProgressionIds::Quest(QuestId);
	if (QuestId.IsNone()) return nullptr;
	UKnightQuestDefinition* Found = nullptr;
	for (UKnightQuestDefinition* Quest : Quests)
	{
		if (!Quest || KnightProgressionIds::Quest(Quest->QuestId) != QuestId) continue;
		if (Found) return nullptr;
		Found = Quest;
	}
	return Found;
}

bool UKnightQuestCatalog::CanSelectQuest(FName QuestId, UGameInstance* GameInstance, FText& OutReason) const
{
	UKnightQuestDefinition* const Quest = FindQuest(QuestId);
	if (!Quest)
	{
		OutReason = LOCTEXT("UnknownQuest", "퀘스트가 없거나 ID가 중복됐습니다.");
		return false;
	}
	if (!Quest->bContentEnabled)
	{
		OutReason = LOCTEXT("DisabledQuest", "이 퀘스트의 전투 연결이 아직 준비되지 않았습니다.");
		return false;
	}
	if (!Quest->HasValidConfiguration(OutReason)) return false;
	const UKnightCodexData* const Codex = CodexData.LoadSynchronous();
	const FKnightCodexBossEntry* const CodexBoss = Codex
		? Codex->Bosses.FindByPredicate([Quest](const FKnightCodexBossEntry& Entry) { return Entry.BossId == Quest->BossId; })
		: nullptr;
	if (!CodexBoss || CodexBoss->MonsterDefinition != Quest->BossDefinition)
	{
		OutReason = LOCTEXT("CodexMismatch", "도감의 보스 ID와 몬스터 정의가 퀘스트와 다릅니다.");
		return false;
	}
	if (!GameInstance)
	{
		OutReason = LOCTEXT("NoGameInstance", "게임 진행 정보를 확인할 수 없습니다.");
		return false;
	}
	const UKnightGameFlowSubsystem* const Flow = GameInstance->GetSubsystem<UKnightGameFlowSubsystem>();
	const UknightProgressionSubsystem* const Progression = GameInstance->GetSubsystem<UknightProgressionSubsystem>();
	FKnightStageDefinition Stage;
	if (!Flow || !Progression || !Flow->FindStageDefinition(Quest->StageId, Stage)
		|| Stage.bDevelopmentOnly || Stage.RaidDefinition.IsNull())
	{
		OutReason = LOCTEXT("MissingStage", "스테이지 또는 레이드 정의가 연결되지 않았습니다.");
		return false;
	}
	const UknightRaidDefinition* const Raid = Stage.RaidDefinition.LoadSynchronous();
	if (!Raid || Raid->TargetMonster != Quest->BossDefinition)
	{
		OutReason = LOCTEXT("BossMismatch", "스테이지의 실제 레이드 대상이 퀘스트 보스와 다릅니다.");
		return false;
	}
	// No hard-mode spawn override is wired into the current StageCatalog/GameFlow yet.
	if (Quest->Difficulty == EKnightQuestDifficulty::Hard)
	{
		OutReason = LOCTEXT("HardNotRouted", "어려움 보스 출격 연결이 아직 준비되지 않았습니다.");
		return false;
	}
	for (const FName Prerequisite : Quest->PrerequisiteQuestIds)
	{
		if (!FindQuest(Prerequisite) || !Progression->IsQuestCompleted(Prerequisite))
		{
			OutReason = LOCTEXT("QuestLocked", "선행 퀘스트를 완료해야 합니다.");
			return false;
		}
	}
	if (!Quest->bRepeatable && Progression->IsQuestCompleted(Quest->QuestId))
	{
		OutReason = LOCTEXT("QuestComplete", "이미 완료한 퀘스트입니다.");
		return false;
	}
	return Flow->CanLaunchStage(Quest->StageId, OutReason);
}

const UKnightQuestCatalog* UKnightQuestSettings::LoadCatalog()
{
	const TSoftObjectPtr<UKnightQuestCatalog>& Reference = GetDefault<UKnightQuestSettings>()->Catalog;
	return Reference.IsNull() ? nullptr : Reference.LoadSynchronous();
}

#undef LOCTEXT_NAMESPACE
