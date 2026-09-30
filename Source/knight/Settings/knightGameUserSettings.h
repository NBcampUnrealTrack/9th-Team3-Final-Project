#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "knightGameUserSettings.generated.h"

/** Anti-aliasing / upscaling method. Vendor upscalers (DLSS, FSR, XeSS) are added here when their plugins are. */
UENUM(BlueprintType)
enum class EknightUpscaler : uint8
{
	TSR UMETA(DisplayName="Temporal Super Resolution"),
	TAA UMETA(DisplayName="Temporal AA"),
	None UMETA(DisplayName="Off")
};

/**
 * Machine settings (GameUserSettings.ini): resolution, quality, frame rate from the engine, plus the upscaler
 * and motion blur. Everything that follows the player instead of the machine is in UknightSharedSettings.
 * Registered as GameUserSettingsClassName in DefaultEngine.ini.
 */
UCLASS()
class KNIGHT_API UknightGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	static UknightGameUserSettings* Get();

	virtual void SetToDefaults() override;
	virtual void ApplyNonResolutionSettings() override;

	UFUNCTION(BlueprintCallable, Category="Knight|Settings")
	void SetUpscaler(EknightUpscaler NewUpscaler) { Upscaler = NewUpscaler; }

	UFUNCTION(BlueprintPure, Category="Knight|Settings")
	EknightUpscaler GetUpscaler() const { return Upscaler; }

	UFUNCTION(BlueprintCallable, Category="Knight|Settings")
	void SetMotionBlurEnabled(bool bEnabled) { bMotionBlurEnabled = bEnabled; }

	UFUNCTION(BlueprintPure, Category="Knight|Settings")
	bool IsMotionBlurEnabled() const { return bMotionBlurEnabled; }

private:
	UPROPERTY(Config)
	EknightUpscaler Upscaler = EknightUpscaler::TSR;

	UPROPERTY(Config)
	bool bMotionBlurEnabled = true;
};
