#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Border.h"
#include "KnightQuestMenuWidget.generated.h"

class UCommonButtonBase;
class UCommonTextBlock;
class UScrollBox;
class UImage;
class UKnightQuestCatalog;
class UKnightQuestDefinition;
class UVerticalBox;

DECLARE_MULTICAST_DELEGATE_OneParam(FKnightQuestLaunchRequested, FName);
DECLARE_MULTICAST_DELEGATE_OneParam(FKnightQuestSelectionChanged, FName);

/** Decorative cut-corner frame; children alone provide interaction. */
UCLASS()
class KNIGHT_API UKnightQuestPanelFrame : public UBorder
{
    GENERATED_BODY()
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
};

/** Content of the existing main-menu stage page. No raid state changes while browsing. */
UCLASS()
class KNIGHT_API UKnightQuestMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void RefreshQuests();
	/** Bind the valid map choices for this region; only the map-selected quest is displayed. */
	void SetRegionQuests(FName RegionId, const FText& RegionName, const TArray<FName>& QuestIds, bool bAnimate = true);
	bool IsPanelChanging() const { return bPanelChanging; }
	void FinishPanelChange();
	bool SelectQuest(FName QuestId);
	FName GetSelectedQuestId() const { return SelectedQuestId; }
	UWidget* GetFocusButton() const;
	FKnightQuestLaunchRequested OnLaunchRequested;
	FKnightQuestSelectionChanged OnSelectionChanged;

protected:
	friend class UknightOperationMapCheckCommandlet;
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual void NativeDestruct() override;
	UPROPERTY(EditAnywhere, Category="Knight|Quest|Presentation", meta=(ClampMin="0.15", ClampMax="0.20"))
	float PanelFadeDuration = .18f;

private:
	void BuildLayout();
	void RebuildRegionContent();
	void RefreshDetails();
	void HandleLaunchClicked();
	FText GetAvailability(const UKnightQuestDefinition* Quest, bool& bCanLaunch) const;
	void AddRewardRow(const struct FknightRaidReward& Reward, const FText& Condition);
	void AddSection(const FText& Label);
	static void SetButtonLabel(UCommonButtonBase* Button, const FText& Label);

	UPROPERTY(Transient)
	TObjectPtr<const UKnightQuestCatalog> Catalog;
	UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> RegionHeading;
	UPROPERTY(Transient) TObjectPtr<UBorder> DetailBorder;
	UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> DetailTitle;
	UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> DifficultyBadge;
	UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> DetailAvailability;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> RewardScroll;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> RewardRows;
	UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> CurrencyText;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> RewardIcons;
	UPROPERTY(Transient) TArray<TObjectPtr<UCommonTextBlock>> RewardValues;
	TSet<FString> ReportedMissingIcons;
	UPROPERTY(Transient)
	TObjectPtr<UCommonButtonBase> LaunchButton;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ContentColumn;
	TArray<FName> RegionQuestIds;
	FName ActiveRegionId;
	FText ActiveRegionName;
	FName SelectedQuestId;
	FName DisplayedQuestId;
	bool bPanelChanging = false;
	bool bFadingOut = false;
	float FadeElapsed = 0.f;
	float FadeStartOpacity = 1.f;
};
