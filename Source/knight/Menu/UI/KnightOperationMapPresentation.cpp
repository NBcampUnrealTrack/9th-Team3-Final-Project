#include "Menu/UI/KnightOperationMapPresentation.h"
#include "Quest/KnightQuestData.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Internationalization/Text.h"

#define LOCTEXT_NAMESPACE "KnightOperationMap"
FKnightQuestPresentation KnightOperationMapPresentation::Query(const UKnightQuestCatalog* Catalog, const UKnightQuestDefinition* Quest, UGameInstance* Instance)
{
	FKnightQuestPresentation Result;
	if (!Catalog || !Quest)
	{
		Result.Availability = LOCTEXT("Missing", "정보 확인 불가");
		Result.Reason = LOCTEXT("MissingReason", "퀘스트 정의를 불러올 수 없습니다.");
		return Result;
	}
	Result.bCanLaunch = Catalog->CanSelectQuest(Quest->QuestId, Instance, Result.Reason);
	// Classify the authoritative gate's stable localization key, not translated
	// prose or a second implementation of the unlock/validation rules.
	const TOptional<FString> Key = FTextInspector::GetKey(Result.Reason);
	const TOptional<FString> Namespace = FTextInspector::GetNamespace(Result.Reason);
	Result.bProgressionLocked = !Result.bCanLaunch && Key.IsSet() && Namespace.IsSet()
		&& ((Namespace.GetValue() == TEXT("KnightQuestData") && (Key.GetValue() == TEXT("QuestLocked") || Key.GetValue() == TEXT("ExplorationLocked")))
			|| (Namespace.GetValue() == TEXT("KnightGameFlow") && Key.GetValue() == TEXT("StageLocked")));
	Result.Availability = Result.bCanLaunch
		? (Quest->bMapExplorationOnly ? LOCTEXT("Explore", "맵 탐색 가능") : LOCTEXT("Ready", "출격 가능"))
		: Result.bProgressionLocked ? LOCTEXT("Locked", "잠김") : LOCTEXT("Unavailable", "출격 불가");
	if (const auto* Progress = Instance ? Instance->GetSubsystem<UknightProgressionSubsystem>() : nullptr)
		Result.bCompleted = !Quest->bMapExplorationOnly && Progress->IsQuestCompleted(Quest->QuestId);
	if (Result.bCompleted)
		Result.ClearLabel = Quest->Difficulty == EKnightQuestDifficulty::Hard
			? LOCTEXT("HardClear", "어려움 클리어") : LOCTEXT("NormalClear", "일반 클리어");
	return Result;
}
#undef LOCTEXT_NAMESPACE
