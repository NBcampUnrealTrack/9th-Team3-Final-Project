#include "KnightMainMenuWidget.h"

#include "GameFlow/KnightGameFlowSubsystem.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "KnightStageMapWidget.h"
#include "Monster/knightMonsterDefinition.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Raid/knightRaidDefinition.h"
#include "Raid/knightRaidSubsystem.h"
#include "Progression/knightSaveSubsystem.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "knight.h"
#include "Settings/KnightSettingsWidget.h"
#include "Codex/KnightCodexWidget.h"
#include "Menu/KnightMaintenanceCameraRig.h"
#include "Menu/UI/KnightMaintenanceWidget.h"
#include "Menu/UI/KnightInventoryWidget.h"
#include "Menu/UI/KnightQuestMenuWidget.h"
#include "Quest/KnightQuestData.h"
#include "System/knightGameplayTags.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "EngineUtils.h"

#define LOCTEXT_NAMESPACE "KnightMainMenu"

namespace
{
	void SetMaintenanceButtonLabel(UCommonButtonBase* Button, const FText& Label)
	{
		if (!Button) return;
		if (UFunction* const Function = Button->FindFunction(TEXT("SetButtonText")))
		{
			struct FParams { FText Text; } Params { Label };
			Button->ProcessEvent(Function, &Params);
		}
	}
}

void UKnightMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ValidateRequiredWidget(Panel_Home, TEXT("Panel_Home"));
	ValidateRequiredWidget(Panel_Stage, TEXT("Panel_Stage"));
	ValidateRequiredWidget(Map_Stages, TEXT("Map_Stages"));
	ValidateRequiredWidget(Panel_StageInfo, TEXT("Panel_StageInfo"));
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
	if (Button_Stage) Button_Stage->OnClicked().AddUObject(this, &ThisClass::HandleStageClicked);
	if (Button_ReturnToTitle) Button_ReturnToTitle->OnClicked().AddUObject(this, &ThisClass::HandleReturnToTitleClicked);
	if (Button_Maintenance) Button_Maintenance->OnClicked().AddUObject(this, &ThisClass::HandleMaintenanceClicked);
	if (Button_Settings) Button_Settings->OnClicked().AddUObject(this, &ThisClass::HandleSettingsClicked);
	if (Button_Codex) Button_Codex->OnClicked().AddUObject(this, &ThisClass::HandleCodexClicked);
	if (Button_LaunchStage) Button_LaunchStage->OnClicked().AddUObject(this, &ThisClass::HandleLaunchStageClicked);
	if (Map_Stages)
	{
		Map_Stages->OnStageClicked.AddUniqueDynamic(this, &ThisClass::HandleMapStageClicked);
		Map_Stages->OnQuestClicked.AddUniqueDynamic(this, &ThisClass::HandleMapQuestClicked);
		Map_Stages->SetLeaderTarget(Panel_StageInfo);
	}
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
			? FText::Format(LOCTEXT("ActiveSlot", "저장 {0}"), FText::AsNumber(SlotNumber))
			: LOCTEXT("NoActiveSlot", "저장 파일 없음"));
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
			? FText::Format(LOCTEXT("RaidSuccessResult", "레이드 성공: {0} | 아이템 {1}개 | 돈 +{2} | 소켓 +{3}"),
				FText::FromName(Result.StageId), FText::AsNumber(RewardCount),
				FText::AsNumber(Result.MoneyEarned), FText::AsNumber(Result.FirstBossSocketSlotsEarned))
			: FText::Format(LOCTEXT("RaidFailedResult", "레이드 실패: {0}"), FText::FromName(Result.StageId)));
		Session->ClearLastResult();
	}
}

void UKnightMainMenuWidget::NativeDestruct()
{
	if (InventoryWidget) InventoryWidget->Close();
	if (MaintenanceWidget) MaintenanceWidget->Close();
	if (MaintenanceCameraRig.IsValid()) MaintenanceCameraRig->SetMaintenanceView(false);
	if (SettingsWidget)
	{
		SettingsWidget->OnClosed.RemoveDynamic(this, &ThisClass::HandleSettingsClosed);
		SettingsWidget->RemoveFromParent();
	}
	if (CodexWidget) CodexWidget->OnClosed.RemoveDynamic(this, &ThisClass::HandleCodexClosed);
	if (RaidDefinitionHandle.IsValid())
	{
		RaidDefinitionHandle->CancelHandle();
		RaidDefinitionHandle.Reset();
	}
	if (QuestWidget)
	{
		QuestWidget->OnLaunchRequested.RemoveAll(this);
		QuestWidget->OnSelectionChanged.RemoveAll(this);
	}
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
	UPanelWidget* Page = nullptr;
	if (NewPage == EKnightMainMenuPage::Inventory) EnsureInventoryWidget();
	switch (NewPage)
	{
	case EKnightMainMenuPage::Home: Page = Panel_Home; break;
	case EKnightMainMenuPage::Stage: Page = Panel_Stage; break;
	case EKnightMainMenuPage::Maintenance: Page = Panel_Maintenance; break;
	case EKnightMainMenuPage::Settings: Page = Panel_Settings; break;
	case EKnightMainMenuPage::Codex: Page = Panel_Codex; break;
	case EKnightMainMenuPage::Inventory: Page = InventoryPanel; break;
	default:
		UE_LOG(Logknight, Error, TEXT("Invalid main menu page %d."), static_cast<int32>(NewPage));
		return false;
	}
	if (!ActivatePage(Page))
	{
		return false;
	}
	const bool bEnteringStage = NewPage == EKnightMainMenuPage::Stage && CurrentPage != EKnightMainMenuPage::Stage;
	const bool bLeavingInventory = CurrentPage == EKnightMainMenuPage::Inventory && NewPage != CurrentPage;
	if (bLeavingInventory)
	{
		if (InventoryWidget) InventoryWidget->Close();
		bRestoreInventoryFocus = NewPage == EKnightMainMenuPage::Home;
	}
	const bool bLeavingMaintenance = CurrentPage == EKnightMainMenuPage::Maintenance && NewPage != CurrentPage;
	if (bLeavingMaintenance)
	{
		if (MaintenanceWidget) MaintenanceWidget->Close();
		if (MaintenanceCameraRig.IsValid()) MaintenanceCameraRig->SetMaintenanceView(false);
		bRestoreMaintenanceFocus = NewPage == EKnightMainMenuPage::Home;
	}
	CurrentPage = NewPage;
	if (NewPage != EKnightMainMenuPage::Home) bRestoreCodexFocus = false;
	if (NewPage != EKnightMainMenuPage::Home) bRestoreMaintenanceFocus = false;
	if (NewPage != EKnightMainMenuPage::Home) bRestoreInventoryFocus = false;
	bool bHasStages = true;
	if (bEnteringStage)
	{
		SelectedStageId = NAME_None;
		if (Map_Stages) { Map_Stages->SetSelectedStage(NAME_None); Map_Stages->SetSelectedQuest(NAME_None); }
		EnsureQuestWidget();
		bQuestMode = true;
		if (Panel_QuestHost) Panel_QuestHost->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (Map_Stages) Map_Stages->SetVisibility(ESlateVisibility::Visible);
		if (UWidget* const Header = WidgetTree ? WidgetTree->FindWidget(TEXT("VerticalBox_StageHeader")) : nullptr)
			Header->SetVisibility(ESlateVisibility::Visible);
		if (UWidget* const Hints = WidgetTree ? WidgetTree->FindWidget(TEXT("Panel_StageHints")) : nullptr)
			Hints->SetVisibility(ESlateVisibility::Visible);
		bStageLaunchAvailable = false;
		if (Panel_StageInfo) Panel_StageInfo->SetVisibility(ESlateVisibility::Collapsed);
		if (QuestWidget) QuestWidget->RefreshQuests();
		bHasStages = RebuildMapMarkers();
		ShowSelectedRegionQuests();
		PlaceQuestPanel();
	}
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
	if (NewPage == EKnightMainMenuPage::Maintenance)
	{
		EnsureMaintenanceWidget();
		if (!MaintenanceCameraRig.IsValid() && GetWorld())
		{
			for (TActorIterator<AKnightMaintenanceCameraRig> It(GetWorld()); It; ++It)
			{
				MaintenanceCameraRig = *It;
				break;
			}
		}
		if (MaintenanceCameraRig.IsValid()) MaintenanceCameraRig->SetMaintenanceView(true);
		ShowMaintenanceSelection();
	}
	if (NewPage == EKnightMainMenuPage::Inventory && InventoryWidget) InventoryWidget->Open();
	// Settings and the codex bring their own close buttons.
	UpdateBackVisibility(NewPage == EKnightMainMenuPage::Home || NewPage == EKnightMainMenuPage::Settings || NewPage == EKnightMainMenuPage::Codex);
	RefreshTravelControls();
	if (!IsTravelPending() && GetFlow() && GetFlow()->GetCurrentFlowState() == EKnightFlowState::MainMenu)
	{
		ClearStatus();
	}
	if (NewPage == EKnightMainMenuPage::Stage && !bHasStages)
	{
		SetStatusText(LOCTEXT("NoStages", "표시할 스테이지가 없습니다. 카탈로그 설정을 확인해 주세요."));
	}
	FocusCurrentPage();
	if (NewPage == EKnightMainMenuPage::Settings && SettingsWidget) SettingsWidget->FocusSettings();
	if (NewPage == EKnightMainMenuPage::Codex && CodexWidget && CodexWidget->GetFocusButton())
	{
		CodexWidget->GetFocusButton()->SetUserFocus(GetOwningPlayer());
	}
	if (NewPage == EKnightMainMenuPage::Maintenance)
		if (UWidget* const Focus = GetPreferredFocusTarget()) Focus->SetUserFocus(GetOwningPlayer());
	if (NewPage == EKnightMainMenuPage::Inventory && InventoryWidget && InventoryWidget->GetFocusButton())
		InventoryWidget->GetFocusButton()->SetUserFocus(GetOwningPlayer());
	return true;
}

bool UKnightMainMenuWidget::HandleBackRequested()
{
	if (CurrentPage == EKnightMainMenuPage::Inventory) return ShowPage(EKnightMainMenuPage::Home);
	if (CurrentPage == EKnightMainMenuPage::Settings && SettingsWidget) { SettingsWidget->RequestClose(); return true; }
	if (CurrentPage == EKnightMainMenuPage::Codex) { HandleCodexClosed(); return true; }
	if (CurrentPage == EKnightMainMenuPage::Maintenance)
	{
		if (IsMaintenanceDestinationOpen() && MaintenanceWidget && MaintenanceWidget->HandleBack()) return true;
		if (IsMaintenanceDestinationOpen()) { ShowMaintenanceSelection(); return true; }
		return ShowPage(EKnightMainMenuPage::Home);
	}
	if (CurrentPage == EKnightMainMenuPage::Stage && QuestWidget
		&& !QuestWidget->GetSelectedQuestId().IsNone()) return QuestWidget->SelectQuest(NAME_None);
	// First Esc clears the selected region, the next one leaves the map.
	if (CurrentPage == EKnightMainMenuPage::Stage && !SelectedStageId.IsNone())
	{
		return SelectStage(NAME_None);
	}
	return CurrentPage == EKnightMainMenuPage::Home ? ShowPage(EKnightMainMenuPage::Settings) : ShowPage(EKnightMainMenuPage::Home);
}

FReply UKnightMainMenuWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::I && !InKeyEvent.IsRepeat() && !IsTravelPending()
		&& (CurrentPage == EKnightMainMenuPage::Home || CurrentPage == EKnightMainMenuPage::Inventory))
	{
		if (CurrentPage == EKnightMainMenuPage::Home)
		{
			FocusBeforeInventory.Reset();
			for (UCommonButtonBase* Button : { Button_Stage.Get(), Button_Maintenance.Get(), Button_Settings.Get(),
				Button_Codex.Get(), Button_ReturnToTitle.Get() })
			{
				if (Button && (Button->HasAnyUserFocus() || Button->HasFocusedDescendants())) { FocusBeforeInventory = Button; break; }
			}
		}
		ShowPage(CurrentPage == EKnightMainMenuPage::Inventory ? EKnightMainMenuPage::Home : EKnightMainMenuPage::Inventory);
		return FReply::Handled();
	}
	if (CurrentPage == EKnightMainMenuPage::Maintenance && IsMaintenanceDestinationOpen() && MaintenanceWidget && !IsTravelPending())
	{
		const FKey Key = InKeyEvent.GetKey();
		// 방향키는 포커스 이동. Q/E 정보 탭, 목록 줄에서 Enter 실행.
		if (Key == EKeys::Q || Key == EKeys::E || Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Gamepad_RightShoulder)
		{
			MaintenanceWidget->CycleInfoTab(Key == EKeys::Q || Key == EKeys::Gamepad_LeftShoulder ? -1 : 1);
			return FReply::Handled();
		}
		if ((Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom) && !InKeyEvent.IsRepeat() && MaintenanceWidget->HandleConfirmKey())
		{
			return FReply::Handled();
		}
	}
	// Left / Right walk the regions on the map.
	if (CurrentPage == EKnightMainMenuPage::Stage && !IsTravelPending())
	{
		const FKey Key = InKeyEvent.GetKey();
		if (Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Right || Key == EKeys::D)
		{
			CycleStage(Key == EKeys::Left || Key == EKeys::A ? -1 : 1);
			return FReply::Handled();
		}
		if ((Key == EKeys::Up || Key == EKeys::Down) && QuestWidget && !SelectedStageId.IsNone())
		{
			QuestWidget->CycleQuest(Key == EKeys::Up ? -1 : 1);
			return FReply::Handled();
		}
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
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

void UKnightMainMenuWidget::EnsureMaintenanceWidget()
{
	if (MaintenanceWidget || !Panel_Maintenance || !GetOwningPlayer() || !WidgetTree) return;
	MaintenanceSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("Switcher_MaintenanceRuntime"));
	MaintenanceSelectionPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Panel_MaintenanceSelectionRuntime"));
	UBorder* const Card = WidgetTree->ConstructWidget<UBorder>();
	Card->SetBrushColor(FLinearColor(0.035f, 0.065f, 0.1f, 0.92f));
	Card->SetPadding(FMargin(26.f, 24.f));
	UCanvasPanelSlot* const CardSlot = MaintenanceSelectionPanel->AddChildToCanvas(Card);
	CardSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	CardSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	CardSlot->SetSize(FVector2D(520.f, 360.f));
	UVerticalBox* const Options = WidgetTree->ConstructWidget<UVerticalBox>();
	Card->SetContent(Options);
	UCommonTextBlock* const Heading = WidgetTree->ConstructWidget<UCommonTextBlock>();
	Heading->SetText(LOCTEXT("MaintenanceSelectionHeading", "정비"));
	if (const TSubclassOf<UCommonTextStyle> Style = LoadClass<UCommonTextStyle>(nullptr,
		TEXT("/Game/Knight/UI/Style/TS_Knight_Subhead.TS_Knight_Subhead_C"))) Heading->SetStyle(Style);
	Options->AddChildToVerticalBox(Heading)->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
	const TSubclassOf<UCommonButtonBase> ButtonClass = LoadClass<UCommonButtonBase>(nullptr,
		TEXT("/Game/Knight/UI/Common/WBP_KnightButton.WBP_KnightButton_C"));
	auto AddChoice = [this, Options, ButtonClass](const FText& Label, EKnightMaintenanceDestination Destination) -> UCommonButtonBase*
	{
		if (!ButtonClass) return nullptr;
		UCommonButtonBase* const Button = CreateWidget<UCommonButtonBase>(GetOwningPlayer(), ButtonClass);
		if (!Button) return nullptr;
		SetMaintenanceButtonLabel(Button, Label);
		Button->SetIsSelectable(true);
		Button->SetIsInteractableWhenSelected(true);
		USizeBox* const Height = WidgetTree->ConstructWidget<USizeBox>();
		Height->SetHeightOverride(70.f);
		Height->AddChild(Button);
		Options->AddChildToVerticalBox(Height)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		Button->OnClicked().AddWeakLambda(this, [this, Destination]() { OpenMaintenanceDestination(Destination); });
		return Button;
	};
	MaintenanceCraftingButton = AddChoice(LOCTEXT("MaintenanceChoiceCrafting", "제작"), EKnightMaintenanceDestination::Crafting);
	MaintenanceEnhancementButton = AddChoice(LOCTEXT("MaintenanceChoiceEnhancement", "강화"), EKnightMaintenanceDestination::Enhancement);
	MaintenanceSocketsButton = AddChoice(LOCTEXT("MaintenanceChoiceSockets", "소켓"), EKnightMaintenanceDestination::Sockets);
	MaintenanceSwitcher->AddChild(MaintenanceSelectionPanel);
	MaintenanceWidget = CreateWidget<UKnightMaintenanceWidget>(GetOwningPlayer(), UKnightMaintenanceWidget::GetSharedWidgetClass());
	if (!MaintenanceWidget) return;
	MaintenanceSwitcher->AddChild(MaintenanceWidget);
	Panel_Maintenance->ClearChildren();
	UPanelSlot* AddedSlot = Panel_Maintenance->AddChild(MaintenanceSwitcher);
	if (UVerticalBoxSlot* MaintenanceSlot = Cast<UVerticalBoxSlot>(AddedSlot))
		MaintenanceSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	else if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(AddedSlot))
	{
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CanvasSlot->SetOffsets(FMargin(0.f));
	}
	MaintenanceWidget->OnSlotFocused.BindWeakLambda(this, [this](FGameplayTag FocusedSlot)
	{
		if (MaintenanceCameraRig.IsValid()) MaintenanceCameraRig->FocusSlot(FocusedSlot);
	});
}

bool UKnightMainMenuWidget::IsMaintenanceDestinationOpen() const
{
	return MaintenanceSwitcher && MaintenanceWidget && MaintenanceSwitcher->GetActiveWidget() == MaintenanceWidget;
}

void UKnightMainMenuWidget::ShowMaintenanceSelection()
{
	if (!MaintenanceSwitcher || !MaintenanceSelectionPanel) return;
	if (MaintenanceWidget) MaintenanceWidget->Close();
	MaintenanceSwitcher->SetActiveWidget(MaintenanceSelectionPanel);
	if (MaintenanceCameraRig.IsValid()) MaintenanceCameraRig->FocusSlot(FGameplayTag());
	if (CurrentPage == EKnightMainMenuPage::Maintenance && GetOwningPlayer() && MaintenanceCraftingButton)
		MaintenanceCraftingButton->SetUserFocus(GetOwningPlayer());
}

void UKnightMainMenuWidget::OpenMaintenanceDestination(EKnightMaintenanceDestination Destination)
{
	if (CurrentPage != EKnightMainMenuPage::Maintenance || IsTravelPending() || !MaintenanceSwitcher || !MaintenanceWidget) return;
	MaintenanceWidget->Open(Destination);
	MaintenanceSwitcher->SetActiveWidget(MaintenanceWidget);
	if (MaintenanceCameraRig.IsValid()) MaintenanceCameraRig->FocusSlot(
		Destination == EKnightMaintenanceDestination::Enhancement ? knightGameplayTags::Equipment_Slot_Weapon : FGameplayTag());
	FocusCurrentPage();
}

void UKnightMainMenuWidget::EnsureInventoryWidget()
{
	if (InventoryWidget || !Switcher_Pages || !GetOwningPlayer() || !WidgetTree) return;
	InventoryPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Panel_InventoryRuntime"));
	Switcher_Pages->AddChild(InventoryPanel);
	InventoryWidget = CreateWidget<UKnightInventoryWidget>(GetOwningPlayer(), UKnightInventoryWidget::StaticClass());
	if (!InventoryWidget) return;
	UCanvasPanelSlot* InventoryCanvasSlot = InventoryPanel->AddChildToCanvas(InventoryWidget);
	InventoryCanvasSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	InventoryCanvasSlot->SetOffsets(FMargin(0));
}

void UKnightMainMenuWidget::EnsureQuestWidget()
{
	if (QuestWidget || !Panel_QuestHost || !GetOwningPlayer()) return;
	QuestWidget = CreateWidget<UKnightQuestMenuWidget>(GetOwningPlayer(), UKnightQuestMenuWidget::StaticClass());
	if (!QuestWidget) return;
	UCanvasPanelSlot* const QuestSlot = Panel_QuestHost->AddChildToCanvas(QuestWidget);
	QuestSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	QuestSlot->SetOffsets(FMargin(0));
	QuestWidget->OnLaunchRequested.AddUObject(this, &ThisClass::HandleQuestLaunchRequested);
	QuestWidget->OnSelectionChanged.AddUObject(this, &ThisClass::HandleQuestSelectionChanged);
	if (Map_Stages) Map_Stages->SetLeaderTarget(QuestWidget);
}

bool UKnightMainMenuWidget::SelectQuest(FName QuestId)
{
	if (IsTravelPending()) return false;
	if (CurrentPage != EKnightMainMenuPage::Stage && !ShowPage(EKnightMainMenuPage::Stage)) return false;
	if (!QuestWidget) return false;
	if (QuestId.IsNone()) return QuestWidget->SelectQuest(NAME_None);
	const FName* const RegionId = QuestRegionById.Find(QuestId);
	if (!RegionId || !SelectStage(*RegionId)) return false;
	return QuestWidget->SelectQuest(QuestId);
}

void UKnightMainMenuWidget::HandleCodexClosed()
{
	bRestoreCodexFocus = true;
	ShowPage(EKnightMainMenuPage::Home);
}

UWidget* UKnightMainMenuWidget::GetPreferredFocusTarget() const
{
	if (CurrentPage == EKnightMainMenuPage::Home && bRestoreInventoryFocus && FocusBeforeInventory.IsValid())
		return FocusBeforeInventory.Get();
	if (CurrentPage == EKnightMainMenuPage::Inventory && InventoryWidget) return InventoryWidget->GetFocusButton();
	if (CurrentPage == EKnightMainMenuPage::Home && bRestoreMaintenanceFocus && Button_Maintenance) return Button_Maintenance;
	if (CurrentPage == EKnightMainMenuPage::Maintenance)
		return IsMaintenanceDestinationOpen() && MaintenanceWidget ? MaintenanceWidget->GetFocusButton() : MaintenanceCraftingButton.Get();
	if (CurrentPage == EKnightMainMenuPage::Home && bRestoreCodexFocus && Button_Codex)
	{
		return Button_Codex;
	}
	if (CurrentPage == EKnightMainMenuPage::Codex && CodexWidget)
	{
		return CodexWidget->GetFocusButton();
	}
	if (CurrentPage == EKnightMainMenuPage::Stage && QuestWidget && QuestWidget->GetFocusButton())
		return QuestWidget->GetFocusButton();
	if (CurrentPage == EKnightMainMenuPage::Stage && Map_Stages) return Map_Stages;
	if (CurrentPage == EKnightMainMenuPage::Stage && !SelectedStageId.IsNone() && CanFocusButton(Button_LaunchStage))
	{
		return Button_LaunchStage;
	}
	if (CurrentPage != EKnightMainMenuPage::Home)
	{
		return Button_Back;
	}
	for (UCommonButtonBase* Button : { Button_Stage.Get(), Button_Maintenance.Get(), Button_Settings.Get(), Button_Codex.Get(), Button_ReturnToTitle.Get() })
	{
		if (CanFocusButton(Button))
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
	if (Button_LaunchStage) Button_LaunchStage->SetIsEnabled(bEnabled && bStageLaunchAvailable && !SelectedStageId.IsNone());
	if (Map_Stages) Map_Stages->SetIsEnabled(bEnabled);
	if (QuestWidget) QuestWidget->SetIsEnabled(bEnabled);
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

bool UKnightMainMenuWidget::RebuildMapMarkers()
{
	StageOrder.Reset();
	QuestRegionById.Reset();
	RegionNames.Reset();
	MapQuestCatalog = UKnightQuestSettings::LoadCatalog();
	UKnightGameFlowSubsystem* const Flow = GetFlow();
	TArray<FKnightStageDefinition> Stages;
	if (!Map_Stages)
	{
		return false;
	}
	if (Flow) Flow->GetStageDefinitions(Stages);
	TArray<FKnightMapMarker> Markers;
	for (const FKnightStageDefinition& Stage : Stages)
	{
		if (Markers.Num() >= 8) break; // The existing UI material has eight region parameters.
		FKnightMapMarker& Marker = Markers.AddDefaulted_GetRef();
		Marker.StageId = Stage.StageId;
		const FText& Name = Stage.RegionName.IsEmpty() ? Stage.DisplayName : Stage.RegionName;
		Marker.Label = Stage.bDevelopmentOnly ? FText::Format(LOCTEXT("DevStageName", "[DEV] {0}"), Name) : Name;
		Marker.Location = Stage.GlobeLocation;
		Marker.RegionRadius = Stage.GlobeRegionRadius;
		Marker.bLocked = true;
		StageOrder.Add(Stage.StageId);
		RegionNames.Add(Stage.StageId, Marker.Label);
	}
	if (MapQuestCatalog)
	{
		for (const FKnightQuestMapRegion& Region : MapQuestCatalog->AdditionalMapRegions)
		{
			if (Region.RegionId.IsNone() || StageOrder.Contains(Region.RegionId) || Markers.Num() >= 8)
			{
				UE_LOG(Logknight, Error, TEXT("Invalid or excess quest map region %s."), *Region.RegionId.ToString());
				continue;
			}
			FKnightMapMarker& Marker = Markers.AddDefaulted_GetRef();
			Marker.StageId = Region.RegionId;
			Marker.Label = Region.DisplayName;
			Marker.Location = Region.GlobeLocation;
			Marker.RegionRadius = Region.GlobeRegionRadius;
			Marker.bLocked = true;
			StageOrder.Add(Region.RegionId);
			RegionNames.Add(Region.RegionId, Region.DisplayName);
		}
	}
	TArray<FKnightMapQuestMarker> QuestMarkers;
	if (MapQuestCatalog)
	{
		for (const FKnightQuestMapPlacement& Placement : MapQuestCatalog->MapPlacements)
		{
			const UKnightQuestDefinition* const Quest = MapQuestCatalog->FindQuest(Placement.QuestId);
			const FKnightMapMarker* const Region = Markers.FindByPredicate(
				[&Placement](const FKnightMapMarker& Entry) { return Entry.StageId == Placement.RegionId; });
			if (!Quest || !Region || QuestRegionById.Contains(Placement.QuestId))
			{
				UE_LOG(Logknight, Error, TEXT("Invalid or duplicate quest map placement %s -> %s."),
					*Placement.QuestId.ToString(), *Placement.RegionId.ToString());
				continue;
			}
			FText Reason;
			const bool bCanLaunch = MapQuestCatalog->CanSelectQuest(Quest->QuestId, GetGameInstance(), Reason);
			FKnightMapQuestMarker& Pin = QuestMarkers.AddDefaulted_GetRef();
			Pin.QuestId = Quest->QuestId;
			Pin.RegionId = Placement.RegionId;
			Pin.Label = Quest->Difficulty == EKnightQuestDifficulty::Hard ? LOCTEXT("MapHard", "어려움") : LOCTEXT("MapNormal", "일반");
			Pin.Location = Region->Location + Placement.MarkerOffset;
			Pin.bLocked = !bCanLaunch;
			QuestRegionById.Add(Quest->QuestId, Placement.RegionId);
			if (bCanLaunch)
			{
				for (FKnightMapMarker& MapRegion : Markers)
				{
					if (MapRegion.StageId == Placement.RegionId) { MapRegion.bLocked = false; break; }
				}
			}
		}
	}
	Map_Stages->SetMarkers(Markers);
	Map_Stages->SetQuestMarkers(QuestMarkers);
	return !StageOrder.IsEmpty();
}

void UKnightMainMenuWidget::ShowSelectedRegionQuests()
{
	if (!QuestWidget) return;
	TArray<FName> QuestIds;
	if (MapQuestCatalog)
	{
		for (const FKnightQuestMapPlacement& Placement : MapQuestCatalog->MapPlacements)
		{
			if (Placement.RegionId == SelectedStageId && QuestRegionById.Contains(Placement.QuestId))
				QuestIds.Add(Placement.QuestId);
		}
	}
	QuestWidget->SetRegionQuests(SelectedStageId, RegionNames.FindRef(SelectedStageId), QuestIds);
	if (Map_Stages) Map_Stages->SetSelectedQuest(NAME_None);
}

void UKnightMainMenuWidget::PlaceQuestPanel()
{
	if (!QuestWidget) return;
	if (UCanvasPanelSlot* const QuestCanvasSlot = Cast<UCanvasPanelSlot>(QuestWidget->Slot))
	{
		// The map and detail stay side by side; the empty host area passes clicks to the map.
		QuestCanvasSlot->SetAnchors(FAnchors(0.69f, 0.04f, 1.f, 0.96f));
		QuestCanvasSlot->SetAlignment(FVector2D::ZeroVector);
		QuestCanvasSlot->SetOffsets(FMargin(0.f, 0.f, 24.f, 0.f));
	}
}

bool UKnightMainMenuWidget::SelectStage(FName StageId)
{
	UKnightGameFlowSubsystem* const Flow = GetFlow();
	if (!Flow || Flow->GetCurrentFlowState() != EKnightFlowState::MainMenu || IsTravelPending())
	{
		SetStatusText(LOCTEXT("CannotSelectNow", "지금 스테이지를 선택할 수 없습니다."));
		return false;
	}
	if (!StageId.IsNone() && !StageOrder.Contains(StageId))
	{
		SetStatusText(LOCTEXT("UnknownRegion", "지도 구역 정보를 확인할 수 없습니다."));
		return false;
	}
	if (CurrentPage != EKnightMainMenuPage::Stage && !ShowPage(EKnightMainMenuPage::Stage))
	{
		return false;
	}
	SelectedStageId = StageId;
	if (Map_Stages) Map_Stages->SetSelectedStage(StageId);
	ClearStatus();
	ShowSelectedRegionQuests();
	FocusCurrentPage();
	return true;
}

bool UKnightMainMenuWidget::CycleStage(int32 Direction)
{
	if (StageOrder.IsEmpty())
	{
		return false;
	}
	const int32 Current = StageOrder.IndexOfByKey(SelectedStageId);
	const int32 Next = Current == INDEX_NONE ? (Direction > 0 ? 0 : StageOrder.Num() - 1) : (Current + Direction + StageOrder.Num()) % StageOrder.Num();
	if (Map_Stages) Map_Stages->FocusStage(StageOrder[Next]);
	return SelectStage(StageOrder[Next]);
}

void UKnightMainMenuWidget::PlaceStageInfo()
{
	if (!Panel_StageInfo)
	{
		return;
	}
	Panel_StageInfo->SetVisibility(SelectedStageId.IsNone() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	UCanvasPanelSlot* const InfoSlot = Cast<UCanvasPanelSlot>(Panel_StageInfo->Slot);
	const UCanvasPanelSlot* const MapSlot = Map_Stages ? Cast<UCanvasPanelSlot>(Map_Stages->Slot) : nullptr;
	if (!InfoSlot || !MapSlot)
	{
		return;
	}
	// Both are anchored to the screen centre; the panel hangs off the map's right edge.
	InfoSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	InfoSlot->SetAlignment(FVector2D(0.f, 0.5f));
	InfoSlot->SetPosition(FVector2D(MapSlot->GetPosition().X + MapSlot->GetSize().X * 0.5f + StageInfoGap, MapSlot->GetPosition().Y));
}

namespace
{
	FText FormatMinutes(float Seconds)
	{
		const int32 Whole = FMath::Max(0, FMath::FloorToInt(Seconds));
		return FText::FromString(FString::Printf(TEXT("%02d:%02d"), Whole / 60, Whole % 60));
	}

	FText FormatRecord(float Seconds)
	{
		const int32 Hundredths = FMath::Max(0, FMath::RoundToInt(Seconds * 100.f));
		return FText::FromString(FString::Printf(TEXT("%02d:%02d.%02d"), Hundredths / 6000, (Hundredths / 100) % 60, Hundredths % 100));
	}
}

void UKnightMainMenuWidget::RefreshStageDetails()
{
	UKnightGameFlowSubsystem* const Flow = GetFlow();
	FKnightStageDefinition Stage;
	if (SelectedStageId.IsNone() || !Flow || !Flow->FindStageDefinition(SelectedStageId, Stage))
	{
		bStageLaunchAvailable = false;
		if (Button_LaunchStage) Button_LaunchStage->SetIsEnabled(false);
		return;
	}
	if (Text_StageTitle) Text_StageTitle->SetText(Stage.bDevelopmentOnly ? FText::Format(LOCTEXT("DevStageName", "[DEV] {0}"), Stage.DisplayName) : Stage.DisplayName);
	if (Text_StageDescription) Text_StageDescription->SetText(Stage.Description);
	if (Text_StageRegion) Text_StageRegion->SetText(Stage.RegionName.IsEmpty() ? LOCTEXT("NoRegion", "-") : Stage.RegionName);

	const UknightProgressionSubsystem* const Progression = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>() : nullptr;
	const float Best = Progression ? Progression->GetBestClearTime(SelectedStageId) : 0.f;
	if (Text_StageRecord) Text_StageRecord->SetText(Best > 0.f ? FormatRecord(Best) : LOCTEXT("NoRecord", "기록 없음"));

	if (Img_StageThumbnail)
	{
		UTexture2D* const Thumbnail = Stage.Thumbnail.LoadSynchronous();
		Img_StageThumbnail->SetBrushFromTexture(Thumbnail);
		Img_StageThumbnail->SetVisibility(Thumbnail ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	FText Reason;
	bStageLaunchAvailable = Flow->CanLaunchStage(SelectedStageId, Reason);
	if (Button_LaunchStage) Button_LaunchStage->SetIsEnabled(bStageLaunchAvailable && !IsTravelPending());
	if (Text_StageLock)
	{
		Text_StageLock->SetText(Reason);
		Text_StageLock->SetVisibility(bStageLaunchAvailable || Reason.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	// Target and time limit live in the raid definition.
	if (RaidDefinitionHandle.IsValid())
	{
		RaidDefinitionHandle->CancelHandle();
		RaidDefinitionHandle.Reset();
	}
	if (Stage.RaidDefinition.IsNull() || Stage.RaidDefinition.Get())
	{
		RefreshRaidDetails();
		return;
	}
	if (Text_StageTarget) Text_StageTarget->SetText(LOCTEXT("Loading", "..."));
	if (Text_StageTimeLimit) Text_StageTimeLimit->SetText(LOCTEXT("Loading", "..."));
	RaidDefinitionHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Stage.RaidDefinition.ToSoftObjectPath(),
		FStreamableDelegate::CreateWeakLambda(this, [this]() { RefreshRaidDetails(); }));
}

void UKnightMainMenuWidget::RefreshRaidDetails()
{
	UKnightGameFlowSubsystem* const Flow = GetFlow();
	FKnightStageDefinition Stage;
	if (!Flow || !Flow->FindStageDefinition(SelectedStageId, Stage))
	{
		return;
	}
	const UknightRaidDefinition* const Raid = Stage.RaidDefinition.Get();
	const UknightMonsterDefinition* const Monster = Raid ? Raid->TargetMonster.Get() : nullptr;
	if (Text_StageTarget) Text_StageTarget->SetText(Monster && !Monster->DisplayName.IsEmpty() ? Monster->DisplayName : LOCTEXT("NoTarget", "-"));
	if (Text_StageTimeLimit)
	{
		Text_StageTimeLimit->SetText(!Raid ? LOCTEXT("NoTimeInfo", "-")
			: (Raid->TimeLimit > 0.f ? FormatMinutes(Raid->TimeLimit) : LOCTEXT("NoTimeLimit", "제한 없음")));
	}
}

void UKnightMainMenuWidget::HandleMapStageClicked(FName StageId)
{
	SelectStage(StageId);
}

void UKnightMainMenuWidget::HandleMapQuestClicked(FName QuestId)
{
	SelectQuest(QuestId);
}

void UKnightMainMenuWidget::HandleQuestSelectionChanged(FName QuestId)
{
	if (Map_Stages) Map_Stages->SetSelectedQuest(QuestId);
}

void UKnightMainMenuWidget::HandleLaunchStageClicked()
{
	if (bQuestMode) return;
	UKnightGameFlowSubsystem* const Flow = GetFlow();
	if (!Flow || IsTravelPending() || !bStageLaunchAvailable || SelectedStageId.IsNone())
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

void UKnightMainMenuWidget::HandleQuestLaunchRequested(FName QuestId)
{
	UKnightGameFlowSubsystem* const Flow = GetFlow();
	if (!Flow) return;
	FText Reason;
	const bool bAccepted = Flow->RequestStartQuest(QuestId, Reason);
	RefreshTravelControls();
	if (bAccepted) SetStatusText(LOCTEXT("LaunchingQuest", "퀘스트 출격 준비 중입니다."));
	else if (!Reason.IsEmpty()) SetStatusText(Reason);
}

#undef LOCTEXT_NAMESPACE
