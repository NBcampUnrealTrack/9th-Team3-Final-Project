#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFlow/KnightFlowTypes.h"
#include "KnightMenuGameMode.generated.h"

/** Base for the future title and main menu GameMode Blueprints. */
UCLASS()
class KNIGHT_API AKnightMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AKnightMenuGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

protected:
	/** Each menu GameMode Blueprint must choose Title or MainMenu. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Knight|Menu")
	EKnightFlowState MenuFlowState = EKnightFlowState::Uninitialized;
};
