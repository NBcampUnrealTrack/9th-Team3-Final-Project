#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "Styling/SlateTypes.h"
#include "Settings/knightGameUserSettings.h"
#include "KnightSettingsWidget.generated.h"

class UButton;
class UCheckBox;
class UComboBoxString;
class UHorizontalBox;
class UInputKeySelector;
class USlider;
class UTextBlock;
class UVerticalBox;
class UknightSharedSettings;

UENUM()
enum class EKnightSettingControl : uint8 { Combo, Slider, Check, Key };

class UKnightSettingsWidget;

/** A single generated row; only it binds the UMG control events. */
UCLASS()
class KNIGHT_API UKnightSettingRow : public UUserWidget
{
	GENERATED_BODY()
public:
	void Configure(UKnightSettingsWidget* InOwner, FName InId, const FText& Label, EKnightSettingControl Type,
		const TArray<FString>& Options, const FString& Value, float Min = 0.f, float Max = 1.f);
	void SetValue(const FString& Value);
	bool IsCapturingKey() const;

private:
	UFUNCTION() void HandleCombo(FString Selected, ESelectInfo::Type SelectionType);
	UFUNCTION() void HandleSlider(float Value);
	UFUNCTION() void HandleCheck(bool bChecked);
	UFUNCTION() void HandleKey(FInputChord SelectedKey);
	void Notify(const FString& Value);
	void UpdateValueText(float Value);

	TWeakObjectPtr<UKnightSettingsWidget> Owner;
	FName Id;
	float MinValue = 0.f;
	float MaxValue = 1.f;
	bool bUpdating = false;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> Combo;
	UPROPERTY(Transient) TObjectPtr<USlider> Slider;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> Check;
	UPROPERTY(Transient) TObjectPtr<UInputKeySelector> KeySelector;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ValueText;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FKnightSettingsClosed);

/** Shared, scrollable settings screen used by title, lobby and raid. */
UCLASS()
class KNIGHT_API UKnightSettingsWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable, Category="Knight|Settings")
	FKnightSettingsClosed OnClosed;

	void Open();
	void FocusSettings();
	void RequestClose();
	bool IsCapturingKey() const;
	static TSubclassOf<UKnightSettingsWidget> GetSharedWidgetClass();
	void OptionEdited(FName Id, const FString& Value);

protected:
	virtual bool Initialize() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	void BuildLayout();
	void BuildTab();
	void AddCombo(FName Id, const FText& Label, const TArray<FString>& Options, const FString& Value);
	void AddSlider(FName Id, const FText& Label, float Value, float Min, float Max);
	void AddCheck(FName Id, const FText& Label, bool bChecked);
	void AddKey(FName Id, const FText& Label, const FKey& Key);
	void AddRow(FName Id, const FText& Label, EKnightSettingControl Type, const TArray<FString>& Options,
		const FString& Value, float Min = 0.f, float Max = 1.f);
	void SetTab(int32 Index);
	void SetStatus(const FText& Text);
	void RefreshApply();
	bool IsDirty() const;
	bool IsPersonalDirty() const;
	bool IsKeyDirty() const;
	void CopyPersonal(UknightSharedSettings* To, const UknightSharedSettings* From) const;
	void Apply();
	void Cancel();
	void RestoreDefaults();
	void CloseNow();
	void ApplyPreset(int32 Quality);
	void RefreshPresetRows();
	void ShowConfirm(bool bShow);
	UFUNCTION() void TabGraphics();
	UFUNCTION() void TabControls();
	UFUNCTION() void TabAudio();
	UFUNCTION() void TabDisplay();
	UFUNCTION() void ClickApply();
	UFUNCTION() void ClickCancel();
	UFUNCTION() void ClickDefaults();
	UFUNCTION() void ClickClose();
	UFUNCTION() void ConfirmApply();
	UFUNCTION() void ConfirmDiscard();
	UFUNCTION() void ConfirmContinue();
	UFUNCTION() void ClickSubtitleExample();

	int32 CurrentTab = 0;
	bool bPendingClose = false;
	FKnightGraphicsChoice SavedGraphics;
	FKnightGraphicsChoice DraftGraphics;
	UPROPERTY(Transient) TObjectPtr<UknightSharedSettings> SavedPersonal;
	UPROPERTY(Transient) TObjectPtr<UknightSharedSettings> DraftPersonal;
	TMap<FName, FKey> SavedKeys;
	TMap<FName, FKey> DraftKeys;
	TMap<FName, FKey> DefaultKeys;
	TMap<FName, FString> KeyLabels;
	TMap<FName, TObjectPtr<UKnightSettingRow>> Rows;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> RowsBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UButton> ApplyButton;
	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> ConfirmBox;
};
