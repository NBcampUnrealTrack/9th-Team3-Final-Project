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
	/** Isolated editor-only settlement probes; refuse to run without -KnightSaveTestNamespace. */
	UFUNCTION(Exec) void KnightCoreRewardTestSlot(int32 SlotNumber);
	UFUNCTION(Exec) void KnightCoreRewardFailNextSaves(int32 Count);
	UFUNCTION(Exec) void KnightCoreRewardResult(FName QuestId, int32 bSuccess, int32 bHead, int32 bTail);
	UFUNCTION(Exec) void KnightCoreRewardStatus();
	/** Editor-only sale probes require -KnightSaveTestNamespace and never touch user slots. */
	UFUNCTION(Exec) void KnightCoreSaleGrant(FName ItemName, int32 Count);
	UFUNCTION(Exec) void KnightCoreSaleAttempt(FName ItemName, int32 Count);
	UFUNCTION(Exec) void KnightCoreSaleRetrySave();
	/** Isolated editor-only weapon-upgrade probes. */
	UFUNCTION(Exec) void KnightCoreUpgradeGrant(FName ItemName, int32 Count);
	UFUNCTION(Exec) void KnightCoreUpgradeMoney(int32 Amount);
	UFUNCTION(Exec) void KnightCoreUpgradeForceNext(int32 bSuccess);
	UFUNCTION(Exec) void KnightCoreUpgradeAttempt(FName WeaponName);
	UFUNCTION(Exec) void KnightCoreUpgradeStatus(FName WeaponName);
	UFUNCTION(Exec) void KnightCoreUpgradeEquip(FName WeaponName);
	UFUNCTION(Exec) void KnightCoreUpgradeCompare(FName WeaponName);
	/** Isolated editor-only socket probes; item grant reuses KnightCoreUpgradeGrant. */
	UFUNCTION(Exec) void KnightCoreSocketUnlock(int32 Count);
	UFUNCTION(Exec) void KnightCoreSocketEquip(int32 SlotIndex, FName ItemName);
	UFUNCTION(Exec) void KnightCoreSocketUnequip(int32 SlotIndex);
	UFUNCTION(Exec) void KnightCoreSocketConsume(FName ItemName, int32 Count);
	UFUNCTION(Exec) void KnightCoreSocketStatus();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ApplyMenuInputMode();

	UPROPERTY(Transient)
	TObjectPtr<UKnightMenuWidgetBase> MenuWidget;
};
