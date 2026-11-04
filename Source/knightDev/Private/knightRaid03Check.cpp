// Explicit editor-only, isolated-profile regression check. Never runs in a player build.
#if WITH_EDITOR
#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFlow/KnightGameFlowSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Progression/knightSaveGame.h"
#include "Progression/knightSaveSubsystem.h"
#include "Quest/KnightQuestData.h"
#include "Raid/knightRaidDefinition.h"
#include "Raid/knightRaidGameMode.h"
#include "Raid/knightRaidGameState.h"
#include "Raid/knightRaidSubsystem.h"

namespace
{
struct FRaid03Check
{
	TWeakObjectPtr<UGameInstance> Instance;
	int32 Step = 0;
	int32 Assertions = 0;
	int32 Failures = 0;
	double Deadline = 0;

	void Check(bool bPassed, const TCHAR* Label)
	{
		++Assertions;
		if (!bPassed) ++Failures;
		UE_LOG(LogTemp, Display, TEXT("Raid03Check %s: %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Label);
	}
	bool Finish()
	{
		UE_LOG(LogTemp, Display, TEXT("Raid03Check COMPLETE assertions=%d failures=%d"), Assertions, Failures);
		FPlatformMisc::RequestExit(false);
		return false;
	}
	void CheckMapTest(UWorld* World)
	{
		auto* GI = Instance.Get();
		auto* Flow = GI->GetSubsystem<UKnightGameFlowSubsystem>();
		auto* Run = GI->GetSubsystem<UknightRaidSubsystem>();
		auto* Saves = GI->GetSubsystem<UknightSaveSubsystem>();
		auto* Mode = World->GetAuthGameMode<AknightRaidGameMode>();
		auto* State = World->GetGameState<AknightRaidGameState>();
		Check(Mode && Mode->GetClass()->GetPathName() == TEXT("/Game/Knight/Blueprints/BP_GM_Raid03.BP_GM_Raid03_C"), TEXT("Raid03 resolves its own GameMode"));
		Check(Flow->GetActiveStageId() == TEXT("Stage_0003"), TEXT("direct map resolves Stage_0003"));
		Check(Mode && Mode->GetRaidDefinition() && Mode->GetRaidDefinition()->GetPathName() == TEXT("/Game/Knight/Raids/DA_RaidStage03.DA_RaidStage03"), TEXT("Raid03 resolves its own definition"));
		if (!Mode || !State) { Check(false, TEXT("raid mode and state present")); return; }
		Check(Mode->IsMapTestMode() && !Mode->GetRaidDefinition()->TargetMonster, TEXT("missing target enters map-test mode"));
		FKnightQuestRunContext Context;
		Check(!Run->GetCurrentRunId().IsValid() && !Run->GetActiveQuestRunContext(Context), TEXT("previous quest/run context is cleared"));
		int32 Monsters = 0;
		for (TActorIterator<AknightMonsterCharacterBase> It(World); It; ++It) ++Monsters;
		Check(Monsters == 0 && Mode->GetTargetMonsters().IsEmpty(), TEXT("no placed or fallback boss is present"));
		Check(World->GetFirstPlayerController() && World->GetFirstPlayerController()->GetPawn(), TEXT("map-test player pawn exists"));
		FString LoadingReason;
		Check(!State->ShouldShowLoadingScreen(LoadingReason) && !UGameplayStatics::IsGamePaused(World), TEXT("raid loading gate released for map testing"));
		const auto* Save = Saves->GetSaveGame();
		if (!Save) { Check(false, TEXT("isolated save loaded")); return; }
		const int64 Money = Save->Money;
		const auto Items = Save->ItemCounts;
		const int32 Records = Save->StageRecords.Num(), Quests = Save->CompletedQuestIds.Num(), Applied = Save->AppliedRaidRuns.Num();
		Check(!Mode->RequestStartRaid(), TEXT("map-test cannot start an encounter"));
		Mode->NotifyObjectiveCompleted();
		Mode->NotifyPlayerDefeated();
		Mode->HandlePlayerDeath(World->GetFirstPlayerController());
		Mode->DebugFinishRaid(true);
		Mode->DebugFinishRaid(false);
		FknightRaidResult Result;
		Check(!Run->GetLastResult(Result), TEXT("completion/death/debug hooks create no result"));
		Check(State->GetRaidPhase() == EknightRaidPhase::Explore && State->GetElapsedRaidTime() == 0.f,
			TEXT("map-test remains explorable with no encounter clock"));
		Check(Save->Money == Money && Save->ItemCounts.OrderIndependentCompareEqual(Items)
			&& Save->StageRecords.Num() == Records && Save->CompletedQuestIds.Num() == Quests && Save->AppliedRaidRuns.Num() == Applied,
			TEXT("hooks do not mutate rewards, clears or quest progression"));
		FText Reason;
		const UKnightQuestCatalog* Catalog = UKnightQuestSettings::LoadCatalog();
		Check(Catalog && Catalog->FindQuest(TEXT("Quest_0003"))
			&& !Catalog->CanSelectQuest(TEXT("Quest_0003"), GI, Reason), TEXT("Raid03 quest locked before Janhyang normal clear"));
		Check(!Flow->CanLaunchStage(TEXT("Stage_0003"), Reason), TEXT("direct stage launch respects profile unlock"));
	}
	bool Tick(float)
	{
		if (FPlatformTime::Seconds() > Deadline) { Check(false, TEXT("bounded runtime timeout")); return Finish(); }
		auto* GI = Instance.Get();
		UWorld* World = GI ? GI->GetWorld() : nullptr;
		if (!World || !World->HasBegunPlay()) return true;
		auto* Flow = GI->GetSubsystem<UKnightGameFlowSubsystem>();
		auto* Run = GI->GetSubsystem<UknightRaidSubsystem>();
		auto* Saves = GI->GetSubsystem<UknightSaveSubsystem>();
		if (Saves->IsSaveInFlight()) return true;
		const FString Map = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
		if (Step == 0)
		{
			Check(!Saves->DoesSlotExist(TEXT("Raid03")) && Saves->SelectSlot(TEXT("Raid03")), TEXT("fresh isolated test profile"));
			Run->ClearLastResult();
			Run->BeginRun(TEXT("Stage_0002"), TEXT("Quest_0002"), TEXT("Janhyang"), EKnightQuestDifficulty::Normal, Saves->GetActiveSlotId());
			++Step;
			UGameplayStatics::OpenLevel(World, TEXT("/Game/Knight/Maps/L_Raid03"));
			return true;
		}
		if ((Step == 1 || Step == 5) && Map.EndsWith(TEXT("/L_Raid03")))
		{
			CheckMapTest(World);
			++Step;
			World->GetAuthGameMode<AknightRaidGameMode>()->RequestReturnToLobby();
			return true;
		}
		if ((Step == 2 || Step == 6) && Flow->GetCurrentFlowState() == EKnightFlowState::MainMenu)
		{
			FknightRaidResult Result;
			Check(!Run->GetLastResult(Result), TEXT("leaving map test does not record abandonment or rewards"));
			if (Step == 6) return Finish();
			++Step;
			UGameplayStatics::OpenLevel(World, TEXT("/Game/Knight/Maps/L_Raid01"));
			return true;
		}
		if ((Step == 3 && Map.EndsWith(TEXT("/L_Raid01"))) || (Step == 4 && Map.EndsWith(TEXT("/L_Raid02"))))
		{
			const FName StageId = Step == 3 ? FName(TEXT("Stage_0001")) : FName(TEXT("Stage_0002"));
			FKnightStageDefinition Stage;
			auto* Mode = World->GetAuthGameMode<AknightRaidGameMode>();
			Check(Flow->GetActiveStageId() == StageId && Flow->FindStageDefinition(StageId, Stage), TEXT("existing raid resolves original stage"));
			Check(Mode && Mode->GetRaidDefinition() == Stage.RaidDefinition.LoadSynchronous()
				&& Mode->GetRaidDefinition()->TargetMonster && !Mode->IsMapTestMode(), TEXT("existing raid keeps original target and encounter behavior"));
			Check(Run->GetSelectedStageId() == StageId, TEXT("existing raid starts its own run"));
			++Step;
			if (Step == 5) Run->BeginRun(TEXT("Stage_0001"), TEXT("Quest_0001"), TEXT("Janyeol"), EKnightQuestDifficulty::Normal, Saves->GetActiveSlotId());
			UGameplayStatics::OpenLevel(World, Step == 4 ? TEXT("/Game/Knight/Maps/L_Raid02") : TEXT("/Game/Knight/Maps/L_Raid03"));
		}
		return true;
	}
};

void StartRaid03Check(UWorld* World)
{
	FString Namespace, UserDir;
	if (!World || !FParse::Value(FCommandLine::Get(), TEXT("KnightSaveTestNamespace="), Namespace)
		|| !Namespace.StartsWith(TEXT("Raid03Check")) || !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir)
		|| !UserDir.Contains(TEXT("Raid03Check")))
	{
		UE_LOG(LogTemp, Error, TEXT("Raid03Check requires isolated Raid03Check namespace and UserDir"));
		return;
	}
	auto Check = MakeShared<FRaid03Check>();
	Check->Instance = World->GetGameInstance();
	Check->Deadline = FPlatformTime::Seconds() + 900.0;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Check](float Delta) { return Check->Tick(Delta); }), .1f);
}
FAutoConsoleCommandWithWorld Raid03CheckCommand(TEXT("knight.Raid03Check"), TEXT("Isolated Raid03 wiring/map-test regression."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&StartRaid03Check), ECVF_Cheat);
}
#endif
