// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "knightPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UKnightFlowTestReturnWidget;
class UKnightSettingsWidget;

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS(abstract)
class KNIGHT_API AknightPlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	/** Uses AknightPlayerCameraManager for pitch limits and combat shakes. */
	AknightPlayerController();

	/** Closes the raid return overlay and restores gameplay input. */
	void CloseRaidReturnMenu();

	/** Clear the transient boss-hit camera/slow beat before opening paused UI or changing worlds. */
	void CancelKnockdownPresentation();

	/**
	 * Esc handler set by the UI (knightUI's HUD binds its pause menu here). This module cannot see the UI classes,
	 * so the UI plugs in. When unbound, Esc opens the settings window directly as before.
	 */
	FSimpleDelegate PauseMenuRequestHandler;

	/** Opens the settings window over a menu. Closing it leaves input and pause to that menu. */
	void OpenSettingsFromMenu();

	/** Fires after the settings window closes, however it was opened. */
	FSimpleMulticastDelegate OnSettingsClosed;

	/** Editor-only raid API probes. These never run as part of normal combat. */
	UFUNCTION(Exec) void KnightRaidDebugStatus();
	UFUNCTION(Exec) void KnightRaidDebugStart();
	UFUNCTION(Exec) void KnightRaidDebugSuccess();
	UFUNCTION(Exec) void KnightRaidDebugFailure();
	UFUNCTION(Exec) void KnightRaidDebugReturn();
	UFUNCTION(Exec) void KnightRaidDebugSettings();

protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** Set only on the raid PlayerController Blueprint to enable the Escape return overlay. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Raid")
	TSubclassOf<UKnightFlowTestReturnWidget> RaidReturnWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UKnightFlowTestReturnWidget> RaidReturnWidget;

	UPROPERTY(Transient)
	TObjectPtr<UKnightSettingsWidget> SettingsWidget;

	bool bSettingsPausedGame = false;
	bool bCursorBeforeSettings = false;
	bool bSettingsOpenedFromMenu = false;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Runs ability input and the input buffer once per frame, after all input for the frame is processed. */
	virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused) override;

	/**
	 * 소리 듣는 위치 (2026-10-12). 기본은 카메라 위치라서, 큰 몬스터를 록온해 카메라가 멀리(최대 14m) 빠지면
	 * 나이트의 칼 휘두르는 소리 같은 가까운 소리가 감쇠 범위 밖으로 나간다. 그래서 위치는 폰 쪽으로 당기고
	 * 방향(좌우 패닝)은 카메라를 그대로 따른다.
	 */
	virtual void GetAudioListenerPosition(FVector& OutLocation, FVector& OutFrontDir, FVector& OutRightDir) const override;

	/** 0 = 카메라 위치, 1 = 폰 위치. */
	UPROPERTY(EditDefaultsOnly, Category="Audio", meta=(ClampMin=0, ClampMax=1))
	float AudioListenerPawnWeight = 1.f;

	/** 폰 위치에서 위로 올리는 높이(머리 높이 근처). */
	UPROPERTY(EditDefaultsOnly, Category="Audio", meta=(Units="cm"))
	float AudioListenerHeight = 60.f;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;
	void ToggleRaidReturnMenu();
	void ToggleSettings();
	void OpenSettings(bool bFromMenu);
	UFUNCTION() void CloseSettings();

};
