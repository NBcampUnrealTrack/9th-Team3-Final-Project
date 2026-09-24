#include "KnightTitleMenuWidget.h"

#include "GameFlow/KnightGameFlowSubsystem.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Kismet/KismetSystemLibrary.h"
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
	ValidateRequiredWidget(Button_Settings, TEXT("Button_Settings"));
	ValidateRequiredWidget(Button_Quit, TEXT("Button_Quit"));
	if (Button_StartGame) Button_StartGame->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStartGameClicked);
	if (Button_SaveSelect) Button_SaveSelect->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSaveSelectClicked);
	if (Button_Settings) Button_Settings->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSettingsClicked);
	if (Button_Quit) Button_Quit->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleQuitClicked);
}

void UKnightTitleMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyPage(EKnightTitlePage::Home, true);
}

bool UKnightTitleMenuWidget::ShowPage(EKnightTitlePage NewPage)
{
	return ApplyPage(NewPage, false);
}

bool UKnightTitleMenuWidget::ApplyPage(EKnightTitlePage NewPage, bool bAllowDuringTravel)
{
	if (!bAllowDuringTravel && IsTravelPending())
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
	if (Button_StartGame) Button_StartGame->SetIsEnabled(bEnabled && GetFlow() != nullptr);
	if (Button_SaveSelect) Button_SaveSelect->SetIsEnabled(bEnabled);
	if (Button_Settings) Button_Settings->SetIsEnabled(bEnabled);
	if (Button_Quit) Button_Quit->SetIsEnabled(bEnabled);
}

void UKnightTitleMenuWidget::HandleStartGameClicked()
{
	if (IsTravelPending())
	{
		return;
	}
	UKnightGameFlowSubsystem* const Flow = GetFlow();
	if (!Flow)
	{
		UE_LOG(Logknight, Error, TEXT("Title StartGame has no GameFlow subsystem."));
		SetStatusText(LOCTEXT("NoFlow", "지금 메뉴를 열 수 없습니다."));
		return;
	}
	const bool bAccepted = Flow->RequestOpenMainMenu();
	RefreshTravelControls();
	if (bAccepted && Flow->IsTravelPending())
	{
		SetStatusText(LOCTEXT("OpeningMainMenu", "메인메뉴로 이동 중입니다."));
	}
}

void UKnightTitleMenuWidget::HandleSaveSelectClicked()
{
	ShowPage(EKnightTitlePage::SaveSelect);
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
