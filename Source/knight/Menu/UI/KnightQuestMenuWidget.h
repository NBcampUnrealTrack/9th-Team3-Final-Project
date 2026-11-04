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

DECLARE_MULTICAST_DELEGATE_OneParam(FKnightQuestLaunchRequested, FName);
DECLARE_MULTICAST_DELEGATE_OneParam(FKnightQuestSelectionChanged, FName);

/** Content of the existing main-menu stage page. No raid state changes while browsing. */
UCLASS()
class KNIGHT_API UKnightQuestMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void RefreshQuests();
	void ResetButtonFills();
	/** Show only the quests on the selected map region. Does not change raid state. */
	void SetRegionQuests(FName RegionId, const FText& RegionName, const TArray<FName>& QuestIds);
	bool SelectQuest(FName QuestId);
	bool CycleQuest(int32 Direction);
	FName GetSelectedQuestId() const { return SelectedQuestId; }
	UWidget* GetFocusButton() const;
	FKnightQuestLaunchRequested OnLaunchRequested;
	FKnightQuestSelectionChanged OnSelectionChanged;

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildLayout();
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
	TArray<TObjectPtr<UCommonButtonBase>> QuestButtons;
	TArray<FName> QuestOrder;
	TArray<FName> RegionQuestIds;
	FName ActiveRegionId;
	FText ActiveRegionName;
	FName SelectedQuestId;
};
