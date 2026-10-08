#pragma once

#include "CoreMinimal.h"
#include "Settings/KnightSettingsWidget.h"
#include "Widgets/knightButtonBase.h"
#include "knightSettingRow.generated.h"

class UButton;
class UCommonTextBlock;
class USlider;
class USoundBase;
class UknightSettingRow;

DECLARE_DELEGATE_TwoParams(FknightSettingRowEdited, FName /*Id*/, const FString& /*Value*/);
DECLARE_DELEGATE_TwoParams(FknightSettingRowFocused, UknightSettingRow* /*Row*/, bool /*bKeyboardFocus*/);

/**
 * One row of the settings screen: label on the left, the control on the right. The whole row is the focusable
 * button, so arrow keys move between rows and left / right change the value.
 *
 * Rotator: left / right, the arrows or a click step through the options.
 * Slider: left / right step, the slider drags.
 * Key: Enter or a click waits for the next key or mouse button; Esc cancels.
 *
 * Layout lives in the Widget Blueprint designer (WBP_KnightSettingRow_Rotator / _Slider / _Key). Optional names:
 * Txt_Label, Txt_Value, Slider_Value, Btn_Prev, Btn_Next, Img_Dirty.
 */
UCLASS(Abstract, Blueprintable, meta=(DisableNativeTick))
class KNIGHTUI_API UknightSettingRow : public UknightButtonBase
{
	GENERATED_BODY()

public:
	UknightSettingRow(const FObjectInitializer& ObjectInitializer);

	void Setup(const FKnightSettingItem& InItem);
	/** New value, dirty mark and enabled state for the same setting. */
	void Refresh(const FKnightSettingItem& InItem);

	const FKnightSettingItem& GetItem() const { return Item; }
	bool IsCapturingKey() const { return bCapturingKey; }

	FknightSettingRowEdited OnEdited;
	/** Hovered (false) or focused (true). */
	FknightSettingRowFocused OnFocused;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnHovered() override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** Played when the value changes by a step (rotator, slider, arrows). */
	UPROPERTY(EditAnywhere, Category="Knight|Settings")
	TObjectPtr<USoundBase> ChangeSound;

	/** Shortest gap between change sounds while a slider is dragged. */
	UPROPERTY(EditAnywhere, Category="Knight|Settings", meta=(ClampMin=0, Units="s"))
	float ChangeSoundInterval = 0.05f;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_Value;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<USlider> Slider_Value;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UButton> Btn_Prev;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UButton> Btn_Next;

	/** Shown while the value is edited but not applied. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Img_Dirty;

private:
	void HandleRowClicked();
	UFUNCTION() void HandlePrevClicked();
	UFUNCTION() void HandleNextClicked();
	UFUNCTION() void HandleSliderChanged(float Value);
	void Step(int32 Direction);
	/** Keyboard step: 1 up to a 50-wide range, 5 up to 100, 10 above. */
	float GetSliderStep() const;
	void Edit(const FString& Value);
	void SetCapturingKey(bool bCapturing);
	void FinishKeyCapture(const FKey& Key);
	void RefreshValueText();
	void PlayChangeSound();

	FKnightSettingItem Item;
	bool bCapturingKey = false;
	bool bUpdating = false;
	double LastSoundTime = 0.0;
};
