#include "KnightMenuPlayerController.h"

#include "KnightMenuGameMode.h"
#include "UI/KnightMainMenuWidget.h"
#include "UI/KnightMenuWidgetBase.h"
#include "UI/KnightTitleMenuWidget.h"
#include "GameFlow/KnightGameFlowSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
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
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
	MenuWidget->FocusCurrentPage();
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
