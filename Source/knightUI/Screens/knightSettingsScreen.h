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
class UImage;

/** Horizontal category tab with a static cyan selected underline. */
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
	/** Down arrow: go into the tab's rows (Enter and clicks arrive through OnClicked). */
	FSimpleDelegate OnTabEntered;

protected:
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_Caption;

	/** Thin cyan underline for the active category. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Settings", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Img_Selected;
};

/** Localized section caption followed by a thin horizontal rule. */
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
 * Shared Settings view: top category tabs, scrolling rows, description and fixed footer.
 * Left/right on tabs changes category; Down/Enter enters rows. Row left/right edits;
 * Up/down navigates. Esc returns to categories before the normal unsaved-close flow.
 * Q/E or controller shoulders switches tabs. R defaults and F applies.
 * Values, previews and persistence remain in UKnightSettingsWidget.
 */
UCLASS(Abstract, Blueprintable, meta=(DisableNativeTick))
class KNIGHTUI_API UknightSettingsScreen : public UKnightSettingsWidget
{
	GENERATED_BODY()

public:
	virtual bool IsCapturingKey() const override;
	friend class UknightSettingsCheckCommandlet;

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
	virtual void HandleExitChromeOpacity(float Alpha) override;
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
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UknightButtonBase> Btn_Back;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWidget> Panel_Editor;

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
	UPROPERTY(Transient)
	TObjectPtr<UImage> CalibrationBackdrop;
	FLinearColor OriginalBackdropTint = FLinearColor::White;

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
	bool bPromptVisible = false;
	TWeakObjectPtr<UknightSettingRow> DetailRow;
	FText PromptApplyText;
	FText PromptDiscardText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UknightSettingRow>> Rows;
};
