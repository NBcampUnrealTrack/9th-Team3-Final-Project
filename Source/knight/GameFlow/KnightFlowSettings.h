#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "KnightFlowSettings.generated.h"

class UWorld;
class UKnightStageCatalog;

/** Menu destinations configured in Project Settings after the maps exist. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Knight Flow Maps"))
class KNIGHT_API UKnightFlowSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Config, Category="Maps")
	TSoftObjectPtr<UWorld> TitleMap;

	UPROPERTY(EditAnywhere, Config, Category="Maps")
	TSoftObjectPtr<UWorld> MainMenuMap;

	/** One catalog shared by the menu UI and GameFlow travel validation. */
	UPROPERTY(EditAnywhere, Config, Category="Stages")
	TSoftObjectPtr<UKnightStageCatalog> StageCatalog;
};
