// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "knightPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UKnightFlowTestReturnWidget;

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

	/** Editor-only raid API probes. These never run as part of normal combat. */
	UFUNCTION(Exec) void KnightRaidDebugStatus();
	UFUNCTION(Exec) void KnightRaidDebugStart();
	UFUNCTION(Exec) void KnightRaidDebugSuccess();
	UFUNCTION(Exec) void KnightRaidDebugFailure();
	UFUNCTION(Exec) void KnightRaidDebugReturn();

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

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;
	void ToggleRaidReturnMenu();

};
