#include "KnightMainMenuWidget.h"

#include "GameFlow/KnightGameFlowSubsystem.h"
#include "CommonButtonBase.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "KnightStageGlobeWidget.h"
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

#define LOCTEXT_NAMESPACE "KnightMainMenu"

void UKnightMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ValidateRequiredWidget(Panel_Home, TEXT("Panel_Home"));
	ValidateRequiredWidget(Panel_Stage, TEXT("Panel_Stage"));
	ValidateRequiredWidget(Globe_Stages, TEXT("Globe_Stages"));
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
	if (Globe_Stages)
	{
		Globe_Stages->OnStageClicked.AddUniqueDynamic(this, &ThisClass::HandleGlobeStageClicked);
		Globe_Stages->SetLeaderTarget(Panel_StageInfo);
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
	if (RaidDefinitionHandle.IsValid())
	{
		RaidDefinitionHandle->CancelHandle();
		RaidDefinitionHandle.Reset();
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
	switch (NewPage)
	{
	case EKnightMainMenuPage::Home: Page = Panel_Home; break;
	case EKnightMainMenuPage::Stage: Page = Panel_Stage; break;
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
	const bool bEnteringStage = NewPage == EKnightMainMenuPage::Stage && CurrentPage != EKnightMainMenuPage::Stage;
	CurrentPage = NewPage;
	if (NewPage != EKnightMainMenuPage::Home) bRestoreCodexFocus = false;
	bool bHasStages = true;
	if (bEnteringStage)
	{
		// Unlocks can change between visits (a clear unlocks the next stage), so rebuild every time.
		SelectedStageId = NAME_None;
		if (Globe_Stages) Globe_Stages->SetSelectedStage(NAME_None);
		bHasStages = RebuildGlobeMarkers();
		RefreshStageDetails();
		PlaceStageInfo();
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
	return true;
}

bool UKnightMainMenuWidget::HandleBackRequested()
{
	if (CurrentPage == EKnightMainMenuPage::Settings && SettingsWidget) { SettingsWidget->RequestClose(); return true; }
	if (CurrentPage == EKnightMainMenuPage::Codex) { HandleCodexClosed(); return true; }
	// First Esc clears the selected region, the next one leaves the globe.
	if (CurrentPage == EKnightMainMenuPage::Stage && !SelectedStageId.IsNone())
	{
		return SelectStage(NAME_None);
	}
	return CurrentPage == EKnightMainMenuPage::Home ? ShowPage(EKnightMainMenuPage::Settings) : ShowPage(EKnightMainMenuPage::Home);
}

FReply UKnightMainMenuWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Left / Right walk the regions on the globe.
	if (CurrentPage == EKnightMainMenuPage::Stage && !IsTravelPending())
	{
		const FKey Key = InKeyEvent.GetKey();
		if (Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Right || Key == EKeys::D)
		{
			CycleStage(Key == EKeys::Left || Key == EKeys::A ? -1 : 1);
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

void UKnightMainMenuWidget::HandleCodexClosed()
{
	bRestoreCodexFocus = true;
	ShowPage(EKnightMainMenuPage::Home);
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
	if (Globe_Stages) Globe_Stages->SetIsEnabled(bEnabled);
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

bool UKnightMainMenuWidget::RebuildGlobeMarkers()
{
	StageOrder.Reset();
	UKnightGameFlowSubsystem* const Flow = GetFlow();
	TArray<FKnightStageDefinition> Stages;
	if (!Globe_Stages || !Flow || !Flow->GetStageDefinitions(Stages))
	{
		if (Globe_Stages) Globe_Stages->SetMarkers({});
		return false;
	}
	TArray<FKnightGlobeMarker> Markers;
	for (const FKnightStageDefinition& Stage : Stages)
	{
		FKnightGlobeMarker& Marker = Markers.AddDefaulted_GetRef();
		Marker.StageId = Stage.StageId;
		Marker.Label = Stage.bDevelopmentOnly ? FText::Format(LOCTEXT("DevStageName", "[DEV] {0}"), Stage.DisplayName) : Stage.DisplayName;
		Marker.Location = Stage.GlobeLocation;
		FText Reason;
		Marker.bLocked = !Flow->CanLaunchStage(Stage.StageId, Reason);
		StageOrder.Add(Stage.StageId);
	}
	Globe_Stages->SetMarkers(Markers);
	return !StageOrder.IsEmpty();
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
	if (!StageId.IsNone() && !Flow->FindStageDefinition(StageId, Stage))
	{
		SetStatusText(LOCTEXT("UnknownStage", "스테이지 정보를 확인할 수 없습니다."));
		return false;
	}
	if (CurrentPage != EKnightMainMenuPage::Stage && !ShowPage(EKnightMainMenuPage::Stage))
	{
		return false;
	}
	SelectedStageId = StageId;
	if (Globe_Stages) Globe_Stages->SetSelectedStage(StageId);
	ClearStatus();

	// The panel goes to the side the region is on, so the leader line stays short.
	FVector2D MarkerPosition;
	bool bFront = false;
	if (Globe_Stages && Globe_Stages->GetMarkerLocalPosition(StageId, MarkerPosition, bFront) && bFront)
	{
		bStageInfoOnRight = MarkerPosition.X >= Globe_Stages->GetCachedGeometry().GetLocalSize().X * 0.5f;
	}
	RefreshStageDetails();
	PlaceStageInfo();
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
	if (Globe_Stages) Globe_Stages->FocusStage(StageOrder[Next]);
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
	const UCanvasPanelSlot* const GlobeSlot = Globe_Stages ? Cast<UCanvasPanelSlot>(Globe_Stages->Slot) : nullptr;
	if (!InfoSlot || !GlobeSlot)
	{
		return;
	}
	// Both are anchored to the screen centre; the panel hangs off the globe's left or right edge.
	const float GlobeHalfWidth = GlobeSlot->GetSize().X * 0.5f;
	const float Offset = GlobeHalfWidth + StageInfoGap;
	InfoSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	InfoSlot->SetAlignment(FVector2D(bStageInfoOnRight ? 0.f : 1.f, 0.5f));
	InfoSlot->SetPosition(FVector2D(GlobeSlot->GetPosition().X + (bStageInfoOnRight ? Offset : -Offset), GlobeSlot->GetPosition().Y));
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

void UKnightMainMenuWidget::HandleGlobeStageClicked(FName StageId)
{
	SelectStage(StageId);
}

void UKnightMainMenuWidget::HandleLaunchStageClicked()
{
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

#undef LOCTEXT_NAMESPACE
