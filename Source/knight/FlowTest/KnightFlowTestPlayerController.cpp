#include "KnightFlowTestPlayerController.h"

#include "KnightFlowTestGameMode.h"
#include "KnightFlowTestReturnWidget.h"
#include "GameFlow/KnightGameFlowSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "knight.h"

AKnightFlowTestPlayerController::AKnightFlowTestPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
}

void AKnightFlowTestPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController() || ReturnWidget)
	{
		return;
	}
	const AKnightFlowTestGameMode* const TestMode = GetWorld() ? Cast<AKnightFlowTestGameMode>(GetWorld()->GetAuthGameMode()) : nullptr;
	UKnightGameFlowSubsystem* const Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>() : nullptr;
	if (!TestMode || !Flow || Flow->GetCurrentFlowState() != EKnightFlowState::Gameplay)
	{
		UE_LOG(Logknight, Error, TEXT("Flow test UI requires a registered Gameplay world and KnightFlowTestGameMode."));
		return;
	}
	UClass* const WidgetClass = TestMode->GetTestWidgetClass();
	if (!WidgetClass)
	{
		UE_LOG(Logknight, Warning, TEXT("%s has no TestWidgetClass yet; no return UI was created."), *TestMode->GetClass()->GetName());
		return;
	}
	ReturnWidget = CreateWidget<UKnightFlowTestReturnWidget>(this, WidgetClass);
	if (!ReturnWidget)
	{
		UE_LOG(Logknight, Error, TEXT("Failed to create flow test return widget %s."), *WidgetClass->GetName());
		return;
	}
	ReturnWidget->AddToViewport();
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(ReturnWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
	ReturnWidget->FocusReturnButton();
}

void AKnightFlowTestPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ReturnWidget)
	{
		ReturnWidget->RemoveFromParent();
		ReturnWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}
