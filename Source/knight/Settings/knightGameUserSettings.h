#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "knightGameUserSettings.generated.h"

USTRUCT(BlueprintType)
struct FKnightGraphicsChoice
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) TEnumAsByte<EWindowMode::Type> WindowMode = EWindowMode::WindowedFullscreen;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FIntPoint Resolution = FIntPoint(1920, 1080);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 FrameRateLimit = 60;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bVSync = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 AntiAliasing = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Shadows = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Textures = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Effects = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PostProcess = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ViewDistance = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bMotionBlur = true;

	bool operator==(const FKnightGraphicsChoice& Other) const;
	bool operator!=(const FKnightGraphicsChoice& Other) const { return !(*this == Other); }
};

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

	/** Called only from GameInstance::Init, once per real game process. */
	bool ApplyPendingGraphicsAtStartup();
	bool QueueGraphicsForNextProcess(const FKnightGraphicsChoice& Choice, FText& Error);
	FKnightGraphicsChoice GetDisplayedGraphics() const;
	FKnightGraphicsChoice GetActiveGraphics() const;
	bool HasPendingGraphics() const { return bHasPendingGraphics; }
	bool ConsumeGraphicsAppliedNotice();
	static bool ValidateGraphics(const FKnightGraphicsChoice& Choice, FText& Error);
	static void GetSupportedResolutions(TArray<FIntPoint>& OutResolutions);

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
	FKnightGraphicsChoice PendingGraphics;

	UPROPERTY(Config)
	bool bHasPendingGraphics = false;

	UPROPERTY(Config)
	bool bKnightGraphicsInitialized = false;

	UPROPERTY(Config)
	int32 PendingGraphicsSaveSerial = 0;

	bool bGraphicsAppliedNotice = false;
	FKnightGraphicsChoice ActiveGraphicsSnapshot;
	UPROPERTY(Config)
	EknightUpscaler Upscaler = EknightUpscaler::TSR;

	UPROPERTY(Config)
	bool bMotionBlurEnabled = true;
};
