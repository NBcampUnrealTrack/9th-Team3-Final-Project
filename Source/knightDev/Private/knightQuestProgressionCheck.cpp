// Explicit, isolated API/UMG-state check. No combat, input automation or production saves.
#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFlow/KnightGameFlowSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Menu/UI/KnightMainMenuWidget.h"
#include "Menu/UI/KnightQuestMenuWidget.h"
#include "Menu/UI/KnightStageMapWidget.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Progression/knightSaveGame.h"
#include "Progression/knightSaveSubsystem.h"
#include "Quest/KnightQuestData.h"
#include "Raid/knightRaidSubsystem.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace
{
struct FQuestProgressionCheck
{
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<UKnightMainMenuWidget> Menu;
	TWeakObjectPtr<UKnightQuestMenuWidget> Detail;
	FString Namespace, Mode;
	double Deadline = 0;
	int32 Step = 0, Assertions = 0, Failures = 0;
	FknightRaidResult SuccessfulResult;

	void Check(bool Passed, const TCHAR* Label)
	{
		++Assertions; if (!Passed) ++Failures;
		UE_LOG(LogTemp, Display, TEXT("QuestProgressionCheck %s %s: %s"), *Mode, Passed ? TEXT("PASS") : TEXT("FAIL"), Label);
	}
	bool Finish()
	{
		UE_LOG(LogTemp, Display, TEXT("QuestProgressionCheck COMPLETE mode=%s assertions=%d failures=%d; API/UMG-state only, no interactive/combat verification"), *Mode, Assertions, Failures);
		FPlatformMisc::RequestExit(false);
		return false;
	}
	bool UIEnabled() const
	{
		if (!Detail.IsValid()) return false;
		const FObjectProperty* Property = FindFProperty<FObjectProperty>(Detail->GetClass(), TEXT("LaunchButton"));
		const UWidget* Button = Property ? Cast<UWidget>(Property->GetObjectPropertyValue_InContainer(Detail.Get())) : nullptr;
		return Button && Button->GetIsEnabled();
	}
	void Result(UknightRaidSubsystem* Raid, UknightSaveSubsystem* Saves, bool Success,
		EknightRaidFailReason Reason, bool Quest = true)
	{
		FknightRaidResult R;
		R.RunId = Raid->BeginRun(TEXT("Stage_01"), Quest ? FName(TEXT("Quest_Janyeol_Normal")) : NAME_None,
			Quest ? FName(TEXT("Janyeol")) : NAME_None, EKnightQuestDifficulty::Normal, Saves->GetActiveSlotId());
		R.StageId = TEXT("Stage_01"); R.bSuccess = Success; R.FailReason = Reason; R.ElapsedTime = 60.f;
		Check(Raid->RecordResult(R), TEXT("result accepted through existing raid settlement event"));
		if (Success && Quest) Raid->GetLastResult(SuccessfulResult);
	}
	bool Tick(float)
	{
		if (FPlatformTime::Seconds() > Deadline) { Check(false, TEXT("bounded check timeout")); return Finish(); }
		UGameInstance* GI = World.IsValid() ? World->GetGameInstance() : nullptr;
		if (!GI) { Check(false, TEXT("game instance exists")); return Finish(); }
		auto* Saves = GI->GetSubsystem<UknightSaveSubsystem>();
		auto* Progress = GI->GetSubsystem<UknightProgressionSubsystem>();
		auto* Flow = GI->GetSubsystem<UKnightGameFlowSubsystem>();
		auto* Raid = GI->GetSubsystem<UknightRaidSubsystem>();
		const auto* Catalog = UKnightQuestSettings::LoadCatalog();
		if (!Saves || !Progress || !Flow || !Raid || !Catalog) { Check(false, TEXT("dependencies loaded")); return Finish(); }
		if (Saves->IsSaveInFlight()) return true;
		if (Saves->IsSaveRecoveryPending()) { Check(false, TEXT("snapshot saved successfully")); return Finish(); }
		FText Reason;
		auto Available = [&]() { return Catalog->CanSelectQuest(TEXT("Quest_0002"), GI, Reason); };
		if (Mode == TEXT("Restore"))
		{
			Check(Saves->DoesSlotExist(TEXT("Gate")) && Saves->SelectSlot(TEXT("Gate")), TEXT("saved isolated profile restored in new process"));
			Check(Available() && Flow->CanLaunchStage(TEXT("Stage_0002"), Reason), TEXT("quest and stage availability persist after restart"));
			Check(Progress->IsQuestCompleted(TEXT("Quest_Janyeol_Normal")) && Progress->IsQuestCompleted(TEXT("Quest_0001")), TEXT("old/new completion API IDs agree"));
			const auto* Q = Catalog->FindQuest(TEXT("Quest_0001"));
			Check(Q && Progress->GetMoney() == int64(Q->MoneyReward) * 2, TEXT("repeat rewards persist exactly twice"));
			const int64 Money = Progress->GetMoney();
			FknightRaidResult Duplicate; Duplicate.RunId = *Saves->GetSaveGame()->AppliedRaidRuns.CreateConstIterator();
			Duplicate.bSuccess = true; Duplicate.StageId = TEXT("Stage_0001");
			Raid->OnRaidResultRecorded.Broadcast(Duplicate);
			Check(Progress->GetMoney() == Money, TEXT("persisted run ID suppresses duplicate payout"));
			Check(Saves->SelectSlot(TEXT("Other")) && !Available(), TEXT("other profile remains locked after restart"));
			Check(Saves->SelectSlot(TEXT("Legacy")) && Available() && Progress->GetMoney() == 123, TEXT("legacy completion snapshot still unlocks without reward replay"));
			return Finish();
		}
		switch (Step++)
		{
		case 0:
		{
			Check(!Saves->DoesSlotExist(TEXT("Gate")), TEXT("fresh isolated namespace"));
			if (Saves->DoesSlotExist(TEXT("Gate"))) return Finish();
			Check(Saves->SelectSlot(TEXT("Gate")), TEXT("fresh profile selected"));
			TArray<UKnightQuestDefinition*> Quests; Catalog->GetQuestDefinitions(Quests);
			Check(Quests.Num() == 6, TEXT("six distinct quest definitions"));
			const auto* First = Catalog->FindQuest(TEXT("Quest_0001"));
			const auto* Second = Catalog->FindQuest(TEXT("Quest_0002"));
			Check(First && Second && First->BossId == TEXT("Janyeol") && Second->BossId == TEXT("Janhyang")
				&& First->Difficulty == EKnightQuestDifficulty::Normal && Second->Difficulty == EKnightQuestDifficulty::Normal,
				TEXT("numbered quests retain actual normal boss identities"));
			Check(First && First == Catalog->FindQuest(TEXT("Quest_Janyeol_Normal"))
				&& Second == Catalog->FindQuest(TEXT("Quest_Janhyang_Normal")), TEXT("legacy quest aliases resolve same assets"));
			Check(Second && Second->PrerequisiteQuestIds.Num() == 1 && Second->PrerequisiteQuestIds[0] == TEXT("Quest_0001"), TEXT("exact prerequisite"));
			for (const auto* Q : Quests) if (Q != First && Q != Second) Check(!Q->bContentEnabled, TEXT("other four quest enable states preserved"));
			FKnightStageDefinition S1, S2;
			Check(Flow->FindStageDefinition(TEXT("Stage_01"), S1) && S1.StageId == TEXT("Stage_0001")
				&& Flow->FindStageDefinition(TEXT("Stage_02"), S2) && S2.StageId == TEXT("Stage_0002"), TEXT("stage aliases resolve canonical IDs"));
			Check(S1.GameplayMap.ToSoftObjectPath().GetLongPackageName() == TEXT("/Game/Knight/Maps/L_Raid01")
				&& S2.GameplayMap.ToSoftObjectPath().GetLongPackageName() == TEXT("/Game/Knight/Maps/L_Raid02"), TEXT("unchanged destination maps"));
			Check(Catalog->CanSelectQuest(TEXT("Quest_0001"), GI, Reason) && !Available(), TEXT("first quest ready; second locked on fresh profile"));
			Saves->GetSaveGame()->UnlockedStages.Add(TEXT("Stage_0002"));
			Check(!Flow->CanLaunchStage(TEXT("Stage_0002"), Reason), TEXT("explicit stage unlock cannot bypass confirmed quest prerequisite"));
			Saves->GetSaveGame()->UnlockedStages.Remove(TEXT("Stage_0002"));
			Check(!Flow->RequestStartQuest(TEXT("Quest_0002"), Reason) && !Flow->IsTravelPending(), TEXT("quest entry API rejects locked quest"));
			Check(!Flow->RequestStartStage(TEXT("Stage_02")) && !Flow->IsTravelPending(), TEXT("direct stage API also rejects locked stage"));
			for (TObjectIterator<UKnightMainMenuWidget> It; It; ++It)
				if (It->GetWorld() == World.Get() && It->IsInViewport()) { Menu = *It; break; }
			Check(Menu.IsValid() && Menu->ShowPage(EKnightMainMenuPage::Stage), TEXT("actual main menu stage page opened by handler"));
			if (Menu.IsValid())
			{
				if (auto* Map = Cast<UKnightStageMapWidget>(Menu->GetWidgetFromName(TEXT("Map_Stages")))) Map->OnQuestClicked.Broadcast(TEXT("Quest_0002"));
				for (TObjectIterator<UKnightQuestMenuWidget> It; It; ++It) if (It->GetWorld() == World.Get()) { Detail = *It; break; }
			}
			Check(Detail.IsValid() && Detail->GetSelectedQuestId() == TEXT("Quest_0002") && !UIEnabled(), TEXT("selected locked quest has disabled launch control"));
			Raid->BeginRun(TEXT("Stage_01"), TEXT("Quest_Janyeol_Normal"), TEXT("Janyeol"), EKnightQuestDifficulty::Normal, Saves->GetActiveSlotId());
			Check(!Available(), TEXT("starting run does not complete prerequisite")); Raid->EndRun();
			Result(Raid, Saves, false, EknightRaidFailReason::KnightDestroyed); break;
		}
		case 1:
			Check(!Available() && Progress->GetMoney() == 0, TEXT("death/failure does not unlock or reward"));
			Result(Raid, Saves, false, EknightRaidFailReason::Abandoned); break;
		case 2:
			Check(!Available() && Progress->GetMoney() == 0, TEXT("abandonment does not unlock or reward"));
			Result(Raid, Saves, true, EknightRaidFailReason::None, false); break;
		case 3:
			Check(Progress->IsStageCleared(TEXT("Stage_01")) && !Available(), TEXT("stage clear without quest context is not quest completion"));
			Result(Raid, Saves, true, EknightRaidFailReason::None); break;
		case 4:
		{
			Check(Available() && Flow->CanLaunchStage(TEXT("Stage_02"), Reason), TEXT("confirmed quest success opens quest and stage"));
			Check(Detail.IsValid() && Detail->GetSelectedQuestId() == TEXT("Quest_0002") && UIEnabled(), TEXT("open quest UI refreshes via progression event without clearing selection"));
			const auto Before = Saves->GetSaveGame()->ItemCounts; const int64 Money = Progress->GetMoney();
			Check(!Raid->RecordResult(SuccessfulResult), TEXT("same run result rejected"));
			Raid->OnRaidResultRecorded.Broadcast(SuccessfulResult);
			Check(Money == Progress->GetMoney() && Before.OrderIndependentCompareEqual(Saves->GetSaveGame()->ItemCounts), TEXT("duplicate settlement cannot add or reroll rewards"));
			Check(!Progress->RecordQuestCompleted(TEXT("Quest_Janyeol_Normal")), TEXT("legacy completion callback remains idempotent"));
			Result(Raid, Saves, true, EknightRaidFailReason::None); break;
		}
		case 5:
		{
			const auto* Q = Catalog->FindQuest(TEXT("Quest_0001"));
			Check(Q && Progress->GetMoney() == int64(Q->MoneyReward) * 2 && Progress->GetUnlockedSharedSocketSlots() == Q->FirstBossSocketSlots,
				TEXT("new run repeats regular rewards but not first-boss award"));
			Raid->EndRun();
			Check(Saves->SelectSlot(TEXT("Other")) && !Available(), TEXT("progress isolated between profiles"));
			Check(Detail.IsValid() && Detail->GetSelectedQuestId().IsNone() && !UIEnabled(), TEXT("profile event clears stale quest UI selection"));
			Check(Saves->SelectSlot(TEXT("Gate")) && Available(), TEXT("saved progress reload unlocks without another clear"));
			if (Menu.IsValid()) { Menu->ShowPage(EKnightMainMenuPage::Home); Menu->ShowPage(EKnightMainMenuPage::Stage); }
			Check(Available(), TEXT("lobby page reentry preserves availability"));
			UknightSaveGame* Legacy = NewObject<UknightSaveGame>();
			Legacy->CompletedQuestIds.Add(TEXT("Quest_Janyeol_Normal")); Legacy->CompletedQuestIds.Add(TEXT("Unrelated_Quest"));
			Legacy->StageRecords.FindOrAdd(TEXT("Stage_01")).ClearCount = 3;
			Legacy->StageRecords.FindOrAdd(TEXT("Stage_0001")).ClearCount = 2;
			Legacy->UnlockedStages.Add(TEXT("Stage_02")); Legacy->Money = 123;
			Check(UGameplayStatics::SaveGameToSlot(Legacy, TEXT("KnightProgress_Test_") + Namespace + TEXT("_Legacy"), 0), TEXT("isolated legacy save fixture written"));
			Check(Saves->SelectSlot(TEXT("Legacy")) && Available(), TEXT("existing legacy completion unlocks immediately on load"));
			Check(Progress->GetMoney() == 123 && Saves->GetSaveGame()->AppliedRaidRuns.IsEmpty()
				&& Saves->GetSaveGame()->CompletedQuestIds.Contains(TEXT("Unrelated_Quest")), TEXT("migration preserves unrelated records without rewards"));
			Check(Saves->GetSaveGame()->StageRecords.FindRef(TEXT("Stage_0001")).ClearCount == 3
				&& !Saves->GetSaveGame()->StageRecords.Contains(TEXT("Stage_01")), TEXT("stage migration merges aliases without double counting"));
			Saves->RequestSave(); break;
		}
		default: return Finish();
		}
		return true;
	}
};
void StartQuestProgressionCheck(const TArray<FString>& Args, UWorld* World)
{
	FString Namespace, UserDir;
	if (!World || Args.Num() != 1 || (Args[0] != TEXT("Seed") && Args[0] != TEXT("Restore"))
		|| !FParse::Value(FCommandLine::Get(), TEXT("KnightSaveTestNamespace="), Namespace) || !Namespace.StartsWith(TEXT("QuestGate"))
		|| !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) || !UserDir.Contains(TEXT("KnightQuestGate")))
	{ UE_LOG(LogTemp, Error, TEXT("QuestProgressionCheck requires isolated KnightQuestGate UserDir and QuestGate namespace")); return; }
	auto Run = MakeShared<FQuestProgressionCheck>(); Run->World = World; Run->Namespace = Namespace; Run->Mode = Args[0];
	Run->Deadline = FPlatformTime::Seconds() + 120;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), .1f);
}
FAutoConsoleCommandWithWorldAndArgs Command(TEXT("knight.QuestProgressionCheck"), TEXT("Isolated quest prerequisite/save compatibility checks: Seed or Restore."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StartQuestProgressionCheck), ECVF_Cheat);
}
#endif
