#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "knightGameUserSettings.generated.h"

/** A current physical display. Index is only a UI hint; ID is the persisted identity. */
struct FKnightDisplayInfo
{
	FString ID;
	FString Label;
	int32 Index = INDEX_NONE;
	FIntPoint DesktopResolution = FIntPoint::ZeroValue;
	FIntPoint WorkAreaSize = FIntPoint::ZeroValue;
	int32 DPI = 0;
	bool bPrimary = false;
};

USTRUCT(BlueprintType)
struct FKnightGraphicsChoice
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) TEnumAsByte<EWindowMode::Type> WindowMode = EWindowMode::WindowedFullscreen;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FIntPoint Resolution = FIntPoint(1920, 1080);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString DisplayID;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 DisplayIndex = INDEX_NONE;
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

	/**
	 * Called only from GameInstance::Init, once per real game process. Restores the pending graphics, which are the
	 * previous display settings when the game closed before a display change was kept.
	 */
	bool ApplyPendingGraphicsAtStartup();
	/**
	 * Applies and saves Choice right away. With RevertTo, RevertTo is also saved as pending until ConfirmGraphics, so
	 * a display change that leaves a black screen is undone on the next launch even when the game is killed.
	 * bRestoring skips the option checks: going back to values the game already ran with must not be refused.
	 */
	bool ApplyGraphicsNow(const FKnightGraphicsChoice& Choice, const FKnightGraphicsChoice* RevertTo, FText& Error, bool bRestoring = false);
	/** Keeps the graphics applied by ApplyGraphicsNow and drops the pending revert. */
	void ConfirmGraphics();
	FKnightGraphicsChoice GetActiveGraphics() const;
	bool HasPendingGraphics() const { return bHasPendingGraphics; }
	/** True once after startup restored the previous display settings. */
	bool ConsumeGraphicsRevertedNotice();
	static bool ValidateGraphics(const FKnightGraphicsChoice& Choice, FText& Error);
	/** Loose check for restoring earlier values: a real window mode and a size the window can take. */
	static bool IsRestorableGraphics(const FKnightGraphicsChoice& Choice);
	static void GetSupportedResolutions(TArray<FIntPoint>& OutResolutions);
	static void GetSupportedResolutions(const FKnightGraphicsChoice& Choice, TArray<FIntPoint>& OutResolutions);
	static void GetConnectedDisplays(TArray<FKnightDisplayInfo>& OutDisplays);
	/** ID wins over the transient enumeration index. A missing saved ID returns the primary display. */
	static bool ResolveDisplay(const FString& ID, int32 Index, FKnightDisplayInfo& OutDisplay);
	/** Replaces a disconnected saved display with the primary without resetting other settings. */
	void RestoreDisconnectedDisplay();
	/** Startup display correction may precede viewport creation; consume it after OnStart. */
	bool ConsumeStartupDisplayRefresh();

	UFUNCTION(BlueprintCallable, Category="Knight|Settings")
	void SetUpscaler(EknightUpscaler NewUpscaler) { Upscaler = NewUpscaler; }

	UFUNCTION(BlueprintPure, Category="Knight|Settings")
	EknightUpscaler GetUpscaler() const { return Upscaler; }

	UFUNCTION(BlueprintCallable, Category="Knight|Settings")
	void SetMotionBlurEnabled(bool bEnabled) { bMotionBlurEnabled = bEnabled; }

	UFUNCTION(BlueprintPure, Category="Knight|Settings")
	bool IsMotionBlurEnabled() const { return bMotionBlurEnabled; }

private:
	/** Sets the engine values without applying them. */
	void SetGraphics(const FKnightGraphicsChoice& Choice);

	UPROPERTY(Config)
	FKnightGraphicsChoice PendingGraphics;

	UPROPERTY(Config)
	bool bHasPendingGraphics = false;

	UPROPERTY(Config)
	bool bKnightGraphicsInitialized = false;

	UPROPERTY(Config)
	int32 PendingGraphicsSaveSerial = 0;

	bool bGraphicsRevertedNotice = false;
	bool bNeedsStartupDisplayRefresh = false;

	UPROPERTY(Config)
	EknightUpscaler Upscaler = EknightUpscaler::TSR;

	UPROPERTY(Config)
	bool bMotionBlurEnabled = true;
};
