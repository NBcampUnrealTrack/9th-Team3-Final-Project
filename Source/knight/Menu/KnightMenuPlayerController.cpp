#include "KnightMenuPlayerController.h"

#include "KnightMenuGameMode.h"
#include "UI/KnightMainMenuWidget.h"
#include "UI/KnightMenuWidgetBase.h"
#include "UI/KnightTitleMenuWidget.h"
#include "GameFlow/KnightGameFlowSubsystem.h"
#include "Progression/knightSaveGame.h"
#include "Progression/knightSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "knight.h"

AKnightMenuPlayerController::AKnightMenuPlayerController()
{
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
