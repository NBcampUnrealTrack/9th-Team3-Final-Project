// Copyright Epic Games, Inc. All Rights Reserved.

#include "knightGameMode.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFlow/KnightGameFlowSubsystem.h"
#include "TimerManager.h"
#include "knight.h"

AknightGameMode::AknightGameMode()
{
	// stub
}

void AknightGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	UWorld* const World = GetWorld();
	UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
	if (UKnightGameFlowSubsystem* const Flow = GameInstance ? GameInstance->GetSubsystem<UKnightGameFlowSubsystem>() : nullptr)
	{
		Flow->RegisterEnteredGameplay(World);
	}
}

void AknightGameMode::HandlePlayerDeath(AController* PlayerController)
{
	if (GetWorldTimerManager().IsTimerActive(ReturnToMainMenuTimer))
	{
		return;
	}
	if (ReturnToMainMenuDelay <= 0.f)
	{
		ReturnToMainMenu();
		return;
	}
	GetWorldTimerManager().SetTimer(ReturnToMainMenuTimer, this, &ThisClass::ReturnToMainMenu, ReturnToMainMenuDelay, false);
}

void AknightGameMode::ReturnToMainMenu()
{
	UGameInstance* const GameInstance = GetGameInstance();
	UKnightGameFlowSubsystem* const Flow = GameInstance ? GameInstance->GetSubsystem<UKnightGameFlowSubsystem>() : nullptr;
	if (!Flow || !Flow->RequestOpenMainMenu())
	{
		UE_LOG(Logknight, Warning, TEXT("%s could not return to the main menu after the player's death."), *GetNameSafe(this));
	}
}
