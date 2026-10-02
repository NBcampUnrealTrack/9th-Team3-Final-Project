#include "KnightTitleMenuWidget.h"

#include "GameFlow/KnightGameFlowSubsystem.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Progression/knightSaveSubsystem.h"
#include "knight.h"

#define LOCTEXT_NAMESPACE "KnightTitleMenu"

void UKnightTitleMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ValidateRequiredWidget(Panel_Home, TEXT("Panel_Home"));
	ValidateRequiredWidget(Panel_SaveSelect, TEXT("Panel_SaveSelect"));
	ValidateRequiredWidget(Panel_Settings, TEXT("Panel_Settings"));
	ValidateRequiredWidget(Button_StartGame, TEXT("Button_StartGame"));
	ValidateRequiredWidget(Button_SaveSelect, TEXT("Button_SaveSelect"));
	ValidateRequiredWidget(Input_SlotId, TEXT("Input_SlotId"));
	ValidateRequiredWidget(Button_UseSlot, TEXT("Button_UseSlot"));
	ValidateRequiredWidget(Text_SlotInfo, TEXT("Text_SlotInfo"));
	ValidateRequiredWidget(Button_Settings, TEXT("Button_Settings"));
	ValidateRequiredWidget(Button_Quit, TEXT("Button_Quit"));
	if (Button_StartGame) Button_StartGame->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStartGameClicked);
	if (Button_SaveSelect) Button_SaveSelect->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSaveSelectClicked);
	if (Button_UseSlot) Button_UseSlot->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleUseSlotClicked);
	if (Input_SlotId) Input_SlotId->OnTextChanged.AddUniqueDynamic(this, &ThisClass::HandleSlotIdChanged);
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
	if (Input_SlotId && Input_SlotId->GetText().IsEmpty()) Input_SlotId->SetText(FText::FromString(TEXT("Default")));
	RefreshSlotInfo();
	RefreshTravelControls();
}

void UKnightTitleMenuWidget::NativeDestruct()
{
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
	UpdateBackVisibility(NewPage == EKnightTitlePage::Home);
	RefreshTravelControls();
	if (!IsTravelPending() && GetFlow() && GetFlow()->GetCurrentFlowState() == EKnightFlowState::Title)
	{
		ClearStatus();
	}
	FocusCurrentPage();
	if (NewPage == EKnightTitlePage::SaveSelect) RefreshSlotInfo();
	return true;
}

bool UKnightTitleMenuWidget::HandleBackRequested()
{
	return CurrentPage != EKnightTitlePage::Home && ShowPage(EKnightTitlePage::Home);
}

UWidget* UKnightTitleMenuWidget::GetPreferredFocusTarget() const
{
	if (CurrentPage != EKnightTitlePage::Home)
	{
		if (CurrentPage == EKnightTitlePage::SaveSelect && Input_SlotId && Input_SlotId->GetIsEnabled()) return Input_SlotId;
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
	if (Button_UseSlot) Button_UseSlot->SetIsEnabled(bReady);
	if (Input_SlotId) Input_SlotId->SetIsEnabled(bReady);
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

void UKnightTitleMenuWidget::HandleSlotIdChanged(const FText& NewText)
{
	RefreshSlotInfo();
}

void UKnightTitleMenuWidget::RefreshSlotInfo()
{
	if (!Text_SlotInfo || !Input_SlotId) return;
	const FString SlotId = Input_SlotId->GetText().ToString().TrimStartAndEnd();
	const UknightSaveSubsystem* const Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	Text_SlotInfo->SetText(SlotId.IsEmpty() ? LOCTEXT("EnterSlot", "슬롯 이름을 입력하세요.")
		: FText::Format(Save && Save->DoesSlotExist(SlotId)
			? LOCTEXT("ExistingSlot", "{0} · 저장 있음") : LOCTEXT("NewSlot", "{0} · 신규 슬롯"), FText::FromString(SlotId)));
}

void UKnightTitleMenuWidget::HandleUseSlotClicked()
{
	UknightSaveSubsystem* const Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	if (!Save || IsTravelPending() || bAwaitingInitialSave || Save->IsSaveInFlight() || !Input_SlotId)
	{
		SetStatusText(LOCTEXT("ProfileBusy", "저장 작업이 끝날 때까지 기다려 주세요."));
		return;
	}
	const FString SlotId = Input_SlotId->GetText().ToString().TrimStartAndEnd();
	const bool bExisting = Save->DoesSlotExist(SlotId);
	SetStatusText(LOCTEXT("LoadingProfile", "프로필을 불러오는 중입니다."));
	if (!Save->SelectSlot(SlotId))
	{
		SetStatusText(Save->DidLoadFail()
			? LOCTEXT("ProfileUnreadable", "저장 파일을 읽을 수 없습니다. 원본은 보존됩니다. 다른 슬롯을 선택해 주세요.")
			: LOCTEXT("InvalidProfile", "슬롯 이름은 1~48자의 문자·숫자·_·-만 사용할 수 있습니다."));
		RefreshSlotInfo();
		return;
	}
	if (bExisting)
	{
		OpenLobby();
		return;
	}
	bAwaitingInitialSave = true;
	SetStatusText(LOCTEXT("CreatingProfile", "새 프로필을 저장하는 중입니다."));
	RefreshTravelControls();
	Save->RequestSave();
}

void UKnightTitleMenuWidget::HandleSaveFinished(bool bSuccess)
{
	const bool bWasCreating = bAwaitingInitialSave;
	bAwaitingInitialSave = false;
	RefreshTravelControls();
	RefreshSlotInfo();
	if (!bWasCreating) return;
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
