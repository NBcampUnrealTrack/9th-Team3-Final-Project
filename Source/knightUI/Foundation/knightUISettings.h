#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "knightUISettings.generated.h"

class UknightConfirmModal;
class UknightPauseMenu;
class UknightPrimaryGameLayout;
class UknightRaidResultScreen;

/** Project Settings > Game > Knight UI. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Knight UI"))
class KNIGHTUI_API UknightUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Root layout Blueprint created for the local player on gameplay maps. */
	UPROPERTY(EditAnywhere, Config, Category="Layout")
	TSoftClassPtr<UknightPrimaryGameLayout> PrimaryGameLayoutClass;

	/** Shared confirmation dialog shown by UknightConfirmModal::Show. */
	UPROPERTY(EditAnywhere, Config, Category="Layout")
	TSoftClassPtr<UknightConfirmModal> ConfirmModalClass;

	/** Pause menu that AknightHUD opens on Esc. */
	UPROPERTY(EditAnywhere, Config, Category="Layout")
	TSoftClassPtr<UknightPauseMenu> PauseMenuClass;

	/** Raid result that AknightHUD opens when the raid succeeds or fails. */
	UPROPERTY(EditAnywhere, Config, Category="Layout")
	TSoftClassPtr<UknightRaidResultScreen> RaidResultScreenClass;

	/** Seconds between refreshes of values that change without events (raid timer, late-bound boss). */
	UPROPERTY(EditAnywhere, Config, Category="ViewModels", meta=(ClampMin=0.05, Units="s"))
	float ViewModelRefreshInterval = 0.25f;
};
