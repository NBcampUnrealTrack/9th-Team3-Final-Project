#pragma once

#include "CoreMinimal.h"
#include "KnightMenuWidgetBase.h"
#include "KnightMainMenuWidget.generated.h"

class UCommonButtonBase;
class UImage;
class UPanelWidget;
class UTextBlock;
class UKnightStageGlobeWidget;
class UKnightSettingsWidget;
class UKnightCodexWidget;
struct FStreamableHandle;

UENUM(BlueprintType)
enum class EKnightMainMenuPage : uint8
{
	Home,
	Stage,
	Maintenance,
	Settings,
	Codex
};

/**
 * C++ parent for the main-menu root WBP. The stage page is a globe: clicking a region (or Left/Right) selects it,
 * the info panel moves to the region's side, and the launch button unlocks when the stage can be played.
 */
UCLASS()
class KNIGHT_API UKnightMainMenuWidget : public UKnightMenuWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Knight|Menu")
	EKnightMainMenuPage GetCurrentPage() const { return CurrentPage; }

	UFUNCTION(BlueprintCallable, Category="Knight|Menu")
	bool ShowPage(EKnightMainMenuPage NewPage);

	/** Shared entry point for globe clicks, keyboard cycling and tests. NAME_None clears the selection. */
	UFUNCTION(BlueprintCallable, Category="Knight|Stage")
	bool SelectStage(FName StageId);

	UFUNCTION(BlueprintPure, Category="Knight|Stage")
	FName GetSelectedStageId() const { return SelectedStageId; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual EKnightFlowState GetExpectedFlowState() const override { return EKnightFlowState::MainMenu; }
	virtual bool HandleBackRequested() override;
	virtual bool ShouldAllowEscapeToChild() const override;
	virtual UWidget* GetPreferredFocusTarget() const override;
	virtual void SetTravelControlsEnabled(bool bEnabled) override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> Panel_Home;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> Panel_Stage;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UKnightStageGlobeWidget> Globe_Stages;

	/** Info panel next to the globe. Must sit directly in a CanvasPanel: C++ moves it to the selected region's side. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UWidget> Panel_StageInfo;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_StageTitle;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_StageDescription;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_StageRegion;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_StageTarget;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_StageTimeLimit;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_StageRecord;

	/** Why the launch button is locked; hidden when the stage can be played. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_StageLock;

	/** Hidden when the stage has no thumbnail. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> Img_StageThumbnail;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UCommonButtonBase> Button_LaunchStage;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> Panel_Maintenance;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> Panel_Settings;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> Panel_Codex;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UCommonButtonBase> Button_Stage;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_ActiveProfile;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UCommonButtonBase> Button_ReturnToTitle;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UCommonButtonBase> Button_Maintenance;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UCommonButtonBase> Button_Settings;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UCommonButtonBase> Button_Codex;

	/** Gap between the globe's edge and the info panel. Slate units. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Stage", meta=(ClampMin=0))
	float StageInfoGap = 64.f;

	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleStageClicked();

	UFUNCTION()
	void HandleReturnToTitleClicked();

	UFUNCTION()
	void HandleMaintenanceClicked();

	UFUNCTION()
	void HandleSettingsClicked();
	UFUNCTION() void HandleSettingsClosed();

	UFUNCTION()
	void HandleCodexClicked();
	UFUNCTION() void HandleCodexClosed();

	UFUNCTION()
	void HandleGlobeStageClicked(FName StageId);

	UFUNCTION()
	void HandleLaunchStageClicked();

	bool ApplyPage(EKnightMainMenuPage NewPage, bool bAllowDuringTravel);
	bool RebuildGlobeMarkers();
	void RefreshStageDetails();
	void RefreshRaidDetails();
	void PlaceStageInfo();
	bool CycleStage(int32 Direction);
	void EnsureSettingsWidget();
	void EnsureCodexWidget();

	UPROPERTY(Transient)
	TObjectPtr<UKnightSettingsWidget> SettingsWidget;
	UPROPERTY(Transient)
	TObjectPtr<UKnightCodexWidget> CodexWidget;
	bool bRestoreCodexFocus = false;

	EKnightMainMenuPage CurrentPage = EKnightMainMenuPage::Home;
	TArray<FName> StageOrder;
	FName SelectedStageId;
	bool bStageLaunchAvailable = false;
	/** Panel stays on one side while the player drags; it only moves when a region is selected. */
	bool bStageInfoOnRight = true;

	/** Raid rules load in the background: the target monster's definition can pull in large assets. */
	TSharedPtr<FStreamableHandle> RaidDefinitionHandle;
};
