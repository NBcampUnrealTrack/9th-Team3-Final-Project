#pragma once

#include "CoreMinimal.h"
#include "FieldNotificationId.h"
#include "GameFramework/HUD.h"
#include "knightHUD.generated.h"

class UCommonActivatableWidget;
class UknightVM_Raid;

/**
 * Gameplay HUD actor. Creates the primary layout through UknightUISubsystem and pushes HUDLayoutClass onto
 * UI.Layer.Game. Set it as the HUD Class of gameplay GameMode Blueprints; the menu maps keep their own UI.
 * Also routes the player controller's Esc to the pause menu (UknightUISettings::PauseMenuClass) on UI.Layer.Menu,
 * and opens the raid result (UknightUISettings::RaidResultScreenClass) there when the raid succeeds or fails.
 */
UCLASS(Blueprintable)
class KNIGHTUI_API AknightHUD : public AHUD
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** HUD layout widget. Its HUD widgets read the view models from UknightUISubsystem. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|UI")
	TSoftClassPtr<UCommonActivatableWidget> HUDLayoutClass;

private:
	/** Esc from the player controller: opens the pause menu, or resumes when it is already open. */
	void TogglePauseMenu();

	void HandleRaidViewModelChanged(UObject* InViewModel, UE::FieldNotification::FFieldId FieldId);
	void OpenRaidResult();

	TWeakObjectPtr<UCommonActivatableWidget> HUDLayout;
	TWeakObjectPtr<UCommonActivatableWidget> PauseMenu;
	TWeakObjectPtr<UCommonActivatableWidget> RaidResult;
	TWeakObjectPtr<UknightVM_Raid> RaidViewModel;
};
