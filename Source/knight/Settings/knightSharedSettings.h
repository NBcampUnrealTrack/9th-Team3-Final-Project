#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "knightSharedSettings.generated.h"

/**
 * Personal settings that follow the player across machines (saved as a local player save game, so Steam
 * Cloud carries it): camera sensitivity and inversion, camera shake, subtitles.
 * Key bindings are not stored here: use Enhanced Input user settings (Project Settings > Enhanced Input >
 * Enable User Settings), which save remaps per player.
 */
UCLASS()
class KNIGHT_API UknightSharedSettings : public ULocalPlayerSaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 1;
	virtual int32 GetLatestDataVersion() const override { return CurrentVersion; }

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Camera", meta=(ClampMin=0.1, ClampMax=5))
	float MouseSensitivity = 1.f;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Camera", meta=(ClampMin=0.1, ClampMax=5))
	float GamepadSensitivity = 1.f;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Camera")
	bool bInvertLookY = false;

	/** Scales every camera shake (0 disables them). */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Camera", meta=(ClampMin=0, ClampMax=1))
	float CameraShakeScale = 1.f;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Accessibility")
	bool bSubtitlesEnabled = true;

	/** Volume categories, applied to the Knight Audio control buses of the same names. */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Audio", meta=(ClampMin=0, ClampMax=1))
	float MasterVolume = 1.f;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Audio", meta=(ClampMin=0, ClampMax=1))
	float MusicVolume = 0.8f;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Audio", meta=(ClampMin=0, ClampMax=1))
	float SFXVolume = 1.f;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Audio", meta=(ClampMin=0, ClampMax=1))
	float VoiceVolume = 1.f;
};
