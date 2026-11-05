#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "knightUISettings.generated.h"

class USoundBase;
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

	/** Raid start: the Knight is spawned and the loading screen is gone. See UknightPilotVoice. */
	UPROPERTY(EditAnywhere, Config, Category="Pilot Voice")
	TSoftObjectPtr<USoundBase> SystemActivatedVoice;

	/** The raid enters combat for the first time. */
	UPROPERTY(EditAnywhere, Config, Category="Pilot Voice")
	TSoftObjectPtr<USoundBase> EnemyDetectedVoice;

	/** The Knight draws the sword (CombatStateTag). */
	UPROPERTY(EditAnywhere, Config, Category="Pilot Voice")
	TSoftObjectPtr<USoundBase> CombatModeVoice;

	/** Health first drops to HealthWarningRatio or below. */
	UPROPERTY(EditAnywhere, Config, Category="Pilot Voice")
	TSoftObjectPtr<USoundBase> HealthWarningVoice;

	/** 마지막 대상 몬스터를 처치한 뒤 토벌 확인 배너가 뜰 때("목표 생명 반응 소실 확인. 토벌 성공입니다."). */
	UPROPERTY(EditAnywhere, Config, Category="Pilot Voice")
	TSoftObjectPtr<USoundBase> RaidSuccessVoice;

	/** Tag the combat stance ability adds while the sword is drawn. */
	UPROPERTY(EditAnywhere, Config, Category="Pilot Voice", meta=(Categories="State"))
	FGameplayTag CombatStateTag;

	/** The sword is drawn often; CombatModeVoice plays at most once in this time. */
	UPROPERTY(EditAnywhere, Config, Category="Pilot Voice", meta=(ClampMin=0, Units="s"))
	float CombatModeVoiceCooldown = 20.f;

	UPROPERTY(EditAnywhere, Config, Category="Pilot Voice", meta=(ClampMin=0, ClampMax=1))
	float HealthWarningRatio = 0.3f;
};
