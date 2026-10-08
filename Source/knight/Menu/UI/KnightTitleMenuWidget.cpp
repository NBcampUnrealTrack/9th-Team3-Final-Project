#include "KnightTitleMenuWidget.h"

#include "GameFlow/KnightGameFlowSubsystem.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Kismet/KismetSystemLibrary.h"
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
	ValidateRequiredWidget(Text_Slot1, TEXT("Text_Slot1"));
	ValidateRequiredWidget(Text_Slot2, TEXT("Text_Slot2"));
	ValidateRequiredWidget(Text_Slot3, TEXT("Text_Slot3"));
	ValidateRequiredWidget(Button_Settings, TEXT("Button_Settings"));
	ValidateRequiredWidget(Button_Quit, TEXT("Button_Quit"));
	if (Button_StartGame) Button_StartGame->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStartGameClicked);
	if (Button_SaveSelect) Button_SaveSelect->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSaveSelectClicked);
	if (Button_Slot1) Button_Slot1->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSlot1Clicked);
	if (Button_Slot2) Button_Slot2->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSlot2Clicked);
	if (Button_Slot3) Button_Slot3->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSlot3Clicked);
	if (Button_Settings) Button_Settings->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSettingsClicked);
	if (Button_Quit) Button_Quit->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleQuitClicked);
}

void UKnightTitleMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyPage(EKnightTitlePage::Home, true);
	if (UknightSaveSubsystem* const Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr)
	{
		Save->OnSaveFinished.AddUniqueDynamic(this, &ThisClass::HandleSaveFinished);
		if (Save->IsSaveInFlight()) SetStatusText(LOCTEXT("SaveBusy", "저장 작업이 끝날 때까지 기다려 주세요."));
	}
	RefreshSlots();
	RefreshTravelControls();
	if (UknightGameUserSettings* const Graphics = UknightGameUserSettings::Get(); Graphics && Graphics->ConsumeGraphicsAppliedNotice())
		SetStatusText(LOCTEXT("GraphicsApplied", "새 그래픽 설정이 적용되었습니다."));
}

void UKnightTitleMenuWidget::NativeDestruct()
{
	if (SettingsWidget) SettingsWidget->OnClosed.RemoveDynamic(this, &ThisClass::HandleSettingsClosed);
	if (UknightSaveSubsystem* const Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr)
	{
		Save->OnSaveFinished.RemoveDynamic(this, &ThisClass::HandleSaveFinished);
	}
	Super::NativeDestruct();
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
		if (SettingsWidget) SettingsWidget->Open();
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

void UKnightTitleMenuWidget::EnsureSettingsWidget()
{
	if (SettingsWidget || !Panel_Settings || !GetOwningPlayer()) return;
	SettingsWidget = CreateWidget<UKnightSettingsWidget>(GetOwningPlayer(), UKnightSettingsWidget::GetSharedWidgetClass());
	if (!SettingsWidget) return;
	Panel_Settings->ClearChildren();
	Panel_Settings->AddChild(SettingsWidget);
	SettingsWidget->OnClosed.AddUniqueDynamic(this, &ThisClass::HandleSettingsClosed);
}

void UKnightTitleMenuWidget::HandleSettingsClosed() { ShowPage(EKnightTitlePage::Home); }

UWidget* UKnightTitleMenuWidget::GetPreferredFocusTarget() const
{
	if (CurrentPage != EKnightTitlePage::Home)
	{
		if (CurrentPage == EKnightTitlePage::SaveSelect && Button_Slot1 && Button_Slot1->GetIsEnabled()) return Button_Slot1;
		return Button_Back;
	}
	for (UButton* Button : { Button_StartGame.Get(), Button_SaveSelect.Get(), Button_Settings.Get(), Button_Quit.Get() })
	{
		if (Button && Button->GetIsFocusable() && Button->IsVisible() && Button->GetIsEnabled())
		{
			return Button;
		}
	}
	return nullptr;
}

void UKnightTitleMenuWidget::SetTravelControlsEnabled(bool bEnabled)
{
	Super::SetTravelControlsEnabled(bEnabled);
	const UknightSaveSubsystem* const Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	const bool bReady = bEnabled && !bAwaitingInitialSave && !(Save && Save->IsSaveInFlight());
	if (Button_StartGame) Button_StartGame->SetIsEnabled(bReady && GetFlow() != nullptr);
	if (Button_SaveSelect) Button_SaveSelect->SetIsEnabled(bReady);
	if (Button_Settings) Button_Settings->SetIsEnabled(bReady);
	if (Button_Quit) Button_Quit->SetIsEnabled(bReady);
	for (UButton* Button : { Button_Slot1.Get(), Button_Slot2.Get(), Button_Slot3.Get() })
	{
		if (Button) Button->SetIsEnabled(bReady);
	}
	if (Button_Back) Button_Back->SetIsEnabled(bReady && CurrentPage != EKnightTitlePage::Home);
}

void UKnightTitleMenuWidget::HandleStartGameClicked()
{
	ShowPage(EKnightTitlePage::SaveSelect);
}

void UKnightTitleMenuWidget::HandleSaveSelectClicked()
{
	ShowPage(EKnightTitlePage::SaveSelect);
}

void UKnightTitleMenuWidget::HandleSlot1Clicked() { SelectFixedSlot(1); }
void UKnightTitleMenuWidget::HandleSlot2Clicked() { SelectFixedSlot(2); }
void UKnightTitleMenuWidget::HandleSlot3Clicked() { SelectFixedSlot(3); }

void UKnightTitleMenuWidget::RefreshSlots()
{
	const UknightSaveSubsystem* const Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	UTextBlock* Labels[] = { Text_Slot1.Get(), Text_Slot2.Get(), Text_Slot3.Get() };
	for (int32 Number = 1; Number <= 3; ++Number)
	{
		if (!Labels[Number - 1]) continue;
		bool bExists = false;
		bool bReadable = false;
		float Seconds = 0.f;
		if (!Save || !Save->GetFixedSlotInfo(Number, bExists, bReadable, Seconds)) continue;
		FString Description;
		if (!bExists)
		{
			Description = FString::Printf(TEXT("세이브 %d / 빈 슬롯 / 새 게임"), Number);
		}
		else if (!bReadable)
		{
			Description = FString::Printf(TEXT("세이브 %d / 저장 파일 오류 / 재시도"), Number);
		}
		else
		{
			const int64 WholeSeconds = FMath::Max<int64>(0, FMath::FloorToInt64(Seconds));
			Description = FString::Printf(TEXT("세이브 %d / 플레이타임 %02lld:%02lld:%02lld / 이어하기"),
				Number, WholeSeconds / 3600, (WholeSeconds / 60) % 60, WholeSeconds % 60);
		}
		Labels[Number - 1]->SetText(FText::FromString(Description));
	}
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
