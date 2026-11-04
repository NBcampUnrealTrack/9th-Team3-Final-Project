#include "KnightMainMenuWidget.h"
#include "KnightButtonFillExtension.h"
#include "Menu/UI/KnightMaintenancePresentation.h"

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
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "knight.h"
#include "Settings/KnightSettingsWidget.h"
#include "Codex/KnightCodexWidget.h"
#include "Menu/KnightMaintenanceCameraRig.h"
#include "Menu/UI/KnightMaintenanceWidget.h"
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
	for (UCommonButtonBase* Button : { Button_Stage.Get(), Button_ReturnToTitle.Get(), Button_Maintenance.Get(), Button_Settings.Get(),
		Button_Codex.Get() })
		UKnightButtonFillExtension::SetLabelFitted(Button, FLinearColor(FColor(36, 60, 88)));
	BuildMaintenanceFooterBack();
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
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UknightProgressionSubsystem* Progress = GI->GetSubsystem<UknightProgressionSubsystem>())
			Progress->OnProgressionChanged.AddUniqueDynamic(this, &ThisClass::HandleQuestProgressChanged);
		if (UknightSaveSubsystem* Saves = GI->GetSubsystem<UknightSaveSubsystem>())
			Saves->OnActiveSlotChanged.AddUniqueDynamic(this, &ThisClass::HandleQuestProfileChanged);
	}
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
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UknightProgressionSubsystem* Progress = GI->GetSubsystem<UknightProgressionSubsystem>())
			Progress->OnProgressionChanged.RemoveDynamic(this, &ThisClass::HandleQuestProgressChanged);
		if (UknightSaveSubsystem* Saves = GI->GetSubsystem<UknightSaveSubsystem>())
			Saves->OnActiveSlotChanged.RemoveDynamic(this, &ThisClass::HandleQuestProfileChanged);
	}
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

void UKnightMainMenuWidget::HandleQuestProgressChanged()
{
	if (CurrentPage != EKnightMainMenuPage::Stage) return;
	RebuildMapMarkers();
	if (QuestWidget) QuestWidget->RefreshQuests();
}

void UKnightMainMenuWidget::HandleQuestProfileChanged(const FString& SlotId)
{
	SelectedStageId = NAME_None;
	if (Map_Stages) { Map_Stages->SetSelectedStage(NAME_None); Map_Stages->SetSelectedQuest(NAME_None); }
	if (QuestWidget) QuestWidget->SelectQuest(NAME_None);
	if (CurrentPage == EKnightMainMenuPage::Stage)
	{
		RebuildMapMarkers();
		ShowSelectedRegionQuests();
		RefreshStageDetails();
	}
}

bool UKnightMainMenuWidget::ShowPage(EKnightMainMenuPage NewPage)
{
	if (NewPage == EKnightMainMenuPage::Inventory) return OpenMaterialsShortcut();
	if (NewPage == EKnightMainMenuPage::Maintenance)
	{
		if (IsTravelPending()) return false;
		ShowMaintenanceSelection();
		return CurrentPage == EKnightMainMenuPage::Home && MaintenanceSubmenu && (MaintenanceSubmenu->GetVisibility() == ESlateVisibility::Visible);
	}
	return ApplyPage(NewPage, false);
}

bool UKnightMainMenuWidget::OpenMaterialsShortcut()
{
	if (CurrentPage != EKnightMainMenuPage::Home || IsTravelPending()) return false;
	FocusBeforeMaterialsShortcut.Reset();
	for (UCommonButtonBase* Button : { Button_Stage.Get(), Button_Maintenance.Get(), Button_Settings.Get(),
		Button_Codex.Get(), Button_ReturnToTitle.Get() })
	{
		if (Button && (Button->HasAnyUserFocus() || Button->HasFocusedDescendants()))
		{
			FocusBeforeMaterialsShortcut = Button;
			break;
		}
	}
	EnsureMaintenanceWidget();
	if (!MaintenanceWidget || !MaintenanceSwitcher) return false;
	OpenMaintenanceDestination(EKnightMaintenanceDestination::Crafting, true);
	return IsMaintenanceDestinationOpen() && MaintenanceWidget->IsMaterialsViewOpen();
}

bool UKnightMainMenuWidget::ToggleMaterialsShortcut()
{
	if (IsTravelPending()) return false;
	if (CurrentPage == EKnightMainMenuPage::Home) return OpenMaterialsShortcut();
	if (CurrentPage != EKnightMainMenuPage::Maintenance || !IsMaintenanceDestinationOpen()
		|| !MaintenanceWidget || !MaintenanceWidget->IsMaterialsViewOpen()
		|| MaintenanceWidget->IsMaterialQuantityInputFocused()) return false;
	if (MaintenanceWidget->HandleBack()) return true;
	bRestoreMaterialsShortcutFocus = true;
	return ShowPage(EKnightMainMenuPage::Home);
}

bool UKnightMainMenuWidget::ApplyPage(EKnightMainMenuPage NewPage, bool bAllowDuringTravel)
{
	if (!bAllowDuringTravel && IsTravelPending())
	{
		return false;
	}
	UPanelWidget* Page = nullptr;
	switch (NewPage)
	{
	case EKnightMainMenuPage::Home: Page = Panel_Home; break;
	case EKnightMainMenuPage::Stage: Page = Panel_Stage; break;
	case EKnightMainMenuPage::Maintenance: Page = Panel_Maintenance; break;
	case EKnightMainMenuPage::Settings: Page = Panel_Settings; break;
	case EKnightMainMenuPage::Codex: Page = Panel_Codex; break;
	case EKnightMainMenuPage::Inventory: return false; // ShowPage handles the serialized legacy value.
	default:
		UE_LOG(Logknight, Error, TEXT("Invalid main menu page %d."), static_cast<int32>(NewPage));
		return false;
	}
	if (!ActivatePage(Page))
	{
		return false;
	}
	for (UCommonButtonBase* Button : { Button_Stage.Get(), Button_ReturnToTitle.Get(), Button_Maintenance.Get(), Button_Settings.Get(),
		Button_Codex.Get() })
		UKnightButtonFillExtension::Reset(Button);
	SetMaintenanceSubmenuOpen(false);
	const bool bEnteringStage = NewPage == EKnightMainMenuPage::Stage && CurrentPage != EKnightMainMenuPage::Stage;
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
	if (NewPage != EKnightMainMenuPage::Home) bRestoreMaterialsShortcutFocus = false;
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
	}
	// Settings and the codex bring their own close buttons.
	UpdateBackVisibility(NewPage == EKnightMainMenuPage::Home || NewPage == EKnightMainMenuPage::Settings || NewPage == EKnightMainMenuPage::Codex);
	ApplyMaintenanceBackPresentation(NewPage == EKnightMainMenuPage::Maintenance);
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
	return true;
}

bool UKnightMainMenuWidget::HandleBackRequested()
{
	if (CurrentPage == EKnightMainMenuPage::Settings && SettingsWidget) { SettingsWidget->RequestClose(); return true; }
	if (CurrentPage == EKnightMainMenuPage::Codex) { HandleCodexClosed(); return true; }
	if (CurrentPage == EKnightMainMenuPage::Maintenance)
	{
		if (IsMaintenanceDestinationOpen() && MaintenanceWidget && MaintenanceWidget->HandleBack()) return true;
		if (bMaterialsShortcutEntry)
		{
			bRestoreMaterialsShortcutFocus = true;
			return ShowPage(EKnightMainMenuPage::Home);
		}
		ShowMaintenanceSelection();
		return true;
	}
	if (CurrentPage == EKnightMainMenuPage::Home && MaintenanceSubmenu && (MaintenanceSubmenu->GetVisibility() == ESlateVisibility::Visible))
	{
		SetMaintenanceSubmenuOpen(false);
		if (Button_Maintenance) Button_Maintenance->SetFocus();
		return true;
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
	if (InKeyEvent.GetKey() == EKeys::I && !InKeyEvent.IsRepeat() && !IsTravelPending())
	{
		// Preview handlers run before an editable quantity field receives its typed character.
		return ToggleMaterialsShortcut() ? FReply::Handled() : FReply::Unhandled();
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

void UKnightMainMenuWidget::BuildMaintenanceFooterBack()
{
	// Reuse the already bound Back button: its mouse click and Escape both call RequestBack(),
	// including the Maintenance dialog/picker-first route. Only its Maintenance content changes.
	UUserWidget* const ButtonWidget = Cast<UUserWidget>(Button_Back);
	UOverlay* const Layers = ButtonWidget ? Cast<UOverlay>(ButtonWidget->GetWidgetFromName(TEXT("Overlay_Root"))) : nullptr;
	NormalBackContent = ButtonWidget ? ButtonWidget->GetWidgetFromName(TEXT("Txt_Label")) : nullptr;
	if (!Layers || !NormalBackContent || !ButtonWidget->WidgetTree) return;
	if (const UCommonButtonStyle* const Style = Button_Back->GetStyle()) NormalBackStyle = Style->GetClass();
	if (const UCanvasPanelSlot* const BackSlot = Cast<UCanvasPanelSlot>(Button_Back->Slot))
	{
		NormalBackPosition = BackSlot->GetPosition();
		NormalBackAnchors = BackSlot->GetAnchors();
	}

	const TSubclassOf<UCommonTextStyle> CaptionStyle = LoadClass<UCommonTextStyle>(nullptr,
		TEXT("/Game/Knight/UI/Style/TS_Knight_Caption.TS_Knight_Caption_C"));
	MaintenanceBackContent = ButtonWidget->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MaintenanceBackFooter"));
	MaintenanceBackContent->SetVisibility(ESlateVisibility::Collapsed);
	UOverlaySlot* const ContentSlot = Layers->AddChildToOverlay(MaintenanceBackContent);
	ContentSlot->SetHorizontalAlignment(HAlign_Center);
	ContentSlot->SetVerticalAlignment(VAlign_Center);
	ContentSlot->SetPadding(FMargin(15.f, 8.f)); // Invisible click area beyond the visible keycap and label.

	UBorder* const Keycap = ButtonWidget->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MaintenanceBackKeycap"));
	FSlateBrush KeycapBrush;
	KeycapBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	KeycapBrush.TintColor = FSlateColor(FLinearColor(0.004f, 0.012f, 0.025f, 0.10f));
	KeycapBrush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(2.f),
		FSlateColor(FLinearColor(0.38f, 0.58f, 0.72f, 0.88f)), 1.f);
	KeycapBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	Keycap->SetBrush(KeycapBrush);
	Keycap->SetPadding(FMargin(7.f, 2.f));
	UCommonTextBlock* const Escape = ButtonWidget->WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("MaintenanceBackEscape"));
	Escape->SetText(INVTEXT("ESC"));
	if (CaptionStyle) Escape->SetStyle(CaptionStyle);
	Keycap->SetContent(Escape);
	MaintenanceBackContent->AddChildToHorizontalBox(Keycap)->SetVerticalAlignment(VAlign_Center);
	UCommonTextBlock* const Return = ButtonWidget->WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("MaintenanceBackReturn"));
	Return->SetText(LOCTEXT("MaintenanceBackFooter", "돌아가기"));
	if (CaptionStyle) Return->SetStyle(CaptionStyle);
	Return->SetStyle(nullptr); // Keep the caption font without reapplying its old blue color.
	Return->SetColorAndOpacity(FSlateColor(KnightMaintenancePresentation::Secondary));
	UHorizontalBoxSlot* const ReturnSlot = MaintenanceBackContent->AddChildToHorizontalBox(Return);
	ReturnSlot->SetVerticalAlignment(VAlign_Center);
	ReturnSlot->SetPadding(FMargin(9.f, 0.f, 0.f, 0.f));
}

void UKnightMainMenuWidget::ApplyMaintenanceBackPresentation(bool bMaintenance)
{
	if (!Button_Back || !MaintenanceBackContent || !NormalBackContent) return;
	NormalBackContent->SetVisibility(bMaintenance ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	MaintenanceBackContent->SetVisibility(bMaintenance ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	const bool bCrafting = bMaintenance && MaintenanceWidget
		&& MaintenanceWidget->GetDestination() == EKnightMaintenanceDestination::Crafting;
	if (UUserWidget* BackWidget = Cast<UUserWidget>(Button_Back))
	{
		if (UWidget* Keycap = BackWidget->GetWidgetFromName(TEXT("MaintenanceBackKeycap")))
			Keycap->SetVisibility(bMaintenance ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		if (UWidget* Label = BackWidget->GetWidgetFromName(TEXT("MaintenanceBackReturn")))
			if (UHorizontalBoxSlot* LabelSlot = Cast<UHorizontalBoxSlot>(Label->Slot))
				LabelSlot->SetPadding(FMargin(bMaintenance ? 0.f : 9.f, 0.f, 0.f, 0.f));
	}
	if (bMaintenance)
	{
		const TSubclassOf<UCommonButtonStyle> FooterStyle = LoadClass<UCommonButtonStyle>(nullptr,
			TEXT("/Game/Knight/UI/Style/BS_Knight_MaintenanceFooterBack.BS_Knight_MaintenanceFooterBack_C"));
		if (FooterStyle) Button_Back->SetStyle(FooterStyle);
	}
	else if (NormalBackStyle) Button_Back->SetStyle(NormalBackStyle);
	if (UCanvasPanelSlot* const BackSlot = Cast<UCanvasPanelSlot>(Button_Back->Slot))
	{
		const float PanelWidth = bCrafting ? .29f : bMaintenance && MaintenanceWidget
			&& MaintenanceWidget->GetDestination() == EKnightMaintenanceDestination::Sockets ? .23f : .30f;
		const float Shift = bMaintenance ? PanelWidth * KnightMaintenancePresentation::PanelArtworkInset : 0.f;
		BackSlot->SetAnchors(FAnchors(NormalBackAnchors.Minimum.X + Shift, NormalBackAnchors.Minimum.Y,
			NormalBackAnchors.Maximum.X + Shift, NormalBackAnchors.Maximum.Y));
		BackSlot->SetPosition(bMaintenance ? FVector2D(bCrafting ? -56.f : -48.f, -40.f) : NormalBackPosition);
	}
}

void UKnightMainMenuWidget::EnsureMaintenanceWidget()
{
	if (MaintenanceWidget || !Panel_Maintenance || !GetOwningPlayer() || !WidgetTree) return;
	MaintenanceSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("Switcher_MaintenanceRuntime"));
	// A child of Home, not another page or a popup window. Its position follows
	// the actual menu item's geometry (including DPI), never a screen pixel offset.
	UCanvasPanel* const Home = Cast<UCanvasPanel>(Panel_Home);
	if (!Home) return;
	MaintenanceSubmenu = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel_MaintenanceSubmenu"));
	MaintenanceSubmenu->SetBrushColor(FLinearColor(0.006f, 0.014f, 0.026f, 0.94f));
	MaintenanceSubmenu->SetPadding(FMargin(10.f));
	UCanvasPanelSlot* const CardSlot = Home->AddChildToCanvas(MaintenanceSubmenu);
	CardSlot->SetSize(FVector2D(260.f, 212.f));
	CardSlot->SetZOrder(10);
	MaintenanceSubmenu->SetVisibility(ESlateVisibility::Collapsed);
	UVerticalBox* const Options = WidgetTree->ConstructWidget<UVerticalBox>();
	MaintenanceSubmenu->SetContent(Options);
	const TSubclassOf<UCommonButtonBase> ButtonClass = LoadClass<UCommonButtonBase>(nullptr,
		TEXT("/Game/Knight/UI/Common/WBP_KnightButton.WBP_KnightButton_C"));
	auto AddChoice = [this, Options, ButtonClass](FName Name, const FText& Label, EKnightMaintenanceDestination Destination) -> UCommonButtonBase*
	{
		if (!ButtonClass) return nullptr;
		UCommonButtonBase* const Button = CreateWidget<UCommonButtonBase>(GetOwningPlayer(), ButtonClass, Name);
		if (!Button) return nullptr;
		SetMaintenanceButtonLabel(Button, Label);
		// Navigation actions are not a persistent selection group.
		Button->SetIsSelectable(false);
		KnightMaintenancePresentation::ConfigureButton(Button, false);
		USizeBox* const Height = WidgetTree->ConstructWidget<USizeBox>();
		Height->SetHeightOverride(60.f);
		Height->AddChild(Button);
		Options->AddChildToVerticalBox(Height)->SetPadding(FMargin(0.f, 2.f));
		Button->SetNavigationRuleExplicit(EUINavigation::Left, Button_Maintenance);
		Button->OnClicked().AddWeakLambda(this, [this, Destination]() { OpenMaintenanceDestination(Destination); });
		return Button;
	};
	MaintenanceCraftingButton = AddChoice(TEXT("MaintenanceChoiceCrafting"), LOCTEXT("MaintenanceChoiceCrafting", "제작"), EKnightMaintenanceDestination::Crafting);
	MaintenanceEnhancementButton = AddChoice(TEXT("MaintenanceChoiceEnhancement"), LOCTEXT("MaintenanceChoiceEnhancement", "강화"), EKnightMaintenanceDestination::Enhancement);
	MaintenanceSocketsButton = AddChoice(TEXT("MaintenanceChoiceSockets"), LOCTEXT("MaintenanceChoiceSockets", "소켓"), EKnightMaintenanceDestination::Sockets);
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
	return CurrentPage == EKnightMainMenuPage::Maintenance && MaintenanceSwitcher && MaintenanceWidget
		&& MaintenanceSwitcher->GetActiveWidget() == MaintenanceWidget;
}

void UKnightMainMenuWidget::ShowMaintenanceSelection()
{
	EnsureMaintenanceWidget();
	if (!ApplyPage(EKnightMainMenuPage::Home, false)) return;
	SetMaintenanceSubmenuOpen(true);
	FocusCurrentPage();
}

void UKnightMainMenuWidget::SetMaintenanceSubmenuOpen(bool bOpen)
{
	if (!MaintenanceSubmenu) return;
	MaintenanceSubmenu->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (Button_Maintenance)
	{
		if (bOpen) Button_Maintenance->SetNavigationRuleExplicit(EUINavigation::Right, MaintenanceCraftingButton);
		else Button_Maintenance->SetNavigationRuleBase(EUINavigation::Right, EUINavigationRule::Escape);
	}
	if (bOpen) { ForceLayoutPrepass(); PlaceMaintenanceSubmenu(); }
}

void UKnightMainMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (CurrentPage == EKnightMainMenuPage::Home && MaintenanceSubmenu && (MaintenanceSubmenu->GetVisibility() == ESlateVisibility::Visible))
		PlaceMaintenanceSubmenu();
}

void UKnightMainMenuWidget::PlaceMaintenanceSubmenu()
{
	if (!Panel_Home || !Button_Maintenance || !MaintenanceSubmenu) return;
	UCanvasPanelSlot* const SubmenuSlot = Cast<UCanvasPanelSlot>(MaintenanceSubmenu->Slot);
	if (!SubmenuSlot) return;
	const FGeometry& Home = Panel_Home->GetCachedGeometry();
	const FGeometry& Item = Button_Maintenance->GetCachedGeometry();
	if (Home.GetLocalSize().IsNearlyZero() || Item.GetLocalSize().IsNearlyZero()) return;
	const FVector2D Right = Home.AbsoluteToLocal(Item.LocalToAbsolute(FVector2D(Item.GetLocalSize().X, 0.f)));
	const FVector2D Size = SubmenuSlot->GetSize();
	SubmenuSlot->SetPosition(FVector2D(
		FMath::Clamp(Right.X + 12.f, 0.f, FMath::Max(0.f, Home.GetLocalSize().X - Size.X - 16.f)),
		FMath::Clamp(Right.Y, 16.f, FMath::Max(16.f, Home.GetLocalSize().Y - Size.Y - 16.f))));
}

void UKnightMainMenuWidget::OpenMaintenanceDestination(EKnightMaintenanceDestination Destination, bool bStartInMaterials)
{
	if (IsTravelPending() || !MaintenanceSwitcher || !MaintenanceWidget) return;
	if (CurrentPage != EKnightMainMenuPage::Home && CurrentPage != EKnightMainMenuPage::Maintenance) return;
	if (!ApplyPage(EKnightMainMenuPage::Maintenance, false)) return;
	bMaterialsShortcutEntry = bStartInMaterials;
	MaintenanceWidget->Open(Destination, bStartInMaterials);
	ApplyMaintenanceBackPresentation(true);
	MaintenanceSwitcher->SetActiveWidget(MaintenanceWidget);
	if (MaintenanceCameraRig.IsValid()) MaintenanceCameraRig->FocusSlot(
		Destination == EKnightMaintenanceDestination::Enhancement ? knightGameplayTags::Equipment_Slot_Weapon : FGameplayTag());
	FocusCurrentPage();
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
	if (CurrentPage == EKnightMainMenuPage::Home && MaintenanceSubmenu && (MaintenanceSubmenu->GetVisibility() == ESlateVisibility::Visible))
		return MaintenanceCraftingButton;
	if (CurrentPage == EKnightMainMenuPage::Home && bRestoreMaterialsShortcutFocus && FocusBeforeMaterialsShortcut.IsValid())
		return FocusBeforeMaterialsShortcut.Get();
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
	if (MaintenanceSubmenu) MaintenanceSubmenu->SetIsEnabled(bEnabled);
}

void UKnightMainMenuWidget::HandleStageClicked()
{
	ShowPage(EKnightMainMenuPage::Stage);
}

void UKnightMainMenuWidget::HandleReturnToTitleClicked()
{
	SetMaintenanceSubmenuOpen(false);
	if (UKnightGameFlowSubsystem* const Flow = GetFlow()) Flow->RequestOpenTitle();
}

void UKnightMainMenuWidget::HandleMaintenanceClicked()
{
	if (IsTravelPending()) return;
	if (CurrentPage == EKnightMainMenuPage::Home && MaintenanceSubmenu && (MaintenanceSubmenu->GetVisibility() == ESlateVisibility::Visible))
	{
		SetMaintenanceSubmenuOpen(false);
		if (Button_Maintenance) Button_Maintenance->SetFocus();
	}
	else ShowMaintenanceSelection();
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
			Pin.Label = Quest->bMapExplorationOnly ? LOCTEXT("MapExplore", "일반 · 탐색")
				: (Quest->Difficulty == EKnightQuestDifficulty::Hard ? LOCTEXT("MapHard", "어려움") : LOCTEXT("MapNormal", "일반"));
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
	if (bAccepted)
	{
		const UKnightQuestDefinition* const Quest = MapQuestCatalog ? MapQuestCatalog->FindQuest(QuestId) : nullptr;
		SetStatusText(Quest && Quest->bMapExplorationOnly ? LOCTEXT("LaunchingExploration", "맵으로 이동 중입니다.")
			: LOCTEXT("LaunchingQuest", "퀘스트 출격 준비 중입니다."));
	}
	else if (!Reason.IsEmpty()) SetStatusText(Reason);
}

#undef LOCTEXT_NAMESPACE
