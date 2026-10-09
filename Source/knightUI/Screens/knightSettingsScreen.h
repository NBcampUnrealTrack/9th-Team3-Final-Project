#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Settings/KnightSettingsWidget.h"
#include "Widgets/knightButtonBase.h"
#include "knightSettingsScreen.generated.h"

class UCommonTextBlock;
class UCommonTextStyle;
class UPanelWidget;
class UScrollBox;
class USoundBase;
class UknightSettingRow;

/**
 * Category button on the left of the settings screen: English caption over the label, marker when selected.
 * Label colors come from the button style's normal / selected text styles.
 */
UCLASS(Abstract, Blueprintable, meta=(DisableNativeTick))
class KNIGHTUI_API UknightSettingsTab : public UknightButtonBase
{
	GENERATED_BODY()

public:
	UknightSettingsTab(const FObjectInitializer& ObjectInitializer);

	void SetTab(const FText& Caption, const FText& Label);
	void SetTabSelected(bool bInSelected);

	/** Keyboard or mouse focus arrived on the tab. */
	FSimpleDelegate OnTabFocused;
	/** Right arrow: go into the tab's rows (Enter and clicks arrive through OnClicked). */
	FSimpleDelegate OnTabEntered;

protected:
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_Caption;

	/** Orange bar and tint behind the selected tab. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Img_Selected;
};

/** Small caps band above a group of rows ("VOLUME"). */
UCLASS(Abstract, Blueprintable, meta=(DisableNativeTick))
class KNIGHTUI_API UknightSettingsSectionHeader : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetCaption(const FText& Caption);

protected:
	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_Caption;
};

/**
 * The settings screen (WBP_KnightSettings): categories on the left, the rows of the selected category in the middle,
 * the focused row's description on the right, apply / revert at the bottom. The values and saving live in the base
 * class (UKnightSettingsWidget, knight module), which title, lobby and raid open through GetSharedWidgetClass.
 *
 * Focus has two levels, like console settings menus. On the categories, up / down pick one (its rows show at once)
 * and Enter / right / a click go into its rows. In the rows, up / down move, left / right change the value, Esc goes
 * back to the categories. Q / E switch categories, R loads the defaults, F applies. Esc on the categories closes
 * (asks first when edits are not applied). Down from the last category reaches the footer buttons; up or Esc there
 * returns to the categories. The key hints at the bottom follow the level.
 *
 * Layout lives in the Widget Blueprint designer. Required: Box_Rows (panel the rows go into), Btn_Apply, Btn_Revert.
 * Optional: Tab_0 .. Tab_3, Scroll_Rows, Txt_DetailTitle, Txt_DetailBody, Panel_Chart, Txt_Status, Txt_DirtyCount,
 * Btn_Defaults, Panel_Prompt, Panel_Footer, Btn_PromptApply, Btn_PromptDiscard, Btn_PromptContinue,
 * Panel_HintsTabs, Panel_HintsRows.
 */
UCLASS(Abstract, Blueprintable, meta=(DisableNativeTick))
class KNIGHTUI_API UknightSettingsScreen : public UKnightSettingsWidget
{
	GENERATED_BODY()

public:
	virtual bool IsCapturingKey() const override;

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	virtual void HandleTabChanged() override;
	virtual void HandleValuesChanged() override;
	virtual void HandleStatusChanged(const FText& Status) override;
	virtual void HandleUnsavedPrompt(bool bShow) override;
	/** Reuses Panel_Prompt: Btn_PromptApply keeps, Btn_PromptDiscard reverts, Btn_PromptContinue hides. */
	virtual void HandleDisplayConfirm(bool bShow) override;
	virtual void HandleFocusRequested() override;
	virtual void HandleOpened() override;
	virtual bool HandleBackRequested() override;

	UPROPERTY(EditAnywhere, Category="Knight|Settings|Classes")
	TSubclassOf<UknightSettingRow> RotatorRowClass;

	UPROPERTY(EditAnywhere, Category="Knight|Settings|Classes")
	TSubclassOf<UknightSettingRow> SliderRowClass;

	UPROPERTY(EditAnywhere, Category="Knight|Settings|Classes")
	TSubclassOf<UknightSettingRow> KeyRowClass;

	UPROPERTY(EditAnywhere, Category="Knight|Settings|Classes")
	TSubclassOf<UknightSettingsSectionHeader> SectionHeaderClass;

	/** Style of the note line under a section. */
	UPROPERTY(EditAnywhere, Category="Knight|Settings|Classes")
	TSubclassOf<UCommonTextStyle> NoteTextStyle;

	/** Played when Q / E switch categories (the tab buttons play their own click sound). */
	UPROPERTY(EditAnywhere, Category="Knight|Settings")
	TObjectPtr<USoundBase> TabSound;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidget))
	TObjectPtr<UPanelWidget> Box_Rows;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidget))
	TObjectPtr<UknightButtonBase> Btn_Apply;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidget))
	TObjectPtr<UknightButtonBase> Btn_Revert;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UknightButtonBase> Btn_Defaults;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UknightSettingsTab> Tab_0;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UknightSettingsTab> Tab_1;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UknightSettingsTab> Tab_2;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UknightSettingsTab> Tab_3;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UScrollBox> Scroll_Rows;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_DetailTitle;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_DetailBody;

	/** Gray steps for gamma, shown on the calibration tab only. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Chart;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_Status;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_DirtyCount;

	/** Unsaved-changes strip; replaces the footer buttons while shown. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Prompt;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Footer;

	/** Key hints shown while the categories have focus. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Panel_HintsTabs;

	/** Key hints shown while the rows have focus. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Panel_HintsRows;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UknightButtonBase> Btn_PromptApply;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UknightButtonBase> Btn_PromptDiscard;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UknightButtonBase> Btn_PromptContinue;

private:
	UknightSettingsTab* GetTabButton(int32 Index) const;
	void HandleRowFocused(UknightSettingRow* Row, bool bKeyboardFocus);
	void HandleRowEdited(FName Id, const FString& Value);
	void HandleTabFocused(int32 Index);
	void RefreshFooter();
	void SwitchTab(int32 Direction);
	/** Moves focus into the rows (true) or back to the selected category (false). */
	void FocusLevel(bool bRows);
	bool IsFooterFocused() const;
	/** Focuses the first enabled footer button. False when none can take focus. */
	bool FocusFooter();
	void SetRowsLevel(bool bRows);
	void ShowPrompt(bool bShow, UknightButtonBase* FocusTarget);

	bool bRowsLevel = false;
	FText PromptApplyText;
	FText PromptDiscardText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UknightSettingRow>> Rows;
};
