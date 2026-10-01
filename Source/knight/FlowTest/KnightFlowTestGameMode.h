#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "KnightFlowTestGameMode.generated.h"

class UKnightFlowTestReturnWidget;

/** Optional empty-map GameMode for testing stage arrival and return travel. */
UCLASS()
class KNIGHT_API AKnightFlowTestGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AKnightFlowTestGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	TSubclassOf<UKnightFlowTestReturnWidget> GetTestWidgetClass() const { return TestWidgetClass; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Knight|Flow Test")
	TSubclassOf<UKnightFlowTestReturnWidget> TestWidgetClass;
};
