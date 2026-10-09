#include "KnightMenuGameMode.h"

#include "GameFlow/KnightGameFlowSubsystem.h"
#include "KnightMenuPlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "knightAudioSubsystem.h"
#include "Sound/SoundBase.h"
#include "System/knightAssetManager.h"
#include "knight.h"

AKnightMenuGameMode::AKnightMenuGameMode()
{
	PlayerControllerClass = AKnightMenuPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}

void AKnightMenuGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if (MenuFlowState != EKnightFlowState::Title && MenuFlowState != EKnightFlowState::MainMenu)
	{
		UE_LOG(Logknight, Error, TEXT("%s has invalid MenuFlowState. Choose Title or MainMenu on its GameMode Blueprint."), *GetClass()->GetName());
		return;
	}

	UWorld* const World = GetWorld();
	UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
	if (UKnightGameFlowSubsystem* const Flow = GameInstance ? GameInstance->GetSubsystem<UKnightGameFlowSubsystem>() : nullptr)
	{
		Flow->RegisterEnteredMenu(MenuFlowState, World);
	}
}

void AKnightMenuGameMode::StartPlay()
{
	Super::StartPlay();
	UknightAudioSubsystem* const Audio = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightAudioSubsystem>() : nullptr;
	if (Audio && !MenuMusic.IsNull())
	{
		Audio->PlayMusic(UknightAssetManager::GetAsset(MenuMusic));
	}
}
