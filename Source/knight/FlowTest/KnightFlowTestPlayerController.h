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

	/** Development console hooks for the isolated core test map. */
	UFUNCTION(Exec) void KnightCoreSelectTestSlot(const FString& SlotId);
	UFUNCTION(Exec) void KnightCoreSuccess();
	UFUNCTION(Exec) void KnightCoreFailure();
	UFUNCTION(Exec) void KnightCoreStatus();
	UFUNCTION(Exec) void KnightCoreProbeInterfaces();
	UFUNCTION(Exec) void KnightCoreSaveQueueTest(const FString& AttemptSwitchSlot);
	UFUNCTION(Exec) void KnightCoreSaveOnce();
	UFUNCTION(Exec) void KnightCoreFailNextSaves(int32 Count);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UKnightFlowTestReturnWidget> ReturnWidget;
};
