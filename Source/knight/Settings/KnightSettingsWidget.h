#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Settings/knightGameUserSettings.h"
#include "KnightSettingsWidget.generated.h"

class UknightSharedSettings;

UENUM(BlueprintType)
enum class EKnightSettingControl : uint8
{
	/** Steps through Options with left / right (on / off switches too). */
	Rotator,
	/** Number between Min and Max. */
	Slider,
	/** Keyboard or mouse key for an Enhanced Input mapping. */
	Key
};

/** One editable setting as the screen shows it. Values are strings so every control type shares OptionEdited. */
USTRUCT(BlueprintType)
struct KNIGHT_API FKnightSettingItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Settings") FName Id;
	UPROPERTY(BlueprintReadOnly, Category="Settings") FText Label;
	UPROPERTY(BlueprintReadOnly, Category="Settings") FText Description;
	UPROPERTY(BlueprintReadOnly, Category="Settings") EKnightSettingControl Type = EKnightSettingControl::Rotator;
	/** Rotator choices, also the values passed to OptionEdited. */
	UPROPERTY(BlueprintReadOnly, Category="Settings") TArray<FString> Options;
	/** Rotator: one of Options. Slider: number. Key: FKey name. */
	UPROPERTY(BlueprintReadOnly, Category="Settings") FString Value;
	UPROPERTY(BlueprintReadOnly, Category="Settings") float Min = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Settings") float Max = 1.f;
	UPROPERTY(BlueprintReadOnly, Category="Settings") float Step = 1.f;
	UPROPERTY(BlueprintReadOnly, Category="Settings") bool bEnabled = true;
	/** Edited but not applied yet. */
	UPROPERTY(BlueprintReadOnly, Category="Settings") bool bDirty = false;
};

/** A titled group of settings within a tab. */
USTRUCT(BlueprintType)
struct KNIGHT_API FKnightSettingSection
{
	GENERATED_BODY()

	/** Small caps header ("VOLUME"). */
	UPROPERTY(BlueprintReadOnly, Category="Settings") FText Caption;
	UPROPERTY(BlueprintReadOnly, Category="Settings") TArray<FKnightSettingItem> Items;
	/** Optional line under the items. */
	UPROPERTY(BlueprintReadOnly, Category="Settings") FText Note;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FKnightSettingsClosed);

/**
 * Settings model shared by title, lobby and raid: graphics (queued for the next launch), personal settings
 * (UknightSharedSettings) and key bindings (Enhanced Input user settings), each edited as a draft and saved by Apply.
 *
 * This class has no layout. The screen is a Blueprint of a knightUI subclass (UknightSettingsScreen) found through
 * GetSharedWidgetClass; it lists GetTabSections, sends edits to OptionEdited and redraws in the Handle* hooks.
 */
UCLASS(Abstract)
class KNIGHT_API UKnightSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	static constexpr int32 TabCount = 4;

	UPROPERTY(BlueprintAssignable, Category="Knight|Settings")
	FKnightSettingsClosed OnClosed;

	/** Loads the stored values into a fresh draft and shows the first tab. */
	void Open();
	void FocusSettings();
	/** Closes, or asks first when there are unapplied edits. */
	void RequestClose();
	/** True while a key row waits for a key; Esc then cancels the capture instead of closing. */
	virtual bool IsCapturingKey() const { return false; }
	static TSubclassOf<UKnightSettingsWidget> GetSharedWidgetClass();

	void OptionEdited(FName Id, const FString& Value);
	void SetTab(int32 Index);
	int32 GetCurrentTab() const { return CurrentTab; }
	static FText GetTabLabel(int32 Index);
	/** English caption above the tab label. */
	static FText GetTabCaption(int32 Index);
	void GetTabSections(int32 Tab, TArray<FKnightSettingSection>& OutSections) const;
	int32 GetDirtyCount() const;
	bool IsDirty() const;

	void Apply();
	/** Drops the draft back to the stored values. */
	void Revert();
	/** Loads the defaults into the draft; Apply saves them. */
	void RestoreDefaults();

	/** Answers to the unsaved-changes prompt. */
	void ConfirmApply();
	void ConfirmDiscard();
	void ConfirmContinue();

	static const FString OnText;
	static const FString OffText;

protected:
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

	/** The tab changed or its rows changed shape (Open, Revert, defaults): rebuild the list. */
	virtual void HandleTabChanged() {}
	/** Values changed in place, possibly on other rows (presets, window mode): refresh the rows' values. */
	virtual void HandleValuesChanged() {}
	virtual void HandleStatusChanged(const FText& Status) {}
	virtual void HandleUnsavedPrompt(bool bShow) {}
	/** Called when FocusSettings runs; put focus on the first row. */
	virtual void HandleFocusRequested() { SetKeyboardFocus(); }
	/** Start of Open, before the first HandleTabChanged. */
	virtual void HandleOpened() {}
	/** Esc that is not a key capture or the unsaved prompt. Return true when the screen used it (leaving a sub-list). */
	virtual bool HandleBackRequested() { return false; }

private:
	FString GetValue(FName Id, bool bSaved) const;
	bool IsItemDirty(FName Id) const;
	void SetStatus(const FText& Text);
	bool IsPersonalDirty() const;
	bool IsKeyDirty() const;
	void CopyPersonal(UknightSharedSettings* To, const UknightSharedSettings* From) const;
	/** Plays the draft volumes, so sliders are heard while dragging. Revert and close play the stored ones again. */
	void PreviewAudio(const UknightSharedSettings* Settings) const;
	void CloseNow();
	void ApplyPreset(int32 Quality);

	void ShowUnsavedPrompt(bool bShow);

	int32 CurrentTab = 0;
	bool bUnsavedPromptShown = false;
	FKnightGraphicsChoice SavedGraphics;
	FKnightGraphicsChoice DraftGraphics;
	UPROPERTY(Transient) TObjectPtr<UknightSharedSettings> SavedPersonal;
	UPROPERTY(Transient) TObjectPtr<UknightSharedSettings> DraftPersonal;
	TMap<FName, FKey> SavedKeys;
	TMap<FName, FKey> DraftKeys;
	TMap<FName, FKey> DefaultKeys;
	TMap<FName, FText> KeyLabels;
};
