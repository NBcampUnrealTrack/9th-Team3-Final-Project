#include "KnightFlowTestGameMode.h"

#include "KnightFlowTestPlayerController.h"
#include "GameFlow/KnightGameFlowSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

AKnightFlowTestGameMode::AKnightFlowTestGameMode()
{
	PlayerControllerClass = AKnightFlowTestPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}

void AKnightFlowTestGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	UWorld* const World = GetWorld();
	UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
	if (UKnightGameFlowSubsystem* const Flow = GameInstance ? GameInstance->GetSubsystem<UKnightGameFlowSubsystem>() : nullptr)
	{
		Flow->RegisterEnteredGameplay(World);
	}
}
