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
#include "Settings/KnightSettingsWidget.h"
#include "UserSettings/EnhancedInputUserSettings.h"

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
	if (SettingsWidget)
	{
		SettingsWidget->OnClosed.RemoveDynamic(this, &ThisClass::CloseSettings);
		SettingsWidget->RemoveFromParent();
		SettingsWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AknightPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent)
	{
		FInputKeyBinding& SettingsBinding = InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ThisClass::ToggleSettings);
		SettingsBinding.bExecuteWhenPaused = true;
	}
	if (RaidReturnWidgetClass && InputComponent)
	{
#if WITH_EDITOR
		// Retain the existing raid return test overlay on its editor shortcut.
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
				if (UEnhancedInputUserSettings* const UserSettings = Subsystem->GetUserSettings())
					UserSettings->RegisterInputMappingContext(CurrentContext);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
					if (UEnhancedInputUserSettings* const UserSettings = Subsystem->GetUserSettings())
						UserSettings->RegisterInputMappingContext(CurrentContext);
				}
			}
		}
	}
}

void AknightPlayerController::ToggleSettings()
{
	if (!IsLocalController() || !GetWorld()) return;
	if (SettingsWidget && SettingsWidget->IsInViewport())
	{
		SettingsWidget->RequestClose();
		return;
	}
	if (RaidReturnWidget && RaidReturnWidget->IsInViewport()) return;
	if (PauseMenuRequestHandler.IsBound())
	{
		PauseMenuRequestHandler.Execute();
		return;
	}
	if (!GetWorld()->GetAuthGameMode<AknightRaidGameMode>()) return;
	OpenSettings(false);
}

void AknightPlayerController::OpenSettingsFromMenu()
{
	if (!IsLocalController() || !GetWorld() || (SettingsWidget && SettingsWidget->IsInViewport())) return;
	OpenSettings(true);
}

void AknightPlayerController::OpenSettings(bool bFromMenu)
{
	if (!SettingsWidget)
	{
		SettingsWidget = CreateWidget<UKnightSettingsWidget>(this, UKnightSettingsWidget::GetSharedWidgetClass());
		if (SettingsWidget) SettingsWidget->OnClosed.AddUniqueDynamic(this, &ThisClass::CloseSettings);
	}
	if (!SettingsWidget) return;
	bSettingsOpenedFromMenu = bFromMenu;
	bCursorBeforeSettings = bShowMouseCursor;
	bSettingsPausedGame = !GetWorld()->IsPaused();
	SettingsWidget->AddToViewport(100);
	SettingsWidget->SetPositionInViewport(FVector2D::ZeroVector, false);
	SettingsWidget->SetDesiredSizeInViewport(FVector2D(800.f, 620.f));
	SettingsWidget->SetAnchorsInViewport(FAnchors(0.5f, 0.5f));
	SettingsWidget->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
	SettingsWidget->Open();
	if (bSettingsPausedGame && !SetPause(true)) bSettingsPausedGame = false;
	if (bFromMenu)
	{
		// UIOnly would make the viewport ignore input, and only GameOnly undoes that. The menu underneath hands
		// back to gameplay through CommonUI, which never touches that flag, so the player would be left frozen.
		// The game is paused under the menu, so letting input through here is harmless.
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(SettingsWidget->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
	}
	else
	{
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(SettingsWidget->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}
	bShowMouseCursor = true;
	SettingsWidget->FocusSettings();
	UE_LOG(Logknight, Log, TEXT("Raid settings opened; paused=%d cursor=%d"), GetWorld()->IsPaused(), bShowMouseCursor);
}

void AknightPlayerController::CloseSettings()
{
	if (SettingsWidget && SettingsWidget->IsInViewport()) SettingsWidget->RemoveFromParent();
	// Opened over a menu (pause): that menu still owns input and focus, so gameplay input is not restored here.
	if (!bSettingsOpenedFromMenu)
	{
		FInputModeGameOnly Mode;
		SetInputMode(Mode);
		bShowMouseCursor = bCursorBeforeSettings;
	}
	if (bSettingsPausedGame && GetWorld() && GetWorld()->IsPaused()) SetPause(false);
	bSettingsPausedGame = false;
	bSettingsOpenedFromMenu = false;
	OnSettingsClosed.Broadcast();
	UE_LOG(Logknight, Log, TEXT("Raid settings closed; paused=%d cursor=%d"), GetWorld() && GetWorld()->IsPaused(), bShowMouseCursor);
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

void AknightPlayerController::KnightRaidDebugSettings()
{
#if WITH_EDITOR
	// Opens the settings window itself, even when Esc is routed to the pause menu.
	if (SettingsWidget && SettingsWidget->IsInViewport()) SettingsWidget->RequestClose();
	else if (IsLocalController() && GetWorld() && GetWorld()->GetAuthGameMode<AknightRaidGameMode>()) OpenSettings(false);
#endif
}

bool AknightPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void AknightPlayerController::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	if (SettingsWidget && SettingsWidget->IsInViewport())
	{
		Super::PostProcessInput(DeltaTime, bGamePaused);
		return;
	}
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
