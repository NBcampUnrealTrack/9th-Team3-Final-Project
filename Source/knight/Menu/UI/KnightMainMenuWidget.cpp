#include "KnightMainMenuWidget.h"

#include "GameFlow/KnightGameFlowSubsystem.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "knight.h"

void UKnightMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ValidateRequiredWidget(Panel_Home, TEXT("Panel_Home"));
	ValidateRequiredWidget(Panel_Stage, TEXT("Panel_Stage"));
	ValidateRequiredWidget(Panel_Maintenance, TEXT("Panel_Maintenance"));
	ValidateRequiredWidget(Panel_Settings, TEXT("Panel_Settings"));
	ValidateRequiredWidget(Panel_Codex, TEXT("Panel_Codex"));
	ValidateRequiredWidget(Button_Stage, TEXT("Button_Stage"));
	ValidateRequiredWidget(Button_Maintenance, TEXT("Button_Maintenance"));
	ValidateRequiredWidget(Button_Settings, TEXT("Button_Settings"));
	ValidateRequiredWidget(Button_Codex, TEXT("Button_Codex"));
	if (Button_Stage) Button_Stage->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStageClicked);
	if (Button_Maintenance) Button_Maintenance->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMaintenanceClicked);
	if (Button_Settings) Button_Settings->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSettingsClicked);
	if (Button_Codex) Button_Codex->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCodexClicked);
}

void UKnightMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyPage(EKnightMainMenuPage::Home, true);
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
	CurrentPage = NewPage;
	UpdateBackVisibility(NewPage == EKnightMainMenuPage::Home);
	RefreshTravelControls();
	if (!IsTravelPending() && GetFlow() && GetFlow()->GetCurrentFlowState() == EKnightFlowState::MainMenu)
	{
		ClearStatus();
	}
	FocusCurrentPage();
	return true;
}

bool UKnightMainMenuWidget::HandleBackRequested()
{
	return CurrentPage != EKnightMainMenuPage::Home && ShowPage(EKnightMainMenuPage::Home);
}

UWidget* UKnightMainMenuWidget::GetPreferredFocusTarget() const
{
	if (CurrentPage != EKnightMainMenuPage::Home)
	{
		return Button_Back;
	}
	for (UButton* Button : { Button_Stage.Get(), Button_Maintenance.Get(), Button_Settings.Get(), Button_Codex.Get() })
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
	if (Button_Maintenance) Button_Maintenance->SetIsEnabled(bEnabled);
	if (Button_Settings) Button_Settings->SetIsEnabled(bEnabled);
	if (Button_Codex) Button_Codex->SetIsEnabled(bEnabled);
}

void UKnightMainMenuWidget::HandleStageClicked()
{
	ShowPage(EKnightMainMenuPage::Stage);
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
