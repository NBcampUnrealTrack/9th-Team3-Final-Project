#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Containers/Ticker.h"
#include "FieldNotificationId.h"
#include "knightHUDLowHealth.generated.h"

class UImage;
class UknightVM_PlayerStatus;

/**
 * Red screen edges and scan lines while the Knight's health is low, beating like a heart. Turns on at the same ratio as the
 * HealthWarning voice (UknightUISettings::HealthWarningRatio), gets darker and beats faster toward 0, and fades out on heal.
 * Ticks only while shown.
 *
 * Widgets are found by name: Img_Vignette (stretched over the screen) and Img_Scanlines (tiled, one tile taller than the screen).
 */
UCLASS(Abstract, Blueprintable, meta=(DisableNativeTick))
class KNIGHTUI_API UknightHUDLowHealth : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UImage> Img_Vignette;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UImage> Img_Scanlines;

	/** Heartbeat period right at the threshold; matches the HUD health bar pulse. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD", meta=(ClampMin=0.1, Units="s"))
	float SlowBeatPeriod = 1.2f;

	/** Heartbeat period at 0 health. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD", meta=(ClampMin=0.1, Units="s"))
	float FastBeatPeriod = 0.6f;

	/** Resting opacity between beats, at the threshold and at 0 health. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD", meta=(ClampMin=0, ClampMax=1))
	FVector2D BaseOpacity = FVector2D(0.35f, 0.75f);

	/** Opacity added on the main beat, at the threshold and at 0 health. The second beat is 60% of it. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD", meta=(ClampMin=0, ClampMax=1))
	FVector2D BeatOpacity = FVector2D(0.25f, 0.4f);

	/** Scan line opacity relative to the edges. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD", meta=(ClampMin=0, ClampMax=1))
	float ScanlineOpacity = 0.7f;

	/** Scan lines drift downward at this speed, in slate units per second. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD", meta=(ClampMin=0))
	float ScanlineSpeed = 18.f;

	/** Height of one scan line tile in the brush; the drift wraps at it. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD", meta=(ClampMin=1))
	float ScanlineTile = 6.f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD", meta=(ClampMin=0, Units="s"))
	float FadeOutTime = 0.5f;

private:
	void HandleViewModelChanged(UObject* InViewModel, UE::FieldNotification::FFieldId FieldId);
	void Refresh();
	bool Tick(float DeltaTime);

	TWeakObjectPtr<UknightVM_PlayerStatus> ViewModel;
	FTSTicker::FDelegateHandle Ticker;
	/** 0 at the threshold, 1 at 0 health. */
	float Severity = 0.f;
	/** 1 while low, falls to 0 over FadeOutTime after a heal. */
	float Shown = 0.f;
	bool bLow = false;
	float BeatTime = 0.f;
	float ScanOffset = 0.f;
};
