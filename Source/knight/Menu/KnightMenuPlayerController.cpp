#include "KnightMenuPlayerController.h"
#include "Camera/knightPlayerCameraManager.h"

#include "KnightMenuGameMode.h"
#include "UI/KnightMainMenuWidget.h"
#include "UI/KnightMenuWidgetBase.h"
#include "UI/KnightTitleMenuWidget.h"
#include "GameFlow/KnightGameFlowSubsystem.h"
#include "Progression/knightSaveGame.h"
#include "Progression/knightSaveSubsystem.h"
#include "Quest/KnightQuestData.h"
#include "Raid/knightRaidSubsystem.h"
#include "knightItemDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "knight.h"

AKnightMenuPlayerController::AKnightMenuPlayerController()
{
	PlayerCameraManagerClass = AknightPlayerCameraManager::StaticClass();
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void AKnightMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController() || MenuWidget)
	{
		return;
	}

	const AKnightMenuGameMode* const MenuMode = GetWorld() ? Cast<AKnightMenuGameMode>(GetWorld()->GetAuthGameMode()) : nullptr;
	if (!MenuMode)
	{
		UE_LOG(Logknight, Error, TEXT("Menu PlayerController started without a Knight menu GameMode."));
		return;
	}

	const EKnightFlowState MenuState = MenuMode->GetMenuFlowState();
	UKnightGameFlowSubsystem* const Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>() : nullptr;
	if (!Flow || (MenuState != EKnightFlowState::Title && MenuState != EKnightFlowState::MainMenu)
		|| Flow->GetCurrentFlowState() != MenuState)
	{
		UE_LOG(Logknight, Error, TEXT("Menu PlayerController cannot create UI: GameMode flow state was not initialized correctly."));
		return;
	}

	UClass* const WidgetClass = MenuMode->GetMenuWidgetClass();
	if (!WidgetClass)
	{
		UE_LOG(Logknight, Warning, TEXT("%s has no MenuWidgetClass yet; no menu UI was created."), *MenuMode->GetClass()->GetName());
		return;
	}
	const bool bCorrectWidgetType = MenuState == EKnightFlowState::Title
		? WidgetClass->IsChildOf(UKnightTitleMenuWidget::StaticClass())
		: WidgetClass->IsChildOf(UKnightMainMenuWidget::StaticClass());
	if (!bCorrectWidgetType)
	{
		UE_LOG(Logknight, Error, TEXT("%s has an incompatible MenuWidgetClass: %s."),
			*MenuMode->GetClass()->GetName(), *WidgetClass->GetName());
		return;
	}

	MenuWidget = CreateWidget<UKnightMenuWidgetBase>(this, WidgetClass);
	if (!MenuWidget)
	{
		UE_LOG(Logknight, Error, TEXT("Failed to create menu widget %s."), *WidgetClass->GetName());
		return;
	}
	MenuWidget->AddToViewport();
	ApplyMenuInputMode();
	GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::ApplyMenuInputMode);
}

void AKnightMenuPlayerController::ApplyMenuInputMode()
{
	if (!IsLocalController() || !MenuWidget || !MenuWidget->IsInViewport()) return;
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
	MenuWidget->FocusCurrentPage();
}

void AKnightMenuPlayerController::KnightCoreUseFixedSlot(int32 SlotNumber)
{
#if WITH_EDITOR
	UKnightTitleMenuWidget* const TitleWidget = Cast<UKnightTitleMenuWidget>(MenuWidget);
	if (!TitleWidget || !TitleWidget->ShowPage(EKnightTitlePage::SaveSelect)) return;
	TitleWidget->SelectFixedSlot(SlotNumber);
	const UknightSaveSubsystem* const Save = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	UE_LOG(Logknight, Log, TEXT("KnightCoreUseFixedSlot slot=%d active=%s loadFailed=%d busy=%d"), SlotNumber,
		Save ? *Save->GetActiveSlotId() : TEXT("none"), Save && Save->DidLoadFail(), Save && Save->IsSaveInFlight());
#endif
}

void AKnightMenuPlayerController::KnightCoreReturnTitle()
{
#if WITH_EDITOR
	UKnightGameFlowSubsystem* const Flow = GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>();
	UE_LOG(Logknight, Log, TEXT("KnightCoreReturnTitle accepted=%d"), Flow && Flow->RequestOpenTitle());
#endif
}

void AKnightMenuPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MenuWidget)
	{
		MenuWidget->RemoveFromParent();
		MenuWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AKnightMenuPlayerController::KnightCoreLaunchTwice(FName StageId)
{
#if WITH_EDITOR
	UKnightGameFlowSubsystem* const Flow = GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>();
	const bool bFirst = Flow && Flow->RequestStartStage(StageId);
	const bool bSecond = Flow && Flow->RequestStartStage(StageId);
	UE_LOG(Logknight, Log, TEXT("KnightCoreLaunchTwice stage=%s first=%d second=%d pending=%d"),
		*StageId.ToString(), bFirst, bSecond, Flow && Flow->IsTravelPending());
#endif
}

void AKnightMenuPlayerController::KnightCoreLaunch(FName StageId)
{
#if WITH_EDITOR
	UKnightGameFlowSubsystem* const Flow = GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>();
	UE_LOG(Logknight, Log, TEXT("KnightCoreLaunch stage=%s accepted=%d"),
		*StageId.ToString(), Flow && Flow->RequestStartStage(StageId));
#endif
}

void AKnightMenuPlayerController::KnightCoreLaunchQuestTwice(FName QuestId)
{
#if WITH_EDITOR
	UKnightGameFlowSubsystem* const Flow = GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>();
	FText FirstReason;
	FText SecondReason;
	const bool bFirst = Flow && Flow->RequestStartQuest(QuestId, FirstReason);
	const bool bSecond = Flow && Flow->RequestStartQuest(QuestId, SecondReason);
	UE_LOG(Logknight, Log, TEXT("KnightCoreLaunchQuestTwice quest=%s first=%d second=%d pending=%d firstReason=%s secondReason=%s"),
		*QuestId.ToString(), bFirst, bSecond, Flow && Flow->IsTravelPending(),
		*FirstReason.ToString(), *SecondReason.ToString());
#endif
}

void AKnightMenuPlayerController::KnightCoreLaunchQuest(FName QuestId)
{
#if WITH_EDITOR
	UKnightGameFlowSubsystem* const Flow = GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>();
	FText Reason;
	const bool bAccepted = Flow && Flow->RequestStartQuest(QuestId, Reason);
	UE_LOG(Logknight, Log, TEXT("KnightCoreLaunchQuest quest=%s accepted=%d pending=%d reason=%s"),
		*QuestId.ToString(), bAccepted, Flow && Flow->IsTravelPending(), *Reason.ToString());
#endif
}

void AKnightMenuPlayerController::KnightCoreQuestStatus()
{
#if WITH_EDITOR
	const UKnightGameFlowSubsystem* const Flow = GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>();
	const UknightRaidSubsystem* const Raid = GetGameInstance()->GetSubsystem<UknightRaidSubsystem>();
	FKnightQuestRunContext Context;
	const bool bHasContext = Raid && Raid->GetActiveQuestRunContext(Context);
	UE_LOG(Logknight, Log, TEXT("KnightCoreQuestStatus flow=%d pending=%d hasContext=%d quest=%s stage=%s boss=%s difficulty=%d run=%s"),
		Flow ? static_cast<int32>(Flow->GetCurrentFlowState()) : -1, Flow && Flow->IsTravelPending(), bHasContext,
		*Context.QuestId.ToString(), *Context.StageId.ToString(), *Context.BossId.ToString(),
		static_cast<int32>(Context.Difficulty), *Context.RunId.ToString());
#endif
}

void AKnightMenuPlayerController::KnightCoreFailNextRaidLoad()
{
#if WITH_EDITOR
	if (UKnightGameFlowSubsystem* const Flow = GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>())
	{
		Flow->DebugFailNextRaidLoad();
		UE_LOG(Logknight, Log, TEXT("KnightCoreFailNextRaidLoad armed"));
	}
#endif
}

void AKnightMenuPlayerController::KnightCoreProfileStatus()
{
#if WITH_EDITOR
	const UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	const UknightSaveGame* const Save = Saves ? Saves->GetSaveGame() : nullptr;
	const int32 MaterialCount = Save ? Save->ItemCounts.FindRef(FPrimaryAssetId(TEXT("Item"), TEXT("DA_CoreTestMaterial"))) : -1;
	UE_LOG(Logknight, Log, TEXT("KnightCoreProfileStatus slot=%s exists=%d loadFailed=%d busy=%d material=%d upgrades=%d unlocks=%d runs=%d"),
		Saves ? *Saves->GetActiveSlotId() : TEXT("None"), Saves && Saves->DoesSlotExist(Saves->GetActiveSlotId()),
		Saves && Saves->DidLoadFail(), Saves && Saves->IsSaveInFlight(), MaterialCount,
		Save ? Save->ItemUpgradeLevels.Num() : -1, Save ? Save->UnlockedStages.Num() : -1,
		Save ? Save->AppliedRaidRuns.Num() : -1);
#endif
}

namespace
{
	bool IsIsolatedRewardProbe()
	{
#if WITH_EDITOR
		FString Namespace;
		return FParse::Value(FCommandLine::Get(), TEXT("KnightSaveTestNamespace="), Namespace) && !Namespace.IsEmpty();
#else
		return false;
#endif
	}
}

void AKnightMenuPlayerController::KnightCoreRewardTestSlot(int32 SlotNumber)
{
#if WITH_EDITOR
	if (!IsIsolatedRewardProbe()) return;
	UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	const bool bSelected = Saves && Saves->SelectFixedSlot(SlotNumber);
	UE_LOG(Logknight, Log, TEXT("KnightCoreRewardTestSlot requested=%d selected=%d active=%s"), SlotNumber,
		bSelected, Saves ? *Saves->GetActiveSlotId() : TEXT("None"));
#endif
}

void AKnightMenuPlayerController::KnightCoreRewardFailNextSaves(int32 Count)
{
#if WITH_EDITOR
	if (!IsIsolatedRewardProbe()) return;
	if (UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>())
	{
		Saves->DebugFailNextSaves(Count);
		UE_LOG(Logknight, Log, TEXT("KnightCoreRewardFailNextSaves count=%d"), Count);
	}
#endif
}

void AKnightMenuPlayerController::KnightCoreRewardResult(FName QuestId, int32 bSuccess, int32 bHead, int32 bTail)
{
#if WITH_EDITOR
	if (!IsIsolatedRewardProbe()) return;
	UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	UknightRaidSubsystem* const Raid = GetGameInstance()->GetSubsystem<UknightRaidSubsystem>();
	const UKnightQuestCatalog* const Catalog = UKnightQuestSettings::LoadCatalog();
	const UKnightQuestDefinition* const Quest = Catalog ? Catalog->FindQuest(QuestId) : nullptr;
	if (!Saves || !Saves->HasSelectedSlot() || Saves->IsSaveInFlight() || !Raid || !Quest
		|| !Quest->bContentEnabled || Quest->StageId.IsNone() || Quest->BossId.IsNone()
		|| Raid->GetCurrentRunId().IsValid())
	{
		UE_LOG(Logknight, Error, TEXT("KnightCoreRewardResult refused quest=%s"), *QuestId.ToString());
		return;
	}
	FknightRaidResult Result;
	Result.RunId = Raid->BeginRun(Quest->StageId, Quest->QuestId, Quest->BossId, Quest->Difficulty,
		Saves->GetActiveSlotId());
	const int32 OtherSlot = Saves->GetActiveFixedSlotNumber() == 1 ? 2 : 1;
	const bool bCrossSlotAccepted = Saves->SelectFixedSlot(OtherSlot);
	UE_LOG(Logknight, Log, TEXT("KnightCoreRewardResult active-run cross-slot switch accepted=%d"), bCrossSlotAccepted);
	Result.StageId = Quest->StageId;
	Result.bSuccess = bSuccess != 0;
	for (const FKnightQuestPartBreakReward& Entry : Quest->PartBreakRewards)
	{
		if ((Entry.Part == EKnightQuestBreakPart::Head && bHead != 0)
			|| (Entry.Part == EKnightQuestBreakPart::Tail && bTail != 0))
		{
			Result.BrokenParts.AddTag(Entry.Reward.RequiredBrokenPart);
		}
	}
	const bool bAccepted = Raid->RecordResult(Result);
	const bool bDuplicateAccepted = Raid->RecordResult(Result);
	FknightRaidResult Settled;
	Raid->GetLastResult(Settled);
	UE_LOG(Logknight, Log, TEXT("KnightCoreRewardResult quest=%s run=%s success=%d head=%d tail=%d accepted=%d duplicateAccepted=%d itemKinds=%d money=%lld sockets=%d"),
		*QuestId.ToString(), *Result.RunId.ToString(), Result.bSuccess, bHead, bTail, bAccepted, bDuplicateAccepted,
		Settled.Rewards.Num(), Settled.MoneyEarned, Settled.FirstBossSocketSlotsEarned);
	for (const FknightItemStack& Stack : Settled.Rewards)
	{
		UE_LOG(Logknight, Log, TEXT("KnightCoreRewardItem item=%s count=%d"),
			Stack.Item ? *Stack.Item->GetPrimaryAssetId().ToString() : TEXT("None"), Stack.Count);
	}
	Raid->EndRun();
#endif
}

void AKnightMenuPlayerController::KnightCoreRewardStatus()
{
#if WITH_EDITOR
	if (!IsIsolatedRewardProbe()) return;
	const UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	const UknightSaveGame* const Save = Saves && Saves->HasSelectedSlot() ? Saves->GetSaveGame() : nullptr;
	if (!Save) return;
	UE_LOG(Logknight, Log, TEXT("KnightCoreRewardStatus slot=%s money=%lld sockets=%d firstBosses=%d completedQuests=%d appliedRuns=%d saveBusy=%d"),
		*Saves->GetActiveSlotId(), Save->Money, Save->UnlockedSharedSocketSlots,
		Save->FirstDefeatedBossIds.Num(), Save->CompletedQuestIds.Num(), Save->AppliedRaidRuns.Num(), Saves->IsSaveInFlight());
	for (const TPair<FPrimaryAssetId, int32>& Entry : Save->ItemCounts)
	{
		UE_LOG(Logknight, Log, TEXT("KnightCoreRewardOwned item=%s count=%d"), *Entry.Key.ToString(), Entry.Value);
	}
#endif
}
