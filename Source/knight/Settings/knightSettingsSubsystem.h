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
	/** Loaded on first access. Never null once a local player exists. */
	UFUNCTION(BlueprintCallable, Category="Knight|Settings")
	UknightSharedSettings* GetSharedSettings();

	/** Writes the shared settings asynchronously. Call after the settings screen applies changes. */
	UFUNCTION(BlueprintCallable, Category="Knight|Settings")
	void SaveSharedSettings();

	static const FString SlotName;

private:
	UPROPERTY(Transient)
	TObjectPtr<UknightSharedSettings> SharedSettings;
};
