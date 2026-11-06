#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "KnightQuestMenuWidget.generated.h"

class UCommonButtonBase;
class UCommonTextBlock;
class UScrollBox;
class UBorder;
class UKnightQuestCatalog;
class UKnightQuestDefinition;
class UVerticalBox;

DECLARE_MULTICAST_DELEGATE_OneParam(FKnightQuestLaunchRequested, FName);
DECLARE_MULTICAST_DELEGATE_OneParam(FKnightQuestSelectionChanged, FName);

/** Content of the existing main-menu stage page. No raid state changes while browsing. */
UCLASS()
class KNIGHT_API UKnightQuestMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void RefreshQuests();
	/** Show only the quests on the selected map region. Does not change raid state. */
	void SetRegionQuests(FName RegionId, const FText& RegionName, const TArray<FName>& QuestIds, bool bAnimate = true);
	bool IsPanelChanging() const { return bPanelChanging; }
	void FinishPanelChange();
	bool SelectQuest(FName QuestId);
	bool CycleQuest(int32 Direction);
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
	static FText FormatReward(const struct FknightRaidReward& Reward);
	static void SetButtonLabel(UCommonButtonBase* Button, const FText& Label);

	UPROPERTY(Transient)
	TObjectPtr<const UKnightQuestCatalog> Catalog;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> QuestList;
	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> RegionHeading;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> DetailBorder;
	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> DetailTitle;
	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> DetailDescription;
	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> DetailTarget;
	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> DetailRewards;
	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> DetailAvailability;
	UPROPERTY(Transient)
	TObjectPtr<UCommonButtonBase> LaunchButton;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ContentColumn;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCommonButtonBase>> QuestButtons;
	TArray<FName> QuestOrder;
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
