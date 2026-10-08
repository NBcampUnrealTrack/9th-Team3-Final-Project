#include "KnightMainMenuWidget.h"

#include "GameFlow/KnightGameFlowSubsystem.h"
#include "Engine/GameInstance.h"
#include "KnightStageEntryWidget.h"
#include "Raid/knightRaidSubsystem.h"
#include "Progression/knightSaveSubsystem.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "knight.h"
#include "Settings/KnightSettingsWidget.h"
#include "Codex/KnightCodexWidget.h"

#define LOCTEXT_NAMESPACE "KnightMainMenu"

void UKnightMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ValidateRequiredWidget(Panel_Home, TEXT("Panel_Home"));
	ValidateRequiredWidget(Panel_Stage, TEXT("Panel_Stage"));
	ValidateRequiredWidget(Panel_StageDetails, TEXT("Panel_StageDetails"));
	ValidateRequiredWidget(Box_StageEntries, TEXT("Box_StageEntries"));
	ValidateRequiredWidget(Text_StageTitle, TEXT("Text_StageTitle"));
	ValidateRequiredWidget(Text_StageDescription, TEXT("Text_StageDescription"));
	ValidateRequiredWidget(Button_LaunchStage, TEXT("Button_LaunchStage"));
	ValidateRequiredWidget(Panel_Maintenance, TEXT("Panel_Maintenance"));
	ValidateRequiredWidget(Panel_Settings, TEXT("Panel_Settings"));
	ValidateRequiredWidget(Panel_Codex, TEXT("Panel_Codex"));
	ValidateRequiredWidget(Button_Stage, TEXT("Button_Stage"));
	ValidateRequiredWidget(Text_ActiveProfile, TEXT("Text_ActiveProfile"));
	ValidateRequiredWidget(Button_ReturnToTitle, TEXT("Button_ReturnToTitle"));
	ValidateRequiredWidget(Button_Maintenance, TEXT("Button_Maintenance"));
	ValidateRequiredWidget(Button_Settings, TEXT("Button_Settings"));
	ValidateRequiredWidget(Button_Codex, TEXT("Button_Codex"));
	if (Button_Stage) Button_Stage->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStageClicked);
	if (Button_ReturnToTitle) Button_ReturnToTitle->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleReturnToTitleClicked);
	if (Button_Maintenance) Button_Maintenance->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMaintenanceClicked);
	if (Button_Settings) Button_Settings->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSettingsClicked);
	if (Button_Codex) Button_Codex->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCodexClicked);
	if (Button_LaunchStage) Button_LaunchStage->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLaunchStageClicked);
}

void UKnightMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyPage(EKnightMainMenuPage::Home, true);
	const UknightSaveSubsystem* const Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	if (Text_ActiveProfile)
	{
		const int32 SlotNumber = Save ? Save->GetActiveFixedSlotNumber() : 0;
		Text_ActiveProfile->SetText(SlotNumber > 0
			? FText::Format(LOCTEXT("ActiveSlot", "세이브 {0}"), FText::AsNumber(SlotNumber))
			: LOCTEXT("NoActiveSlot", "세이브 없음"));
	}
	UknightRaidSubsystem* const Session = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightRaidSubsystem>() : nullptr;
	FknightRaidResult Result;
	if (Session && Session->GetLastResult(Result))
	{
		int32 RewardCount = 0;
		for (const FknightItemStack& Reward : Result.Rewards)
		{
			RewardCount += FMath::Max(0, Reward.Count);
		}
		SetStatusText(Result.bSuccess
			? FText::Format(LOCTEXT("RaidSuccessResult", "레이드 성공: {0} | 보상 {1}개"),
				FText::FromName(Result.StageId), FText::AsNumber(RewardCount))
			: FText::Format(LOCTEXT("RaidFailedResult", "레이드 실패: {0}"), FText::FromName(Result.StageId)));
		Session->ClearLastResult();
	}
}

void UKnightMainMenuWidget::NativeDestruct()
{
	if (SettingsWidget)
	{
		SettingsWidget->OnClosed.RemoveDynamic(this, &ThisClass::HandleSettingsClosed);
		SettingsWidget->RemoveFromParent();
	}
	if (CodexWidget) CodexWidget->OnClosed.RemoveDynamic(this, &ThisClass::HandleCodexClosed);
	RemoveStageEntries();
	Super::NativeDestruct();
}

bool UKnightMainMenuWidget::ShowPage(EKnightMainMenuPage NewPage)
{
	return ApplyPage(NewPage, false);
}

bool UKnightMainMenuWidget::ApplyPage(EKnightMainMenuPage NewPage, bool bAllowDuringTravel)
{
	if (!bAllowDuringTravel && IsTravelPending())
	{
		return false;
	}
	if (NewPage == EKnightMainMenuPage::Stage && !bStageEntriesBuilt)
	{
		RebuildStageEntries();
	}
	if (NewPage == EKnightMainMenuPage::StageDetails && !RefreshStageDetails())
	{
		return false;
	}
	UPanelWidget* Page = nullptr;
	switch (NewPage)
	{
	case EKnightMainMenuPage::Home: Page = Panel_Home; break;
	case EKnightMainMenuPage::Stage: Page = Panel_Stage; break;
	case EKnightMainMenuPage::StageDetails: Page = Panel_StageDetails; break;
	case EKnightMainMenuPage::Maintenance: Page = Panel_Maintenance; break;
	case EKnightMainMenuPage::Settings: Page = Panel_Settings; break;
	case EKnightMainMenuPage::Codex: Page = Panel_Codex; break;
	default:
		UE_LOG(Logknight, Error, TEXT("Invalid main menu page %d."), static_cast<int32>(NewPage));
		return false;
	}
	if (!ActivatePage(Page))
	{
		return false;
	}
	CurrentPage = NewPage;
	if (NewPage != EKnightMainMenuPage::Home) bRestoreCodexFocus = false;
	if (NewPage == EKnightMainMenuPage::Settings)
	{
		EnsureSettingsWidget();
		// The settings screen is laid out for the whole screen, so it goes over the menu rather than into Panel_Settings.
		if (SettingsWidget && !SettingsWidget->IsInViewport()) SettingsWidget->AddToViewport(50);
		if (SettingsWidget) SettingsWidget->Open();
	}
	else if (SettingsWidget && SettingsWidget->IsInViewport())
	{
		SettingsWidget->RemoveFromParent();
	}
	if (NewPage == EKnightMainMenuPage::Codex)
	{
		EnsureCodexWidget();
		if (CodexWidget) CodexWidget->Open();
	}
	UpdateBackVisibility(NewPage == EKnightMainMenuPage::Home || NewPage == EKnightMainMenuPage::Settings);
	RefreshTravelControls();
	if (!IsTravelPending() && GetFlow() && GetFlow()->GetCurrentFlowState() == EKnightFlowState::MainMenu)
	{
		ClearStatus();
	}
	if (NewPage == EKnightMainMenuPage::StageDetails)
	{
		RefreshStageDetails();
	}
	else if (NewPage == EKnightMainMenuPage::Stage && !bStageEntriesBuilt)
	{
		SetStatusText(LOCTEXT("NoStages", "표시할 스테이지가 없습니다. 카탈로그 설정을 확인해 주세요."));
	}
	FocusCurrentPage();
	if (NewPage == EKnightMainMenuPage::Settings && SettingsWidget) SettingsWidget->FocusSettings();
	if (NewPage == EKnightMainMenuPage::Codex && CodexWidget && CodexWidget->GetFocusButton())
	{
		CodexWidget->GetFocusButton()->SetUserFocus(GetOwningPlayer());
	}
	return true;
}

bool UKnightMainMenuWidget::HandleBackRequested()
{
	if (CurrentPage == EKnightMainMenuPage::Settings && SettingsWidget) { SettingsWidget->RequestClose(); return true; }
	if (CurrentPage == EKnightMainMenuPage::Codex) { HandleCodexClosed(); return true; }
	if (CurrentPage == EKnightMainMenuPage::StageDetails)
	{
		return ShowPage(EKnightMainMenuPage::Stage);
	}
	return CurrentPage == EKnightMainMenuPage::Home ? ShowPage(EKnightMainMenuPage::Settings) : ShowPage(EKnightMainMenuPage::Home);
}

bool UKnightMainMenuWidget::ShouldAllowEscapeToChild() const
{
	return CurrentPage == EKnightMainMenuPage::Settings && SettingsWidget && SettingsWidget->IsCapturingKey();
}

void UKnightMainMenuWidget::EnsureSettingsWidget()
{
	if (SettingsWidget || !Panel_Settings || !GetOwningPlayer()) return;
	SettingsWidget = CreateWidget<UKnightSettingsWidget>(GetOwningPlayer(), UKnightSettingsWidget::GetSharedWidgetClass());
	if (!SettingsWidget) return;
	// The page stays as the menu's "settings" state but shows nothing; its placeholder would show through.
	Panel_Settings->ClearChildren();
	SettingsWidget->OnClosed.AddUniqueDynamic(this, &ThisClass::HandleSettingsClosed);
}

void UKnightMainMenuWidget::HandleSettingsClosed() { ShowPage(EKnightMainMenuPage::Home); }

void UKnightMainMenuWidget::EnsureCodexWidget()
{
	if (CodexWidget || !Panel_Codex || !GetOwningPlayer()) return;
	CodexWidget = CreateWidget<UKnightCodexWidget>(GetOwningPlayer(), UKnightCodexWidget::GetSharedWidgetClass());
	if (!CodexWidget) return;
	Panel_Codex->ClearChildren();
	UPanelSlot* const AddedSlot = Panel_Codex->AddChild(CodexWidget);
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(AddedSlot))
	{
		CanvasSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		CanvasSlot->SetOffsets(FMargin(0, 0, 0, 0));
	}
	else if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(AddedSlot))
	{
		VerticalSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	else if (UHorizontalBoxSlot* HorizontalSlot = Cast<UHorizontalBoxSlot>(AddedSlot))
	{
		HorizontalSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	CodexWidget->OnClosed.AddUniqueDynamic(this, &ThisClass::HandleCodexClosed);
}

void UKnightMainMenuWidget::HandleCodexClosed()
{
	bRestoreCodexFocus = true;
	ShowPage(EKnightMainMenuPage::Home);
}

bool UKnightMainMenuWidget::IsCodexButtonFocused() const
{
	return Button_Codex && GetOwningPlayer() && Button_Codex->HasUserFocus(GetOwningPlayer());
}

UWidget* UKnightMainMenuWidget::GetPreferredFocusTarget() const
{
	if (CurrentPage == EKnightMainMenuPage::Home && bRestoreCodexFocus && Button_Codex)
	{
		return Button_Codex;
	}
	if (CurrentPage == EKnightMainMenuPage::Codex && CodexWidget)
	{
		return CodexWidget->GetFocusButton();
	}
	if (CurrentPage == EKnightMainMenuPage::StageDetails && Button_LaunchStage
		&& Button_LaunchStage->GetIsFocusable() && Button_LaunchStage->IsVisible() && Button_LaunchStage->GetIsEnabled())
	{
		return Button_LaunchStage;
	}
	if (CurrentPage == EKnightMainMenuPage::Stage)
	{
		for (const UKnightStageEntryWidget* Entry : GeneratedStageEntries)
		{
			UButton* const SelectButton = Entry ? Entry->GetSelectionButton() : nullptr;
			if (SelectButton && SelectButton->GetIsFocusable() && SelectButton->IsVisible() && SelectButton->GetIsEnabled())
			{
				return SelectButton;
			}
		}
	}
	if (CurrentPage != EKnightMainMenuPage::Home)
	{
		return Button_Back;
	}
	for (UButton* Button : { Button_Stage.Get(), Button_Maintenance.Get(), Button_Settings.Get(), Button_Codex.Get(), Button_ReturnToTitle.Get() })
	{
		if (Button && Button->GetIsFocusable() && Button->IsVisible() && Button->GetIsEnabled())
		{
			return Button;
		}
	}
	return nullptr;
}

void UKnightMainMenuWidget::SetTravelControlsEnabled(bool bEnabled)
{
	Super::SetTravelControlsEnabled(bEnabled);
	if (Button_Stage) Button_Stage->SetIsEnabled(bEnabled);
	if (Button_ReturnToTitle) Button_ReturnToTitle->SetIsEnabled(bEnabled);
	if (Button_Maintenance) Button_Maintenance->SetIsEnabled(bEnabled);
	if (Button_Settings) Button_Settings->SetIsEnabled(bEnabled);
	if (Button_Codex) Button_Codex->SetIsEnabled(bEnabled);
	if (Button_LaunchStage) Button_LaunchStage->SetIsEnabled(bEnabled && bStageLaunchAvailable && CurrentPage == EKnightMainMenuPage::StageDetails);
	for (UKnightStageEntryWidget* Entry : GeneratedStageEntries)
	{
		if (Entry) Entry->SetSelectionEnabled(bEnabled);
	}
}

void UKnightMainMenuWidget::HandleStageClicked()
{
	ShowPage(EKnightMainMenuPage::Stage);
}

void UKnightMainMenuWidget::HandleReturnToTitleClicked()
{
	if (UKnightGameFlowSubsystem* const Flow = GetFlow()) Flow->RequestOpenTitle();
}

void UKnightMainMenuWidget::HandleMaintenanceClicked()
{
	ShowPage(EKnightMainMenuPage::Maintenance);
}

void UKnightMainMenuWidget::HandleSettingsClicked()
{
	ShowPage(EKnightMainMenuPage::Settings);
}

void UKnightMainMenuWidget::HandleCodexClicked()
{
	ShowPage(EKnightMainMenuPage::Codex);
}

void UKnightMainMenuWidget::RemoveStageEntries()
{
	for (UKnightStageEntryWidget* Entry : GeneratedStageEntries)
	{
		if (Entry)
		{
			Entry->OnStageSelected.RemoveDynamic(this, &ThisClass::HandleStageEntrySelected);
			Entry->RemoveFromParent();
		}
	}
	GeneratedStageEntries.Reset();
	bStageEntriesBuilt = false;
}

bool UKnightMainMenuWidget::RebuildStageEntries()
{
	RemoveStageEntries();
	if (!Box_StageEntries || !StageEntryWidgetClass || !StageEntryWidgetClass->IsChildOf(UKnightStageEntryWidget::StaticClass()))
	{
		UE_LOG(Logknight, Error, TEXT("%s requires Box_StageEntries and a valid StageEntryWidgetClass."), *GetClass()->GetName());
		SetStatusText(LOCTEXT("EntrySetupMissing", "스테이지 목록 화면 설정을 확인해 주세요."));
		return false;
	}
	UKnightGameFlowSubsystem* const Flow = GetFlow();
	if (!Flow || Flow->GetCurrentFlowState() != EKnightFlowState::MainMenu)
	{
		UE_LOG(Logknight, Error, TEXT("Cannot build stage entries without an active MainMenu GameFlow."));
		SetStatusText(LOCTEXT("NoFlowForStages", "스테이지 목록을 열 수 없습니다."));
		return false;
	}
	TArray<FKnightStageDefinition> Stages;
	if (!Flow->GetStageDefinitions(Stages))
	{
		SetStatusText(LOCTEXT("NoValidStages", "표시할 스테이지가 없습니다. 카탈로그 설정을 확인해 주세요."));
		return false;
	}
	APlayerController* const OwningPlayer = GetOwningPlayer();
	if (!OwningPlayer)
	{
		UE_LOG(Logknight, Error, TEXT("Cannot build stage entries without an owning PlayerController."));
		return false;
	}
	for (const FKnightStageDefinition& Stage : Stages)
	{
		UKnightStageEntryWidget* const Entry = CreateWidget<UKnightStageEntryWidget>(OwningPlayer, StageEntryWidgetClass);
		if (!Entry)
		{
			UE_LOG(Logknight, Error, TEXT("Could not create stage entry for %s using %s."), *Stage.StageId.ToString(), *StageEntryWidgetClass->GetName());
			continue;
		}
		Entry->SetStage(Stage.StageId, Stage.bDevelopmentOnly
			? FText::Format(LOCTEXT("DevStageName", "[DEV] {0}"), Stage.DisplayName) : Stage.DisplayName);
		Entry->OnStageSelected.AddUniqueDynamic(this, &ThisClass::HandleStageEntrySelected);
		if (!Box_StageEntries->AddChildToVerticalBox(Entry))
		{
			Entry->OnStageSelected.RemoveDynamic(this, &ThisClass::HandleStageEntrySelected);
			UE_LOG(Logknight, Error, TEXT("Could not add stage entry %s to Box_StageEntries."), *Stage.StageId.ToString());
			continue;
		}
		GeneratedStageEntries.Add(Entry);
	}
	bStageEntriesBuilt = !GeneratedStageEntries.IsEmpty();
	return bStageEntriesBuilt;
}

bool UKnightMainMenuWidget::SelectStage(FName StageId)
{
	UKnightGameFlowSubsystem* const Flow = GetFlow();
	if (!Flow || Flow->GetCurrentFlowState() != EKnightFlowState::MainMenu || IsTravelPending())
	{
		SetStatusText(LOCTEXT("CannotSelectNow", "지금 스테이지를 선택할 수 없습니다."));
		return false;
	}
	FKnightStageDefinition Stage;
	if (!Flow->FindStageDefinition(StageId, Stage))
	{
		SetStatusText(LOCTEXT("UnknownStage", "스테이지 정보를 확인할 수 없습니다."));
		return false;
	}
	const FName PreviousSelection = SelectedStageId;
	SelectedStageId = StageId;
	if (!ShowPage(EKnightMainMenuPage::StageDetails))
	{
		SelectedStageId = PreviousSelection;
		return false;
	}
	return true;
}

bool UKnightMainMenuWidget::RefreshStageDetails()
{
	UKnightGameFlowSubsystem* const Flow = GetFlow();
	FKnightStageDefinition Stage;
	if (!Flow || !Flow->FindStageDefinition(SelectedStageId, Stage))
	{
		bStageLaunchAvailable = false;
		if (Button_LaunchStage) Button_LaunchStage->SetIsEnabled(false);
		SetStatusText(LOCTEXT("NoSelectedStage", "유효한 스테이지를 먼저 선택해 주세요."));
		return false;
	}
	if (Text_StageTitle) Text_StageTitle->SetText(Stage.DisplayName);
	if (Text_StageDescription) Text_StageDescription->SetText(Stage.Description);
	FText Reason;
	bStageLaunchAvailable = Flow->CanLaunchStage(SelectedStageId, Reason);
	if (Button_LaunchStage) Button_LaunchStage->SetIsEnabled(bStageLaunchAvailable && !IsTravelPending());
	if (!bStageLaunchAvailable) SetStatusText(Reason);
	return true;
}

void UKnightMainMenuWidget::HandleStageEntrySelected(FName StageId)
{
	SelectStage(StageId);
}

void UKnightMainMenuWidget::HandleLaunchStageClicked()
{
	UKnightGameFlowSubsystem* const Flow = GetFlow();
	if (!Flow || IsTravelPending() || !bStageLaunchAvailable)
	{
		return;
	}
	const bool bAccepted = Flow->RequestStartStage(SelectedStageId);
	RefreshTravelControls();
	if (bAccepted && Flow->IsTravelPending())
	{
		SetStatusText(LOCTEXT("LaunchingStage", "스테이지로 이동 중입니다."));
	}
}

#undef LOCTEXT_NAMESPACE
