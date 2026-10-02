// Copyright Epic Games, Inc. All Rights Reserved.


#include "knightPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "Components/InputComponent.h"
#include "Blueprint/UserWidget.h"
#include "FlowTest/KnightFlowTestReturnWidget.h"
#include "GameFlow/KnightGameFlowSubsystem.h"
#include "Progression/knightSaveGame.h"
#include "Progression/knightSaveSubsystem.h"
#include "Raid/knightRaidGameMode.h"
#include "Raid/knightRaidGameState.h"
#include "Raid/knightRaidDefinition.h"
#include "Raid/knightRaidSubsystem.h"
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

void AknightPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (RaidReturnWidget)
	{
		RaidReturnWidget->RemoveFromParent();
		RaidReturnWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AknightPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (RaidReturnWidgetClass && InputComponent)
	{
		FInputKeyBinding& ReturnBinding = InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ThisClass::ToggleRaidReturnMenu);
		ReturnBinding.bExecuteWhenPaused = true;
#if WITH_EDITOR
		// The editor reserves Escape to stop in-viewport PIE; F6 exercises the same menu there.
		FInputKeyBinding& PieReturnBinding = InputComponent->BindKey(EKeys::F6, IE_Pressed, this, &ThisClass::ToggleRaidReturnMenu);
		PieReturnBinding.bExecuteWhenPaused = true;
#endif
	}

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

void AknightPlayerController::ToggleRaidReturnMenu()
{
	if (!IsLocalController() || !RaidReturnWidgetClass || !GetWorld()
		|| !GetWorld()->GetAuthGameMode<AknightRaidGameMode>()) return;
	if (RaidReturnWidget && RaidReturnWidget->IsInViewport())
	{
		CloseRaidReturnMenu();
		return;
	}
	if (!RaidReturnWidget)
	{
		RaidReturnWidget = CreateWidget<UKnightFlowTestReturnWidget>(this, RaidReturnWidgetClass);
	}
	if (!RaidReturnWidget) return;
	RaidReturnWidget->AddToViewport();
	SetPause(true);
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(RaidReturnWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
	RaidReturnWidget->FocusReturnButton();
	UE_LOG(Logknight, Log, TEXT("Raid return overlay opened; paused=%d cursor=%d"), GetWorld()->IsPaused(), bShowMouseCursor);
}

void AknightPlayerController::CloseRaidReturnMenu()
{
	if (RaidReturnWidget && RaidReturnWidget->IsInViewport()) RaidReturnWidget->RemoveFromParent();
	SetPause(false);
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	bShowMouseCursor = false;
	UE_LOG(Logknight, Log, TEXT("Raid return overlay closed; paused=%d cursor=%d"), GetWorld() && GetWorld()->IsPaused(), bShowMouseCursor);
}

void AknightPlayerController::KnightRaidDebugStatus()
{
#if WITH_EDITOR
	const AknightRaidGameState* const State = GetWorld() ? GetWorld()->GetGameState<AknightRaidGameState>() : nullptr;
	const UknightRaidSubsystem* const Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightRaidSubsystem>() : nullptr;
	const UKnightGameFlowSubsystem* const Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>() : nullptr;
	const AknightRaidGameMode* const Raid = GetWorld() ? GetWorld()->GetAuthGameMode<AknightRaidGameMode>() : nullptr;
	const UknightSaveSubsystem* const Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	const UknightSaveGame* const Save = Saves ? Saves->GetSaveGame() : nullptr;
	UE_LOG(Logknight, Log, TEXT("KnightRaidDebugStatus map=%s stage=%s definition=%s phase=%d run=%s slot=%s time=%.3f pawn=%s paused=%d cursor=%d"),
		GetWorld() ? *GetWorld()->GetMapName() : TEXT("None"),
		Flow ? *Flow->GetActiveStageId().ToString() : TEXT("None"), Raid ? *GetNameSafe(Raid->GetRaidDefinition()) : TEXT("None"),
		State ? static_cast<int32>(State->GetRaidPhase()) : -1,
		Run ? *Run->GetCurrentRunId().ToString() : TEXT("None"), Saves ? *Saves->GetActiveSlotId() : TEXT("None"),
		Save ? Save->TotalPlayTime : -1.f, *GetNameSafe(GetPawn()), GetWorld() && GetWorld()->IsPaused(), bShowMouseCursor);
#endif
}

void AknightPlayerController::KnightRaidDebugStart()
{
#if WITH_EDITOR
	if (AknightRaidGameMode* const Raid = GetWorld() ? GetWorld()->GetAuthGameMode<AknightRaidGameMode>() : nullptr)
	{
		UE_LOG(Logknight, Log, TEXT("KnightRaidDebugStart accepted=%d"), Raid->RequestStartRaid());
	}
#endif
}

void AknightPlayerController::KnightRaidDebugSuccess()
{
#if WITH_EDITOR
	if (AknightRaidGameMode* const Raid = GetWorld() ? GetWorld()->GetAuthGameMode<AknightRaidGameMode>() : nullptr)
	{
		Raid->NotifyObjectiveCompleted();
	}
#endif
}

void AknightPlayerController::KnightRaidDebugFailure()
{
#if WITH_EDITOR
	if (AknightRaidGameMode* const Raid = GetWorld() ? GetWorld()->GetAuthGameMode<AknightRaidGameMode>() : nullptr)
	{
		Raid->NotifyPlayerDefeated();
	}
#endif
}

void AknightPlayerController::KnightRaidDebugReturn()
{
#if WITH_EDITOR
	if (AknightRaidGameMode* const Raid = GetWorld() ? GetWorld()->GetAuthGameMode<AknightRaidGameMode>() : nullptr)
	{
		CloseRaidReturnMenu();
		Raid->RequestReturnToLobby();
	}
#endif
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
