#include "KnightTitleMenuWidget.h"

#include "GameFlow/KnightGameFlowSubsystem.h"
#include "CommonButtonBase.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "KnightSaveSlotCard.h"
#include "KnightMenuVideoBackgroundWidget.h"
#include "LoadingScreenManager.h"
#include "Misc/Paths.h"
#include "Materials/MaterialInterface.h"
#include "Progression/knightSaveGame.h"
#include "Progression/knightSaveSubsystem.h"
#include "knight.h"
#include "Settings/KnightSettingsWidget.h"
#include "Settings/knightGameUserSettings.h"

#define LOCTEXT_NAMESPACE "KnightTitleMenu"

void UKnightTitleMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ValidateRequiredWidget(Panel_Home, TEXT("Panel_Home"));
	ValidateRequiredWidget(Panel_SaveSelect, TEXT("Panel_SaveSelect"));
	ValidateRequiredWidget(Panel_Settings, TEXT("Panel_Settings"));
	ValidateRequiredWidget(Button_StartGame, TEXT("Button_StartGame"));
	ValidateRequiredWidget(Button_SaveSelect, TEXT("Button_SaveSelect"));
	ValidateRequiredWidget(Button_Slot1, TEXT("Button_Slot1"));
	ValidateRequiredWidget(Button_Slot2, TEXT("Button_Slot2"));
	ValidateRequiredWidget(Button_Slot3, TEXT("Button_Slot3"));
	ValidateRequiredWidget(Button_Settings, TEXT("Button_Settings"));
	ValidateRequiredWidget(Button_Quit, TEXT("Button_Quit"));
	if (Button_StartGame) Button_StartGame->OnClicked().AddUObject(this, &ThisClass::HandleStartGameClicked);
	if (Button_SaveSelect) Button_SaveSelect->OnClicked().AddUObject(this, &ThisClass::HandleSaveSelectClicked);
	if (Button_Slot1) Button_Slot1->OnClicked().AddUObject(this, &ThisClass::HandleSlot1Clicked);
	if (Button_Slot2) Button_Slot2->OnClicked().AddUObject(this, &ThisClass::HandleSlot2Clicked);
	if (Button_Slot3) Button_Slot3->OnClicked().AddUObject(this, &ThisClass::HandleSlot3Clicked);
	if (Button_Settings) Button_Settings->OnClicked().AddUObject(this, &ThisClass::HandleSettingsClicked);
	if (Button_Quit) Button_Quit->OnClicked().AddUObject(this, &ThisClass::HandleQuitClicked);
}

void UKnightTitleMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Decided before the first page focus, so focus stays on the widget itself until the menu is shown.
	const bool bCanWaitForKey = Txt_PressKey && VerticalBox_Menu;
	UKnightGameFlowSubsystem* const Flow = GetFlow();
	if (bCanWaitForKey && Panel_Intro && Img_StudioLogo && Img_EngineLogo && Flow && Flow->ConsumeStartupIntro())
	{
		EnterIntroPhase(EKnightTitleIntroPhase::StudioLogo);
	}
	else
	{
		EnterIntroPhase(bCanWaitForKey ? EKnightTitleIntroPhase::PressKey : EKnightTitleIntroPhase::Menu);
	}
	ApplyPage(EKnightTitlePage::Home, true);
	UpdateVideoBackground();
	if (UknightSaveSubsystem* const Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr)
	{
		Save->OnSaveFinished.AddUniqueDynamic(this, &ThisClass::HandleSaveFinished);
		if (Save->IsSaveInFlight()) SetStatusText(LOCTEXT("SaveBusy", "저장 작업이 끝날 때까지 기다려 주세요."));
	}
	RefreshSlots();
	RefreshTravelControls();
	if (UknightGameUserSettings* const Graphics = UknightGameUserSettings::Get(); Graphics && Graphics->ConsumeGraphicsRevertedNotice())
		SetStatusText(LOCTEXT("GraphicsReverted", "유지하지 않은 화면 설정을 이전 값으로 되돌렸습니다."));
}

void UKnightTitleMenuWidget::NativeDestruct()
{
	if (VideoBackgroundWidget) VideoBackgroundWidget->Stop();
	if (SettingsWidget)
	{
		SettingsWidget->OnClosed.RemoveDynamic(this, &ThisClass::HandleSettingsClosed);
		SettingsWidget->RemoveFromParent();
	}
	if (UknightSaveSubsystem* const Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr)
	{
		Save->OnSaveFinished.RemoveDynamic(this, &ThisClass::HandleSaveFinished);
	}
	Super::NativeDestruct();
}

void UKnightTitleMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!IsMenuShown())
	{
		TickIntro(InDeltaTime);
	}
}

FReply UKnightTitleMenuWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Before the menu every key only moves the intro on, Escape included (it would open settings otherwise).
	if (!IsMenuShown())
	{
		if (!InKeyEvent.IsRepeat()) AdvanceIntro();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UKnightTitleMenuWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!IsMenuShown())
	{
		AdvanceIntro();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

void UKnightTitleMenuWidget::EnterIntroPhase(EKnightTitleIntroPhase NewPhase)
{
	IntroPhase = NewPhase;
	IntroPhaseTime = 0.f;
	const bool bLogos = NewPhase == EKnightTitleIntroPhase::StudioLogo || NewPhase == EKnightTitleIntroPhase::EngineLogo;
	if (Panel_Intro) Panel_Intro->SetVisibility(IsMenuShown() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	if (Img_IntroBackdrop) Img_IntroBackdrop->SetRenderOpacity(bLogos || NewPhase == EKnightTitleIntroPhase::FadeIn ? 1.f : 0.f);
	// Both start hidden; TickIntro shows the one whose turn it is.
	if (Img_StudioLogo) Img_StudioLogo->SetRenderOpacity(0.f);
	if (Img_EngineLogo) Img_EngineLogo->SetRenderOpacity(0.f);
	if (Txt_PressKey)
	{
		Txt_PressKey->SetVisibility(IsMenuShown() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		Txt_PressKey->SetRenderOpacity(1.f);
	}
	if (VerticalBox_Menu)
	{
		const bool bItemsShown = NewPhase == EKnightTitleIntroPhase::Reveal || IsMenuShown();
		VerticalBox_Menu->SetVisibility(bItemsShown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
		SetMenuItemsReveal(IsMenuShown() ? TNumericLimits<float>::Max() : 0.f);
	}
	if (NewPhase == EKnightTitleIntroPhase::Reveal && PressKeySound)
	{
		UGameplayStatics::PlaySound2D(this, PressKeySound);
	}
	FocusCurrentPage();
}

void UKnightTitleMenuWidget::AdvanceIntro()
{
	switch (IntroPhase)
	{
	case EKnightTitleIntroPhase::StudioLogo:
	case EKnightTitleIntroPhase::EngineLogo:
	case EKnightTitleIntroPhase::FadeIn:
		EnterIntroPhase(EKnightTitleIntroPhase::PressKey);
		break;
	case EKnightTitleIntroPhase::PressKey:
		EnterIntroPhase(EKnightTitleIntroPhase::Reveal);
		break;
	default:
		// Reveal: the items are already coming in; input waits for them.
		break;
	}
}

void UKnightTitleMenuWidget::TickIntro(float DeltaTime)
{
	// At startup the loading screen stays up a moment after the title is ready; the intro waits for it.
	const ULoadingScreenManager* const LoadingScreen = GetGameInstance() ? GetGameInstance()->GetSubsystem<ULoadingScreenManager>() : nullptr;
	if (LoadingScreen && LoadingScreen->GetLoadingScreenDisplayStatus())
	{
		return;
	}
	IntroPhaseTime += DeltaTime;
	const float LogoTime = LogoFadeTime * 2.f + LogoHoldTime + LogoGapTime;
	switch (IntroPhase)
	{
	case EKnightTitleIntroPhase::StudioLogo:
	case EKnightTitleIntroPhase::EngineLogo:
	{
		const bool bStudio = IntroPhase == EKnightTitleIntroPhase::StudioLogo;
		ShowLogo(bStudio ? Img_StudioLogo : Img_EngineLogo, IntroPhaseTime);
		if (IntroPhaseTime >= LogoTime)
		{
			EnterIntroPhase(bStudio ? EKnightTitleIntroPhase::EngineLogo : EKnightTitleIntroPhase::FadeIn);
		}
		break;
	}
	case EKnightTitleIntroPhase::FadeIn:
		if (Img_IntroBackdrop) Img_IntroBackdrop->SetRenderOpacity(1.f - FMath::SmoothStep(0.f, LogoFadeTime, IntroPhaseTime));
		if (IntroPhaseTime >= LogoFadeTime) EnterIntroPhase(EKnightTitleIntroPhase::PressKey);
		break;
	case EKnightTitleIntroPhase::PressKey:
		if (Txt_PressKey)
		{
			const float Wave = 0.5f + 0.5f * FMath::Cos(IntroPhaseTime * UE_TWO_PI / PressKeyBlinkPeriod);
			Txt_PressKey->SetRenderOpacity(0.35f + 0.65f * Wave);
		}
		break;
	case EKnightTitleIntroPhase::Reveal:
	{
		if (Txt_PressKey)
		{
			// Fades out from wherever the blink was.
			Txt_PressKey->SetRenderOpacity(FMath::Min(Txt_PressKey->GetRenderOpacity(), 1.f - IntroPhaseTime / PressKeyFadeOutTime));
		}
		const float ItemTime = IntroPhaseTime - PressKeyFadeOutTime;
		SetMenuItemsReveal(ItemTime);
		const int32 ItemCount = VerticalBox_Menu ? VerticalBox_Menu->GetChildrenCount() : 0;
		if (ItemTime >= FMath::Max(ItemCount - 1, 0) * ItemRevealStagger + ItemRevealTime)
		{
			EnterIntroPhase(EKnightTitleIntroPhase::Menu);
		}
		break;
	}
	default:
		break;
	}
}

void UKnightTitleMenuWidget::ShowLogo(UWidget* Logo, float Time) const
{
	if (!Logo) return;
	const float FadeOutStart = LogoFadeTime + LogoHoldTime;
	const float Alpha = Time < LogoFadeTime ? FMath::SmoothStep(0.f, LogoFadeTime, Time)
		: 1.f - FMath::SmoothStep(FadeOutStart, FadeOutStart + LogoFadeTime, Time);
	const float Grow = FMath::Clamp(Time / (FadeOutStart + LogoFadeTime), 0.f, 1.f);
	Logo->SetRenderOpacity(Alpha);
	Logo->SetRenderScale(FVector2D(FMath::Lerp(LogoStartScale, 1.f, 1.f - FMath::Square(1.f - Grow))));
}

void UKnightTitleMenuWidget::SetMenuItemsReveal(float Time) const
{
	if (!VerticalBox_Menu) return;
	for (int32 Index = 0; Index < VerticalBox_Menu->GetChildrenCount(); ++Index)
	{
		UWidget* const Item = VerticalBox_Menu->GetChildAt(Index);
		const float Linear = FMath::Clamp((Time - Index * ItemRevealStagger) / ItemRevealTime, 0.f, 1.f);
		const float Eased = 1.f - FMath::Cube(1.f - Linear);
		Item->SetRenderOpacity(Eased);
		Item->SetRenderTranslation(FVector2D(ItemRevealOffset * (1.f - Eased), 0.f));
	}
}

bool UKnightTitleMenuWidget::ShowPage(EKnightTitlePage NewPage)
{
	return ApplyPage(NewPage, false);
}

bool UKnightTitleMenuWidget::ApplyPage(EKnightTitlePage NewPage, bool bAllowDuringTravel)
{
	if (!bAllowDuringTravel && (IsTravelPending() || bAwaitingInitialSave))
	{
		return false;
	}
	UPanelWidget* Page = nullptr;
	switch (NewPage)
	{
	case EKnightTitlePage::Home: Page = Panel_Home; break;
	case EKnightTitlePage::SaveSelect: Page = Panel_SaveSelect; break;
	case EKnightTitlePage::Settings: Page = Panel_Settings; break;
	default:
		UE_LOG(Logknight, Error, TEXT("Invalid title page %d."), static_cast<int32>(NewPage));
		return false;
	}
	if (!ActivatePage(Page))
	{
		return false;
	}
	CurrentPage = NewPage;
	if (NewPage == EKnightTitlePage::Settings)
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
	UpdateBackVisibility(NewPage == EKnightTitlePage::Home || NewPage == EKnightTitlePage::Settings);
	RefreshTravelControls();
	if (!IsTravelPending() && GetFlow() && GetFlow()->GetCurrentFlowState() == EKnightFlowState::Title)
	{
		ClearStatus();
	}
	FocusCurrentPage();
	if (NewPage == EKnightTitlePage::Settings && SettingsWidget) SettingsWidget->FocusSettings();
	if (NewPage == EKnightTitlePage::SaveSelect) RefreshSlots();
	return true;
}

bool UKnightTitleMenuWidget::HandleBackRequested()
{
	if (CurrentPage == EKnightTitlePage::Settings && SettingsWidget) { SettingsWidget->RequestClose(); return true; }
	return CurrentPage == EKnightTitlePage::Home ? ShowPage(EKnightTitlePage::Settings) : ShowPage(EKnightTitlePage::Home);
}

bool UKnightTitleMenuWidget::ShouldAllowEscapeToChild() const
{
	return CurrentPage == EKnightTitlePage::Settings && SettingsWidget && SettingsWidget->IsCapturingKey();
}

void UKnightTitleMenuWidget::EnsureVideoBackground()
{
	if (VideoBackgroundWidget || !bUseVideoBackground || !GetOwningPlayer()) return;
	UCanvasPanel* const Root = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
	if (!Root)
	{
		UE_LOG(Logknight, Warning, TEXT("Title video requires a CanvasPanel root; leaving the existing background unchanged."));
		return;
	}
	VideoBackgroundWidget = CreateWidget<UKnightMenuVideoBackgroundWidget>(
		GetOwningPlayer(), UKnightMenuVideoBackgroundWidget::StaticClass());
	if (!VideoBackgroundWidget) return;
	UCanvasPanelSlot* const BackgroundSlot = Root->AddChildToCanvas(VideoBackgroundWidget);
	BackgroundSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	BackgroundSlot->SetOffsets(FMargin(0.f));
	BackgroundSlot->SetAlignment(FVector2D::ZeroVector);
	BackgroundSlot->SetZOrder(-1000); // Below the intro, title pages, buttons and focus targets.
	UMaterialInterface* const Material = BackgroundVideoMaterial
		? BackgroundVideoMaterial.Get()
		: LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Knight/UI/Menu/M_UI_MenuVideo.M_UI_MenuVideo"));
	VideoBackgroundWidget->Configure(bUseVideoBackground, BackgroundMediaSource, bLoopBackgroundVideo,
		FallbackBackgroundImage, BackgroundDimOpacity, Material);
}

void UKnightTitleMenuWidget::UpdateVideoBackground()
{
	const UKnightGameFlowSubsystem* const Flow = GetFlow();
	if (!bUseVideoBackground || IsTravelPending() || !Flow || Flow->GetCurrentFlowState() != EKnightFlowState::Title)
	{
		if (VideoBackgroundWidget) VideoBackgroundWidget->Stop();
		return;
	}
	EnsureVideoBackground();
	if (VideoBackgroundWidget) VideoBackgroundWidget->Start();
}

void UKnightTitleMenuWidget::EnsureSettingsWidget()
{
	if (SettingsWidget || !Panel_Settings || !GetOwningPlayer()) return;
	SettingsWidget = CreateWidget<UKnightSettingsWidget>(GetOwningPlayer(), UKnightSettingsWidget::GetSharedWidgetClass());
	if (!SettingsWidget) return;
	// The page stays as the menu's "settings" state but shows nothing; its placeholder would show through.
	Panel_Settings->ClearChildren();
	SettingsWidget->OnClosed.AddUniqueDynamic(this, &ThisClass::HandleSettingsClosed);
}

void UKnightTitleMenuWidget::HandleSettingsClosed() { ShowPage(EKnightTitlePage::Home); }

UWidget* UKnightTitleMenuWidget::GetPreferredFocusTarget() const
{
	if (!IsMenuShown())
	{
		// The menu buttons stay visible inside a hidden box, so they would still pass CanFocusButton.
		return const_cast<UKnightTitleMenuWidget*>(this);
	}
	if (CurrentPage != EKnightTitlePage::Home)
	{
		if (CurrentPage == EKnightTitlePage::SaveSelect)
		{
			// Start on the slot being played, otherwise the first one.
			for (UKnightSaveSlotCard* Card : { Button_Slot1.Get(), Button_Slot2.Get(), Button_Slot3.Get() })
			{
				if (Card && Card->GetSlotView().bActive && CanFocusButton(Card)) return Card;
			}
			if (CanFocusButton(Button_Slot1)) return Button_Slot1;
		}
		return Button_Back;
	}
	for (UCommonButtonBase* Button : { Button_StartGame.Get(), Button_SaveSelect.Get(), Button_Settings.Get(), Button_Quit.Get() })
	{
		if (CanFocusButton(Button))
		{
			return Button;
		}
	}
	return nullptr;
}

void UKnightTitleMenuWidget::SetTravelControlsEnabled(bool bEnabled)
{
	Super::SetTravelControlsEnabled(bEnabled);
	UpdateVideoBackground();
	const UknightSaveSubsystem* const Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	const bool bReady = bEnabled && !bAwaitingInitialSave && !(Save && Save->IsSaveInFlight());
	if (Button_StartGame) Button_StartGame->SetIsEnabled(bReady && GetFlow() != nullptr);
	if (Button_SaveSelect) Button_SaveSelect->SetIsEnabled(bReady);
	if (Button_Settings) Button_Settings->SetIsEnabled(bReady);
	if (Button_Quit) Button_Quit->SetIsEnabled(bReady);
	for (UKnightSaveSlotCard* Button : { Button_Slot1.Get(), Button_Slot2.Get(), Button_Slot3.Get() })
	{
		if (Button) Button->SetIsEnabled(bReady);
	}
	if (Button_Back) Button_Back->SetIsEnabled(bReady && CurrentPage != EKnightTitlePage::Home);
}

void UKnightTitleMenuWidget::HandleStartGameClicked()
{
	// Slot choice is asked only the first time; after that the last played slot continues.
	if (const int32 Latest = FindLatestSlot(); Latest > 0)
	{
		SelectFixedSlot(Latest);
		return;
	}
	ShowPage(EKnightTitlePage::SaveSelect);
}

void UKnightTitleMenuWidget::HandleSaveSelectClicked()
{
	ShowPage(EKnightTitlePage::SaveSelect);
}

void UKnightTitleMenuWidget::HandleSlot1Clicked() { SelectFixedSlot(1); }
void UKnightTitleMenuWidget::HandleSlot2Clicked() { SelectFixedSlot(2); }
void UKnightTitleMenuWidget::HandleSlot3Clicked() { SelectFixedSlot(3); }

namespace
{
	/**
	 * Read-only access to a fixed slot's file. Mirrors the slot names in UknightSaveSubsystem (1 = KnightProgress,
	 * 2/3 = KnightProgress_Slot2/3). Editor save tests use their own namespace and simply show as empty here.
	 */
	FString FixedSlotFileName(int32 SlotNumber)
	{
		return SlotNumber == 1 ? FString(TEXT("KnightProgress")) : FString::Printf(TEXT("KnightProgress_Slot%d"), SlotNumber);
	}

	/** Local time the slot was last written; the save has no timestamp of its own. */
	FDateTime GetSlotFileTime(int32 SlotNumber)
	{
		const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), FixedSlotFileName(SlotNumber) + TEXT(".sav"));
		const FDateTime Utc = IFileManager::Get().GetTimeStamp(*Path);
		return Utc == FDateTime::MinValue() ? Utc : Utc + (FDateTime::Now() - FDateTime::UtcNow());
	}
}

void UKnightTitleMenuWidget::RefreshSlots()
{
	const UknightSaveSubsystem* const Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	// "Game start" continues this slot, so the card marks it.
	const int32 LatestSlot = FindLatestSlot();
	UKnightSaveSlotCard* const Cards[] = { Button_Slot1.Get(), Button_Slot2.Get(), Button_Slot3.Get() };
	for (int32 Number = 1; Number <= 3; ++Number)
	{
		UKnightSaveSlotCard* const Card = Cards[Number - 1];
		bool bExists = false;
		bool bReadable = false;
		float Seconds = 0.f;
		if (!Card || !Save || !Save->GetFixedSlotInfo(Number, bExists, bReadable, Seconds)) continue;

		FKnightSaveSlotView View;
		View.SlotNumber = Number;
		View.State = !bExists ? EKnightSaveSlotState::Empty : (bReadable ? EKnightSaveSlotState::Ready : EKnightSaveSlotState::Unreadable);
		if (View.State == EKnightSaveSlotState::Ready)
		{
			View.PlayTimeSeconds = Seconds;
			View.LastPlayed = GetSlotFileTime(Number);
			const UknightSaveGame* const Data = Cast<UknightSaveGame>(UGameplayStatics::LoadGameFromSlot(FixedSlotFileName(Number), UknightSaveSubsystem::UserIndex));
			if (Data)
			{
				for (const TPair<FName, FknightStageRecord>& Record : Data->StageRecords)
				{
					if (Record.Value.ClearCount > 0)
					{
						++View.ClearedStages;
						View.TotalClears += Record.Value.ClearCount;
					}
				}
			}
			View.bActive = Number == LatestSlot;
		}
		Card->SetSlotView(View);
	}
}

int32 UKnightTitleMenuWidget::FindLatestSlot() const
{
	const UknightSaveSubsystem* const Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	int32 Latest = 0;
	FDateTime LatestTime = FDateTime::MinValue();
	for (int32 Number = 1; Number <= 3 && Save; ++Number)
	{
		bool bExists = false;
		bool bReadable = false;
		float Seconds = 0.f;
		if (!Save->GetFixedSlotInfo(Number, bExists, bReadable, Seconds) || !bExists || !bReadable) continue;
		const FDateTime FileTime = GetSlotFileTime(Number);
		if (Latest == 0 || FileTime > LatestTime)
		{
			Latest = Number;
			LatestTime = FileTime;
		}
	}
	return Latest;
}

void UKnightTitleMenuWidget::SelectFixedSlot(int32 SlotNumber)
{
	UknightSaveSubsystem* const Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	if (!Save || IsTravelPending() || bAwaitingInitialSave || Save->IsSaveInFlight())
	{
		SetStatusText(LOCTEXT("ProfileBusy", "저장 작업이 끝날 때까지 기다려 주세요."));
		return;
	}
	bool bExisting = false;
	bool bReadable = false;
	float Seconds = 0.f;
	if (!Save->GetFixedSlotInfo(SlotNumber, bExisting, bReadable, Seconds)) return;
	if (bExisting && !bReadable)
	{
		SetStatusText(LOCTEXT("ProfileUnreadable", "저장 파일을 읽을 수 없습니다. 원본은 보존됩니다. 다시 시도하거나 다른 슬롯을 선택해 주세요."));
		RefreshSlots();
		return;
	}
	SetStatusText(LOCTEXT("LoadingProfile", "프로필을 불러오는 중입니다."));
	if (!Save->SelectFixedSlot(SlotNumber))
	{
		SetStatusText(LOCTEXT("ProfileUnreadable", "저장 파일을 읽을 수 없습니다. 원본은 보존됩니다. 다시 시도하거나 다른 슬롯을 선택해 주세요."));
		RefreshSlots();
		return;
	}
	if (bExisting)
	{
		OpenLobby();
		return;
	}
	bAwaitingInitialSave = true;
	SetStatusText(LOCTEXT("CreatingProfile", "새 게임을 저장하는 중입니다."));
	RefreshTravelControls();
	Save->RequestSave();
}

void UKnightTitleMenuWidget::HandleSaveFinished(bool bSuccess)
{
	const bool bWasCreating = bAwaitingInitialSave;
	bAwaitingInitialSave = false;
	RefreshTravelControls();
	RefreshSlots();
	if (!bWasCreating)
	{
		if (bSuccess) ClearStatus();
		else SetStatusText(LOCTEXT("BackgroundSaveFailed", "저장하지 못했습니다. 잠시 후 다시 시도해 주세요."));
		return;
	}
	if (!bSuccess)
	{
		SetStatusText(LOCTEXT("CreateFailed", "프로필을 저장하지 못했습니다. 같은 슬롯으로 다시 시도할 수 있습니다."));
		return;
	}
	OpenLobby();
}

void UKnightTitleMenuWidget::OpenLobby()
{
	UKnightGameFlowSubsystem* const Flow = GetFlow();
	if (!Flow || !Flow->RequestOpenMainMenu())
	{
		SetStatusText(LOCTEXT("LobbyOpenFailed", "로비를 열지 못했습니다. 다시 시도해 주세요."));
		return;
	}
	if (VideoBackgroundWidget) VideoBackgroundWidget->Stop();
	RefreshTravelControls();
	SetStatusText(LOCTEXT("OpeningMainMenu", "메인메뉴로 이동 중입니다."));
}

void UKnightTitleMenuWidget::HandleSettingsClicked()
{
	ShowPage(EKnightTitlePage::Settings);
}

void UKnightTitleMenuWidget::HandleQuitClicked()
{
	if (IsTravelPending())
	{
		return;
	}
	APlayerController* const OwningPlayer = GetOwningPlayer();
	if (!OwningPlayer || !OwningPlayer->IsLocalController())
	{
		UE_LOG(Logknight, Error, TEXT("Title Quit has no local owning PlayerController."));
		SetStatusText(LOCTEXT("NoOwningPlayer", "게임을 종료할 수 없습니다."));
		return;
	}
	UKismetSystemLibrary::QuitGame(this, OwningPlayer, EQuitPreference::Quit, false);
}

#undef LOCTEXT_NAMESPACE
