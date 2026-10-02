// Copyright Epic Games, Inc. All Rights Reserved.


#include "knightPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "knight.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "AbilitySystem/knightAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Pawn.h"
#include "Input/knightInputBufferComponent.h"
#include "Camera/knightPlayerCameraManager.h"

AknightPlayerController::AknightPlayerController()
{
	PlayerCameraManagerClass = AknightPlayerCameraManager::StaticClass();
}

void AknightPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = false;
	}

	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(Logknight, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void AknightPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

bool AknightPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void AknightPlayerController::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	APawn* const ControlledPawn = GetPawn();
	if (UknightAbilitySystemComponent* const ASC = Cast<UknightAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ControlledPawn)))
	{
		ASC->ProcessAbilityInput(DeltaTime, bGamePaused);
		if (UknightInputBufferComponent* const InputBuffer = ControlledPawn->FindComponentByClass<UknightInputBufferComponent>())
		{
			InputBuffer->ProcessBuffer(ASC);
		}
	}

	Super::PostProcessInput(DeltaTime, bGamePaused);
}
