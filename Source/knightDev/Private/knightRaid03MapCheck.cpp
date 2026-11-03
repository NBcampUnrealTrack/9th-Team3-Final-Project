// Isolated menu-to-map exploration regression; editor module only.
#if WITH_EDITOR
#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFlow/KnightGameFlowSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Progression/knightSaveGame.h"
#include "Progression/knightSaveSubsystem.h"
#include "Quest/KnightQuestData.h"
#include "Raid/knightRaidGameMode.h"
#include "Raid/knightRaidSubsystem.h"
#include "Raid/knightRaidDefinition.h"

namespace
{
struct FRaid03MapCheck
{
	TWeakObjectPtr<UGameInstance> GI;
	int32 Step = 0, Assertions = 0, Failures = 0;
	double Deadline = 0;
	int64 MoneyAtEntry = 0;
	TMap<FPrimaryAssetId, int32> ItemsAtEntry;
	int32 ClearsAtEntry = 0, RunsAtEntry = 0;

	void Check(bool Passed, const TCHAR* Label)
	{
		++Assertions;
		if (!Passed) ++Failures;
		UE_LOG(LogTemp, Display, TEXT("Raid03MapCheck %s: %s"), Passed ? TEXT("PASS") : TEXT("FAIL"), Label);
	}
	bool Finish()
	{
		UE_LOG(LogTemp, Display, TEXT("Raid03MapCheck COMPLETE assertions=%d failures=%d"), Assertions, Failures);
		FPlatformMisc::RequestExit(Failures > 0);
		return false;
	}
	bool Tick(float)
	{
		if (FPlatformTime::Seconds() > Deadline) { Check(false, TEXT("runtime timeout")); return Finish(); }
		UGameInstance* Instance = GI.Get();
		UWorld* World = Instance ? Instance->GetWorld() : nullptr;
		if (!World || !World->HasBegunPlay()) return true;
		auto* Flow = Instance->GetSubsystem<UKnightGameFlowSubsystem>();
		auto* Progress = Instance->GetSubsystem<UknightProgressionSubsystem>();
		auto* Saves = Instance->GetSubsystem<UknightSaveSubsystem>();
		auto* Run = Instance->GetSubsystem<UknightRaidSubsystem>();
		const UKnightQuestCatalog* Catalog = UKnightQuestSettings::LoadCatalog();
		if (!Flow || !Progress || !Saves || !Run || !Catalog) { Check(false, TEXT("required subsystems")); return Finish(); }
		if (Saves->IsSaveInFlight()) return true;
		const FString Map = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
		FText Reason;
		if (Step == 0 && Flow->GetCurrentFlowState() == EKnightFlowState::MainMenu)
		{
			Check(!Saves->DoesSlotExist(TEXT("MapA")) && Saves->SelectSlot(TEXT("MapA")), TEXT("fresh isolated profile A"));
			Check(!Progress->IsQuestCompleted(TEXT("Quest_0002")) && !Flow->CanLaunchStage(TEXT("Stage_0003"), Reason)
				&& !Catalog->CanSelectQuest(TEXT("Quest_0003"), Instance, Reason), TEXT("Raid03 locked before normal Janhyang clear"));
			TArray<FKnightStageDefinition> VisibleStages;
			Flow->GetStageDefinitions(VisibleStages);
			Check(!VisibleStages.ContainsByPredicate([](const FKnightStageDefinition& Stage) { return Stage.StageId == TEXT("Stage_0003"); }),
				TEXT("Raid03 has no duplicate stage region marker"));
			Check(Progress->RecordQuestCompleted(TEXT("Quest_0002")), TEXT("record normal Janhyang clear through progression API"));
			Step = 1;
			return true;
		}
		if (Step == 1 && Flow->GetCurrentFlowState() == EKnightFlowState::MainMenu)
		{
			Check(Progress->IsQuestCompleted(TEXT("Quest_0002")) && Flow->CanLaunchStage(TEXT("Stage_0003"), Reason)
				&& Catalog->CanSelectQuest(TEXT("Quest_0003"), Instance, Reason), TEXT("same profile unlocks Raid03 map exploration"));
			Check(!Catalog->CanSelectQuest(TEXT("Quest_Heukgwan_Hard"), Instance, Reason), TEXT("unfinished hard quest remains disabled"));
			Check(Flow->RequestStartQuest(TEXT("Quest_0003"), Reason), TEXT("normal menu quest flow accepts map exploration"));
			Step = 2;
			return true;
		}
		if (Step == 2 && Map.EndsWith(TEXT("/L_Raid03")))
		{
			auto* Mode = World->GetAuthGameMode<AknightRaidGameMode>();
			if (!Mode) { Check(false, TEXT("Raid03 GameMode exists")); return Finish(); }
			FKnightQuestRunContext Context;
			Check(Flow->GetActiveStageId() == TEXT("Stage_0003") && Mode->IsMapTestMode()
				&& !Run->GetCurrentRunId().IsValid() && !Run->GetActiveQuestRunContext(Context),
				TEXT("menu travel enters Raid03 without a reward run"));
			int32 Monsters = 0;
			for (TActorIterator<AknightMonsterCharacterBase> It(World); It; ++It) ++Monsters;
			Check(Monsters == 0, TEXT("map exploration has no boss"));
			const UknightSaveGame* Save = Saves->GetSaveGame();
			if (!Save) { Check(false, TEXT("isolated save exists")); return Finish(); }
			MoneyAtEntry = Save->Money;
			ItemsAtEntry = Save->ItemCounts;
			ClearsAtEntry = Save->StageRecords.Num();
			RunsAtEntry = Save->AppliedRaidRuns.Num();
			Mode->NotifyObjectiveCompleted();
			Mode->NotifyPlayerDefeated();
			FknightRaidResult Result;
			Check(!Run->GetLastResult(Result), TEXT("completion and defeat cannot settle map exploration"));
			Mode->RequestReturnToLobby();
			Step = 3;
			return true;
		}
		if (Step == 3 && Flow->GetCurrentFlowState() == EKnightFlowState::MainMenu)
		{
			const UknightSaveGame* Save = Saves->GetSaveGame();
			FknightRaidResult Result;
			Check(Save && Save->Money == MoneyAtEntry && Save->ItemCounts.OrderIndependentCompareEqual(ItemsAtEntry)
				&& Save->StageRecords.Num() == ClearsAtEntry && Save->AppliedRaidRuns.Num() == RunsAtEntry
				&& Save->CompletedQuestIds.Contains(TEXT("Quest_0002")) && !Save->CompletedQuestIds.Contains(TEXT("Quest_0003"))
				&& !Run->GetLastResult(Result), TEXT("return grants nothing and preserves quest progress"));
			Check(!Saves->DoesSlotExist(TEXT("MapB")) && Saves->SelectSlot(TEXT("MapB")), TEXT("fresh isolated profile B"));
			Check(!Flow->CanLaunchStage(TEXT("Stage_0003"), Reason)
				&& !Catalog->CanSelectQuest(TEXT("Quest_0003"), Instance, Reason), TEXT("profile B remains locked"));
			Step = 4;
			return true;
		}
		if (Step == 4 && Flow->GetCurrentFlowState() == EKnightFlowState::MainMenu)
		{
			Check(Saves->SelectSlot(TEXT("MapA")) && Flow->CanLaunchStage(TEXT("Stage_0003"), Reason)
				&& Catalog->CanSelectQuest(TEXT("Quest_0003"), Instance, Reason), TEXT("profile A unlock survives save and reload"));
			return Finish();
		}
		return true;
	}
};

void StartRaid03MapCheck(UWorld* World)
{
	FString Namespace, UserDir;
	if (!World || !FParse::Value(FCommandLine::Get(), TEXT("KnightSaveTestNamespace="), Namespace)
		|| !Namespace.StartsWith(TEXT("Raid03Map")) || !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir)
		|| !UserDir.Contains(TEXT("Raid03Map"))) return;
	auto Check = MakeShared<FRaid03MapCheck>();
	Check->GI = World->GetGameInstance();
	Check->Deadline = FPlatformTime::Seconds() + 900.0;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Check](float Delta) { return Check->Tick(Delta); }), .1f);
}
FAutoConsoleCommandWithWorld Raid03MapCheckCommand(TEXT("knight.Raid03MapCheck"), TEXT("Isolated Raid03 quest-map travel check."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&StartRaid03MapCheck), ECVF_Cheat);
}
#endif
