#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "KnightFlowTestPlayerController.generated.h"

class UKnightFlowTestReturnWidget;

/** Owns the optional return-test UI for the local player only. */
UCLASS()
class KNIGHT_API AKnightFlowTestPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AKnightFlowTestPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UKnightFlowTestReturnWidget> ReturnWidget;
};
