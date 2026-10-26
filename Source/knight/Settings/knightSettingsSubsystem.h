#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "knightSettingsSubsystem.generated.h"

class UknightSharedSettings;
class FknightDisplayCalibrationViewExtension;

/** Loads and saves the local player's UknightSharedSettings. Gameplay reads settings through here. */
UCLASS()
class KNIGHT_API UknightSettingsSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	/** Loaded on first access (which also applies the audio volumes). Never null once a local player exists. */
	UFUNCTION(BlueprintCallable, Category="Knight|Settings")
	UknightSharedSettings* GetSharedSettings();

	/** Synchronous save, so the settings screen can report failures accurately. */
	UFUNCTION(BlueprintCallable, Category="Knight|Settings")
	bool SaveSharedSettings();

	/** Pushes the volume settings to UknightAudioSubsystem. */
	UFUNCTION(BlueprintCallable, Category="Knight|Settings")
	void ApplyAudioSettings();

	/** Temporary calibration used by the settings screen. It never changes the saved object. */
	void PreviewDisplaySettings(float FieldOfViewOffset, float Brightness, float Gamma);
	void EndDisplayPreview();
	void GetEffectiveDisplaySettings(float& FieldOfViewOffset, float& Brightness, float& Gamma);

	UFUNCTION(BlueprintPure, Category="Knight|Settings")
	bool AreSubtitlesEnabled();

	static const FString SlotName;

protected:
	/** Loads the settings as soon as the player has a controller, so menus already play at the saved volume. */
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

private:
	TSharedPtr<FknightDisplayCalibrationViewExtension, ESPMode::ThreadSafe> DisplayCalibration;
	void RegisterMappableContexts();
	bool bMappableContextsRegistered = false;
	UPROPERTY(Transient)
	TObjectPtr<UknightSharedSettings> SharedSettings;
	bool bDisplayPreviewActive = false;
	float PreviewFieldOfViewOffset = 0.f;
	float PreviewBrightness = 1.f;
	float PreviewGamma = 1.f;
	void RefreshActiveCamera();
};
