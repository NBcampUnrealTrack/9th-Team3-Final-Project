#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "knightSettingsSubsystem.generated.h"

class UknightSharedSettings;

/** Loads and saves the local player's UknightSharedSettings. Gameplay reads settings through here. */
UCLASS()
class KNIGHT_API UknightSettingsSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	/** Loaded on first access (which also applies the audio volumes). Never null once a local player exists. */
	UFUNCTION(BlueprintCallable, Category="Knight|Settings")
	UknightSharedSettings* GetSharedSettings();

	/** Applies and writes the shared settings asynchronously. Call after the settings screen applies changes. */
	UFUNCTION(BlueprintCallable, Category="Knight|Settings")
	void SaveSharedSettings();

	/** Pushes the volume settings to UknightAudioSubsystem. */
	UFUNCTION(BlueprintCallable, Category="Knight|Settings")
	void ApplyAudioSettings();

	static const FString SlotName;

protected:
	/** Loads the settings as soon as the player has a controller, so menus already play at the saved volume. */
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UknightSharedSettings> SharedSettings;
};
