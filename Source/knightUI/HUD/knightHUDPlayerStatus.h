#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Containers/Ticker.h"
#include "FieldNotificationId.h"
#include "knightHUDPlayerStatus.generated.h"

class UCommonTextBlock;
class UProgressBar;
class UknightVM_PlayerStatus;

/**
 * Player panel of the HUD, laid out over the whole screen: health (large number + bar with a delayed "ghost" bar) at the
 * bottom left, boost gauge at the bottom centre with the overheat warning and a red blinking frame when it runs low.
 *
 * Reads the PlayerStatus view model owned by UknightUISubsystem. Widgets are found by name (see the BindWidgetOptional
 * properties), so the layout can be redesigned in the Widget Blueprint as long as the names stay.
 * Animations (ghost drain, low-health and overheat pulses) tick only while they run.
 */
UCLASS(Abstract, Blueprintable, meta=(DisableNativeTick))
class KNIGHTUI_API UknightHUDPlayerStatus : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> Bar_Health;

	/** Behind Bar_Health; shows recent damage before draining down to the real value. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> Bar_HealthGhost;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> Bar_Boost;

	/** Current health, "72" */
	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_Health;

	/** "/100" */
	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_HealthMax;

	/** Frame around the boost bar; blinks while the gauge is at or below LowBoostThreshold. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Img_BoostAlert;

	/** "84%" */
	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_Boost;

	/** Shown only while overheated, so the state never depends on color alone. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_Overheat;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Colors")
	FLinearColor HealthColor = FLinearColor(FColor(0xF4, 0xF8, 0xFC));

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Colors")
	FLinearColor LowHealthColor = FLinearColor(FColor(0xE5, 0x48, 0x4D));

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Colors")
	FLinearColor GhostColor = FLinearColor(FColor(0xC2, 0x54, 0x1A));

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Colors")
	FLinearColor BoostColor = FLinearColor(FColor(0xA8, 0xC8, 0xF0));

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Colors")
	FLinearColor OverheatColor = FLinearColor(FColor(0xFF, 0x7A, 0x1A));

	/** Health fraction at or below which the health bar turns red and pulses. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Motion", meta=(ClampMin=0, ClampMax=1))
	float LowHealthThreshold = 0.3f;

	/** Boost fraction at or below which the frame around the boost bar blinks red. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Motion", meta=(ClampMin=0, ClampMax=1))
	float LowBoostThreshold = 0.2f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Motion", meta=(ClampMin=0, Units="s"))
	float GhostHoldTime = 0.5f;

	/** Time for the ghost bar to drain the whole bar; smaller hits drain proportionally faster. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Motion", meta=(ClampMin=0.01, Units="s"))
	float GhostDrainTime = 0.4f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Motion", meta=(ClampMin=0.1, Units="s"))
	float PulsePeriod = 1.2f;

	/** Lowest opacity reached by a pulsing bar. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Motion", meta=(ClampMin=0, ClampMax=1))
	float PulseMinOpacity = 0.45f;

private:
	void HandleViewModelChanged(UObject* InViewModel, UE::FieldNotification::FFieldId FieldId);
	void Refresh(bool bSnapGhost);
	void UpdateTicker();
	bool TickAnimation(float DeltaTime);
	void ApplyAnimation(float DeltaTime);
	bool NeedsTick() const;

	TWeakObjectPtr<UknightVM_PlayerStatus> ViewModel;

	float HealthPercent = 0.f;
	float GhostPercent = 0.f;
	float GhostHoldLeft = 0.f;
	float PulseTime = 0.f;
	bool bLowHealth = false;
	bool bOverheated = false;
	bool bLowBoost = false;

	FTSTicker::FDelegateHandle AnimTicker;
};
