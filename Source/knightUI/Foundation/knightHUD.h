#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "knightHUD.generated.h"

class UCommonActivatableWidget;

/**
 * Gameplay HUD actor. Creates the primary layout through UknightUISubsystem and pushes HUDLayoutClass onto
 * UI.Layer.Game. Set it as the HUD Class of gameplay GameMode Blueprints; the menu maps keep their own UI.
 * Also routes the player controller's Esc to the pause menu (UknightUISettings::PauseMenuClass) on UI.Layer.Menu.
 */
UCLASS(Blueprintable)
class KNIGHTUI_API AknightHUD : public AHUD
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** HUD layout widget. Usually made of UI extension points (HUD.Slot.*) bound to the HUD view models. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|UI")
	TSoftClassPtr<UCommonActivatableWidget> HUDLayoutClass;

private:
	/** Esc from the player controller: opens the pause menu, or resumes when it is already open. */
	void TogglePauseMenu();

	TWeakObjectPtr<UCommonActivatableWidget> HUDLayout;
	TWeakObjectPtr<UCommonActivatableWidget> PauseMenu;
};
