// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "knightGameMode.generated.h"

/**
 * Base GameMode for gameplay (raid) maps.
 *
 * Registers the level with UKnightGameFlowSubsystem so menu travel works from here, and returns
 * to the main menu after the player's Knight is destroyed. Raid phases and results come later.
 */
UCLASS(abstract)
class KNIGHT_API AknightGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	/** Constructor */
	AknightGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	/** Called by the Knight when it is destroyed. Starts the return to the main menu. */
	virtual void HandlePlayerDeath(AController* PlayerController);

protected:
	/** Seconds between the Knight's destruction and travelling back to the main menu. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Flow", meta=(ClampMin=0, Units="s"))
	float ReturnToMainMenuDelay = 5.f;

	/** Requests travel to the main menu through UKnightGameFlowSubsystem. */
	virtual void ReturnToMainMenu();

private:
	FTimerHandle ReturnToMainMenuTimer;
};
