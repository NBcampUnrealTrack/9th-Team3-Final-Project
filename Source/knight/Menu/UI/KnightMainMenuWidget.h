#pragma once

#include "CoreMinimal.h"
#include "KnightMenuWidgetBase.h"
#include "KnightMainMenuWidget.generated.h"

class UButton;
class UPanelWidget;
class UTextBlock;
class UVerticalBox;
class UKnightStageEntryWidget;

UENUM(BlueprintType)
enum class EKnightMainMenuPage : uint8
{
	Home,
	Stage,
	StageDetails,
	Maintenance,
	Settings,
	Codex
};

/** C++ parent for the main-menu root WBP. Stage detail/back behavior extends here in step 3. */
UCLASS()
class KNIGHT_API UKnightMainMenuWidget : public UKnightMenuWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Knight|Menu")
	EKnightMainMenuPage GetCurrentPage() const { return CurrentPage; }

	UFUNCTION(BlueprintCallable, Category="Knight|Menu")
	bool ShowPage(EKnightMainMenuPage NewPage);

	/** Shared entry point for list items and future globe markers. */
	UFUNCTION(BlueprintCallable, Category="Knight|Stage")
	bool SelectStage(FName StageId);

	UFUNCTION(BlueprintCallable, Category="Knight|Stage")
	bool RebuildStageEntries();

	UFUNCTION(BlueprintPure, Category="Knight|Stage")
	FName GetSelectedStageId() const { return SelectedStageId; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual EKnightFlowState GetExpectedFlowState() const override { return EKnightFlowState::MainMenu; }
	virtual bool HandleBackRequested() override;
	virtual UWidget* GetPreferredFocusTarget() const override;
	virtual void SetTravelControlsEnabled(bool bEnabled) override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> Panel_Home;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> Panel_Stage;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> Panel_StageDetails;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UVerticalBox> Box_StageEntries;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_StageTitle;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_StageDescription;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_LaunchStage;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> Panel_Maintenance;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> Panel_Settings;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> Panel_Codex;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Stage;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_ActiveProfile;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_ChangeProfile;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Maintenance;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Settings;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Codex;

	/** Set to a WBP derived from UKnightStageEntryWidget on the main-menu WBP defaults. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Knight|Stage")
	TSubclassOf<UKnightStageEntryWidget> StageEntryWidgetClass;

	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleStageClicked();

	UFUNCTION()
	void HandleChangeProfileClicked();

	UFUNCTION()
	void HandleMaintenanceClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleCodexClicked();

	UFUNCTION()
	void HandleStageEntrySelected(FName StageId);

	UFUNCTION()
	void HandleLaunchStageClicked();

	bool ApplyPage(EKnightMainMenuPage NewPage, bool bAllowDuringTravel);
	bool RefreshStageDetails();
	void RemoveStageEntries();

	EKnightMainMenuPage CurrentPage = EKnightMainMenuPage::Home;
	FName SelectedStageId;
	bool bStageEntriesBuilt = false;
	bool bStageLaunchAvailable = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UKnightStageEntryWidget>> GeneratedStageEntries;
};
