#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "KnightMenuPlayerController.generated.h"

class UKnightMenuWidgetBase;

/** Owns one local root widget for the active title or main menu level. */
UCLASS()
class KNIGHT_API AKnightMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AKnightMenuPlayerController();

	/** Editor validation commands; the retail menu uses the widget buttons. */
	UFUNCTION(Exec) void KnightCoreLaunchTwice(FName StageId);
	UFUNCTION(Exec) void KnightCoreLaunch(FName StageId);
	UFUNCTION(Exec) void KnightCoreFailNextRaidLoad();
	UFUNCTION(Exec) void KnightCoreProfileStatus();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UKnightMenuWidgetBase> MenuWidget;
};
