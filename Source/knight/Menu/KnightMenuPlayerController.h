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
	/** The root currently displayed by this controller. */
	UFUNCTION(BlueprintPure, Category="Knight|Menu")
	UKnightMenuWidgetBase* GetMenuWidget() const { return MenuWidget; }

	/** Editor validation commands; the retail menu uses the widget buttons. */
	UFUNCTION(Exec) void KnightCoreLaunchTwice(FName StageId);
	UFUNCTION(Exec) void KnightCoreLaunch(FName StageId);
	UFUNCTION(Exec) void KnightCoreLaunchQuestTwice(FName QuestId);
	UFUNCTION(Exec) void KnightCoreLaunchQuest(FName QuestId);
	UFUNCTION(Exec) void KnightCoreQuestStatus();
	UFUNCTION(Exec) void KnightCoreFailNextRaidLoad();
	UFUNCTION(Exec) void KnightCoreProfileStatus();
	UFUNCTION(Exec) void KnightCoreUseFixedSlot(int32 SlotNumber);
	UFUNCTION(Exec) void KnightCoreReturnTitle();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ApplyMenuInputMode();

	UPROPERTY(Transient)
	TObjectPtr<UKnightMenuWidgetBase> MenuWidget;
};
