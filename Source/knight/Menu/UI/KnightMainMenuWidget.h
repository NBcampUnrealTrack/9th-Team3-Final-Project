#pragma once

#include "CoreMinimal.h"
#include "KnightMenuWidgetBase.h"
#include "KnightOperationMapTransition.h"
#include "Codex/KnightCodexTransition.h"
#include "KnightMainMenuWidget.generated.h"

class UCommonButtonBase;
class UImage;
class UPanelWidget;
class UTextBlock;
class UKnightStageMapWidget;
class UKnightSettingsWidget;
class UKnightCodexWidget;
class UKnightMaintenanceWidget;
class UKnightQuestMenuWidget;
class UKnightQuestCatalog;
class UCanvasPanel;
class UBorder;
class UHorizontalBox;
class UCommonButtonStyle;
class AKnightMaintenanceCameraRig;
enum class EKnightMaintenanceDestination : uint8;
struct FStreamableHandle;

UENUM(BlueprintType)
enum class EKnightMainMenuPage : uint8
{
	Home,
	Stage,
	Maintenance,
	Settings,
	Codex,
	Inventory = 5 UMETA(Hidden) // Serialized legacy value; ShowPage redirects it to Crafting > Materials.
};

/**
 * C++ parent for the main-menu root WBP. The stage page shows QuestCatalog entries and retains the map as a
 * fallback if the quest view cannot be created.
 */
UCLASS()
class KNIGHT_API UKnightMainMenuWidget : public UKnightMenuWidgetBase
{
	GENERATED_BODY()
	friend class UknightEquipmentActionCheckCommandlet;
	friend class UknightMaintenanceEntranceCheckCommandlet;
	friend class UknightOperationMapCheckCommandlet;
	friend class UKnightOperationMapTransition;
	friend class UKnightCodexTransition;
	friend class UKnightCodexCheckCommandlet;

public:
	UFUNCTION(BlueprintPure, Category="Knight|Menu")
	EKnightMainMenuPage GetCurrentPage() const { return CurrentPage; }

	UFUNCTION(BlueprintCallable, Category="Knight|Menu")
	bool ShowPage(EKnightMainMenuPage NewPage);
	/** Main-menu I shortcut; also callable by headless integration checks. */
	bool ToggleMaterialsShortcut();

	/** Shared entry point for map clicks, keyboard cycling and tests. NAME_None clears the selection. */
	UFUNCTION(BlueprintCallable, Category="Knight|Stage")
	bool SelectStage(FName StageId);

	UFUNCTION(BlueprintPure, Category="Knight|Stage")
	FName GetSelectedStageId() const { return SelectedStageId; }

	UFUNCTION(BlueprintCallable, Category="Knight|Quest")
	bool SelectQuest(FName QuestId);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool NativeSupportsCustomNavigation() const override;
	virtual FNavigationReply NativeOnNavigation(const FGeometry& Geometry, const FNavigationEvent& Event, const FNavigationReply& DefaultReply) override;
	virtual EKnightFlowState GetExpectedFlowState() const override { return EKnightFlowState::MainMenu; }
	virtual bool HandleBackRequested() override;
	virtual bool ShouldAllowEscapeToChild() const override;
	virtual UWidget* GetPreferredFocusTarget() const override;
	virtual void SetTravelControlsEnabled(bool bEnabled) override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> Panel_Home;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> Panel_Stage;

	/** Hit-test-transparent host for the map-side quest details in WBP_KnightMainMenu. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UCanvasPanel> Panel_QuestHost;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UKnightStageMapWidget> Map_Stages;

	/** Info panel to the right of the map. Must sit directly in a CanvasPanel: C++ places it against the map's edge. */
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

	/** Gap between the map's edge and the info panel. Slate units. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Stage", meta=(ClampMin=0))
	float StageInfoGap = 64.f;
	UPROPERTY(EditDefaultsOnly, Category="Knight|Stage|Transition")
	FKnightOperationMapTransitionSettings OperationMapTransitionSettings;
	UPROPERTY(EditDefaultsOnly, Category="Knight|Codex|Transition")
	FKnightCodexTransitionSettings CodexTransitionSettings;

	virtual void NativeDestruct() override;

private:
	UFUNCTION() void HandleQuestProgressChanged();
	UFUNCTION() void HandleQuestProfileChanged(const FString& SlotId);

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
	void HandleMapStageClicked(FName StageId);
	UFUNCTION()
	void HandleMapQuestClicked(FName QuestId);
	void HandleQuestSelectionChanged(FName QuestId);

	UFUNCTION()
	void HandleLaunchStageClicked();
	void HandleQuestLaunchRequested(FName QuestId);

	bool ApplyPage(EKnightMainMenuPage NewPage, bool bAllowDuringTravel);
	bool OpenMaterialsShortcut();
	bool RebuildMapMarkers();
	void ShowSelectedRegionQuests(bool bAnimate = true);
	void SaveOperationMapContext();
	void RestoreOperationMapContext();
	FString OperationMapProfile;
	void PlaceQuestPanel();
	void RefreshStageDetails();
	void RefreshRaidDetails();
	void PlaceStageInfo();
	bool CycleStage(int32 Direction);
	void EnsureSettingsWidget();
	void EnsureCodexWidget();
	void EnsureMaintenanceWidget();
	void BuildMaintenanceFooterBack();
	void ApplyMaintenanceBackPresentation(bool bMaintenance);
	void ShowMaintenanceSelection();
	void SetMaintenanceSubmenuOpen(bool bOpen);
	void PlaceMaintenanceSubmenu();
	void OpenMaintenanceDestination(EKnightMaintenanceDestination Destination, bool bStartInMaterials = false);
	bool IsMaintenanceDestinationOpen() const;
	void LeaveMaintenance();
	TSet<FKey> ReturnSuppressedKeys;
	void EnsureQuestWidget();
	bool IsOperationMapTransitionActive() const;
	void CompleteOperationMapTransition(bool bExited);
	UPROPERTY(Transient) TObjectPtr<UKnightOperationMapTransition> OperationMapTransition;
	bool IsCodexTransitionActive() const;
	void CompleteCodexTransition(bool bExited);
	UPROPERTY(Transient) TObjectPtr<UKnightCodexTransition> CodexTransition;

	UPROPERTY(Transient)
	TObjectPtr<UKnightSettingsWidget> SettingsWidget;
	UPROPERTY(Transient)
	TObjectPtr<UKnightCodexWidget> CodexWidget;
	UPROPERTY(Transient)
	TObjectPtr<UKnightMaintenanceWidget> MaintenanceWidget;
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> MaintenanceBackContent;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> NormalBackContent;
	UPROPERTY(Transient)
	TSubclassOf<UCommonButtonStyle> NormalBackStyle;
	FVector2D NormalBackPosition = FVector2D::ZeroVector;
	FAnchors NormalBackAnchors;
	UPROPERTY(Transient)
	TObjectPtr<UWidgetSwitcher> MaintenanceSwitcher;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> MaintenanceSubmenu;
	UPROPERTY(Transient)
	TObjectPtr<UCommonButtonBase> MaintenanceCraftingButton;
	UPROPERTY(Transient)
	TObjectPtr<UCommonButtonBase> MaintenanceEnhancementButton;
	UPROPERTY(Transient)
	TObjectPtr<UCommonButtonBase> MaintenanceSocketsButton;
	UPROPERTY(Transient)
	TObjectPtr<UKnightQuestMenuWidget> QuestWidget;
	UPROPERTY(Transient)
	TObjectPtr<const UKnightQuestCatalog> MapQuestCatalog;
	TWeakObjectPtr<UWidget> FocusBeforeMaterialsShortcut;
	bool bRestoreMaterialsShortcutFocus = false;
	bool bMaterialsShortcutEntry = false;
	bool bReturnToWeaponEquipment = false;
	TWeakObjectPtr<AKnightMaintenanceCameraRig> MaintenanceCameraRig;
	bool bRestoreMaintenanceFocus = false;
	bool bRestoreCodexFocus = false;

	EKnightMainMenuPage CurrentPage = EKnightMainMenuPage::Home;
	TArray<FName> StageOrder;
	TMap<FName, FName> QuestRegionById;
	TMap<FName, FText> RegionNames;
	FName SelectedStageId;
	bool bStageLaunchAvailable = false;
	bool bQuestMode = false;

	/** Raid rules load in the background: the target monster's definition can pull in large assets. */
	TSharedPtr<FStreamableHandle> RaidDefinitionHandle;
};
