#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Containers/Ticker.h"
#include "Settings/knightGameUserSettings.h"
#include "KnightSettingsWidget.generated.h"

class UknightSharedSettings;
class UknightSettingsSubsystem;

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
	/** 화면에 보이는 선택지 이름. Options와 같은 순서. Options는 언어와 상관없는 키("Low", "On")라서 그대로 보여 주지 않는다.
	 *  비어 있거나 칸이 모자라면 Options 값을 그대로 보여 준다(해상도·FPS 숫자). */
	UPROPERTY(BlueprintReadOnly, Category="Settings") TArray<FText> OptionLabels;
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
 * Settings model shared by title, lobby and raid: graphics (UknightGameUserSettings), personal settings
 * (UknightSharedSettings) and key bindings (Enhanced Input user settings), each edited as a draft and saved by Apply.
 * Graphics apply at once. A display, window mode or resolution change then waits DisplayConfirmSeconds for Keep and goes back to
 * the previous values otherwise, so a mode the monitor cannot show recovers by itself.
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
	friend class UknightSettingsCheckCommandlet;

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

	static constexpr double DisplayConfirmSeconds = 15.0;
	/** True while a new display, window mode or resolution waits for KeepDisplay. Editing is locked meanwhile. */
	bool IsDisplayConfirmShown() const { return bDisplayConfirmShown; }
	void KeepDisplay();
	/** Goes back to the display, window mode and resolution from before Apply. Also runs when the countdown ends. */
	void RevertDisplay();

	static const FString OnText;
	static const FString OffText;

protected:
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual void NativeDestruct() override;

	/** The tab changed or its rows changed shape (Open, Revert, defaults): rebuild the list. */
	virtual void HandleTabChanged() {}
	/** Values changed in place, possibly on other rows (presets, window mode): refresh the rows' values. */
	virtual void HandleValuesChanged() {}
	virtual void HandleStatusChanged(const FText& Status) {}
	virtual void HandleUnsavedPrompt(bool bShow) {}
	/** Show or hide the keep / revert choice after a display change. The countdown arrives through HandleStatusChanged. */
	virtual void HandleDisplayConfirm(bool bShow) {}
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
	void PreviewDisplay(const UknightSharedSettings* Settings) const;
	void EndDisplayPreview() const;
	void CloseNow();
	void ApplyPreset(int32 Quality);

	void ShowUnsavedPrompt(bool bShow);
	void StartDisplayConfirm(const FKnightGraphicsChoice& Previous);
	void StopDisplayConfirm();
	bool TickDisplayConfirm(float DeltaTime);
	void UpdateDisplayCountdown();
	/** bRefreshScreen is false while the widget is being destroyed. */
	void RestoreDisplay(bool bRefreshScreen);

	int32 CurrentTab = 0;
	bool bUnsavedPromptShown = false;
	bool bDisplayConfirmShown = false;
	/** Set when Apply came from the close prompt: keeping the display then closes the screen. */
	bool bCloseAfterDisplayConfirm = false;
	double DisplayConfirmDeadline = 0.0;
	int32 DisplaySecondsShown = -1;
	FKnightGraphicsChoice DisplayRevertGraphics;
	FTSTicker::FDelegateHandle DisplayTicker;
	FKnightGraphicsChoice SavedGraphics;
	FKnightGraphicsChoice DraftGraphics;
	UPROPERTY(Transient) TObjectPtr<UknightSharedSettings> SavedPersonal;
	UPROPERTY(Transient) TObjectPtr<UknightSharedSettings> DraftPersonal;
	TMap<FName, FKey> SavedKeys;
	TMap<FName, FKey> DraftKeys;
	/** 게임 언어("ko", "en" ...). 적용할 때 바꾸고 GameUserSettings.ini에 저장한다. */
	FString SavedCulture;
	FString DraftCulture;
	TMap<FName, FKey> DefaultKeys;
	TMap<FName, FText> KeyLabels;
};
