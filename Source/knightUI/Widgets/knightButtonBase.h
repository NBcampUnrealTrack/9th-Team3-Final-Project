#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Containers/Ticker.h"
#include "knightButtonBase.generated.h"

class UCommonTextBlock;
class UCommonTextStyle;

/**
 * Base for every menu button (WBP_KnightButton). Visuals per state come from the CommonButtonStyle; this class adds
 * the motion from the UI style guide: hover/press scale, focus brackets that open on hover, and a shake when a locked
 * button is clicked.
 *
 * Layout lives in the Widget Blueprint designer. The widgets are found by name, so a redesigned layout keeps working
 * as long as it keeps these names: Txt_Label (CommonTextBlock), Bracket_L and Bracket_R (any widget).
 *
 * For a button that must explain why it cannot be used, call SetLockedWithReason instead of disabling it: a disabled
 * button ignores clicks, a locked one plays the locked sound from the style, shakes and shows the reason as a tooltip.
 */
UCLASS(Abstract, Blueprintable, meta=(DisableNativeTick))
class KNIGHTUI_API UknightButtonBase : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	/** Opt-in Settings-only static presentation: 0 row, 1 tab, 2 outlined action, 3 text Back. */
	void ConfigureSettingsButton(uint8 Kind);
	/** Rounded-box outlines ignore inherited tint in Slate; Settings exit supplies its group fade. */
	void SetSettingsOutlineOpacity(float Alpha);
	void SetSettingsSelected(bool bInSelected);
	/** Owner-controlled tab/list selection; actions never latch. Opt-in so other screens retain their presentation. */
	UFUNCTION(BlueprintCallable, Category="Knight|Button")
	void ConfigureMaintenanceButton(bool bInSelectable, bool bMainTab = false);
	/** Static blue feedback, scoped to the operation-map information panel. */
	UFUNCTION(BlueprintCallable, Category="Knight|Button")
	void ConfigureOperationMapButton(bool bInSelectable);
	UFUNCTION(BlueprintCallable, Category="Knight|Button")
	void SetButtonText(const FText& InText);

	UFUNCTION(BlueprintPure, Category="Knight|Button")
	FText GetButtonText() const { return ButtonText; }

	/** Locks or unlocks the button. The reason is shown as the tooltip while locked. */
	UFUNCTION(BlueprintCallable, Category="Knight|Button")
	void SetLockedWithReason(bool bInLocked, const FText& InReason);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	virtual void NativeOnPressed() override;
	virtual void NativeOnReleased() override;
	virtual void NativeOnClicked() override;
	virtual void NativeOnEnabled() override;
	virtual void NativeOnDisabled() override;
	virtual void NativeOnCurrentTextStyleChanged() override;
	virtual void HandleFocusReceived() override;
	virtual void HandleFocusLost() override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Knight|Button", meta=(ExposeOnSpawn=true))
	FText ButtonText;

	/** Tooltip while locked. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Knight|Button", meta=(ExposeOnSpawn=true))
	FText LockReason;

	/** Text style while locked. The style asset has no locked state of its own. */
	UPROPERTY(EditAnywhere, Category="Knight|Button")
	TSubclassOf<UCommonTextStyle> LockedTextStyle;

	/** Text style while the button has keyboard focus. The style asset only knows mouse hover. Unset = no change. */
	UPROPERTY(EditAnywhere, Category="Knight|Button")
	TSubclassOf<UCommonTextStyle> FocusedTextStyle;

	/** Hovering moves focus here, so a list highlights one item whether the mouse or the keyboard is used. */
	UPROPERTY(EditAnywhere, Category="Knight|Button")
	bool bFocusOnHover = false;

	/** Opacity of the whole button while locked. */
	UPROPERTY(EditAnywhere, Category="Knight|Button", meta=(ClampMin=0, ClampMax=1))
	float LockedOpacity = 0.55f;

	UPROPERTY(EditAnywhere, Category="Knight|Button|Motion", meta=(ClampMin=0.5, ClampMax=1.5))
	float HoverScale = 1.04f;

	UPROPERTY(EditAnywhere, Category="Knight|Button|Motion", meta=(ClampMin=0.5, ClampMax=1.5))
	float PressedScale = 0.95f;

	/** Higher is snappier. 25 settles in about 0.1 s. */
	UPROPERTY(EditAnywhere, Category="Knight|Button|Motion", meta=(ClampMin=1))
	float ScaleInterpSpeed = 25.f;

	UPROPERTY(EditAnywhere, Category="Knight|Button|Motion", meta=(ClampMin=1))
	float BracketInterpSpeed = 25.f;

	/** Bracket height at the start of the open motion, as a fraction of full height. */
	UPROPERTY(EditAnywhere, Category="Knight|Button|Motion", meta=(ClampMin=0, ClampMax=1))
	float BracketClosedScale = 0.4f;

	UPROPERTY(EditAnywhere, Category="Knight|Button|Motion", meta=(ClampMin=0, Units="s"))
	float ShakeDuration = 0.24f;

	/** Slate units (pixels at 1080p). */
	UPROPERTY(EditAnywhere, Category="Knight|Button|Motion", meta=(ClampMin=0))
	float ShakeAmplitude = 4.f;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Button", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_Label;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Button", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Bracket_L;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Button", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Bracket_R;

private:
	void RefreshSettingsVisual();
	bool bSettingsPresentation = false;
	bool bSettingsSelected = false;
	uint8 SettingsKind = 0;
	float SettingsOutlineOpacity = 1.f;
	void HandleLockClicked();
	void RefreshTargets();
	void RefreshLockVisual();
	void ApplyLabelStyle();
	void StartAnimating();
	void StopAnimating();
	bool TickMotion(float DeltaTime);
	void ApplyMotion();

	float CurrentScale = 1.f;
	float TargetScale = 1.f;
	float BracketAlpha = 0.f;
	float TargetBracketAlpha = 0.f;
	float ShakeTimeLeft = 0.f;
	bool bPressedVisual = false;
	bool bFocusedVisual = false;
	bool bMaintenancePresentation = false;
	bool bOperationMapPresentation = false;

	FTSTicker::FDelegateHandle MotionTicker;
};
