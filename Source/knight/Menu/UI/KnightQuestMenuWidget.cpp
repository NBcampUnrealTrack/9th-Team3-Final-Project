#include "Menu/UI/KnightQuestMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Codex/KnightCodexData.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFlow/KnightGameFlowSubsystem.h"
#include "Monster/knightMonsterDefinition.h"
#include "Quest/KnightQuestData.h"
#include "Progression/knightProgressionSubsystem.h"
#include "knightItemDefinition.h"
#include "knight.h"
#include "Menu/UI/KnightOperationMapPresentation.h"
#include "Menu/UI/KnightButtonFillExtension.h"

#define LOCTEXT_NAMESPACE "KnightQuestMenu"

namespace
{
	const FLinearColor QuestPanelColor(FColor(15, 25, 35, 238));
	const FLinearColor QuestMutedColor(FColor(160, 177, 189));

	UCommonTextBlock* NewQuestText(UWidgetTree* Tree, const TCHAR* Style)
	{
		UCommonTextBlock* const Text = Tree->ConstructWidget<UCommonTextBlock>();
		if (const TSubclassOf<UCommonTextStyle> StyleClass = LoadClass<UCommonTextStyle>(nullptr,
			*FString::Printf(TEXT("/Game/Knight/UI/Style/%s.%s_C"), Style, Style)))
		{
			Text->SetStyle(StyleClass);
		}
		Text->SetAutoWrapText(true);
		return Text;
	}

	UCommonButtonBase* NewQuestButton(APlayerController* Owner, bool bSelectable = false)
	{
		const TSubclassOf<UCommonButtonBase> ButtonClass = LoadClass<UCommonButtonBase>(nullptr,
			TEXT("/Game/Knight/UI/Common/WBP_KnightButton.WBP_KnightButton_C"));
		UCommonButtonBase* const Button = Owner && ButtonClass ? CreateWidget<UCommonButtonBase>(Owner, ButtonClass) : nullptr;
		if (Button)
		{
			UKnightButtonFillExtension::SetEnabled(Button, false);
			if (UFunction* Function = Button->FindFunction(TEXT("ConfigureOperationMapButton")))
			{
				struct { bool bInSelectable; } Params { bSelectable };
				Button->ProcessEvent(Function, &Params);
			}
		}
		return Button;
	}

	FText RewardCount(const FknightRaidReward& Reward)
	{
		return Reward.MinCount == Reward.MaxCount ? FText::AsNumber(Reward.MinCount)
			: FText::Format(LOCTEXT("CountRange", "{0}~{1}"), FText::AsNumber(Reward.MinCount), FText::AsNumber(Reward.MaxCount));
	}
}

void UKnightQuestMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildLayout();
}

void UKnightQuestMenuWidget::BuildLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget) return;
	UBorder* const Root = WidgetTree->ConstructWidget<UBorder>();
	Root->SetBrushColor(QuestPanelColor);
	Root->SetPadding(FMargin(18, 16));
	WidgetTree->RootWidget = Root;
	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>();
	ContentColumn = Column;
	Root->SetContent(Column);
	RegionHeading = NewQuestText(WidgetTree, TEXT("TS_Knight_Subhead"));
	RegionHeading->SetText(LOCTEXT("ChooseRegion", "지도에서 구역을 선택하세요"));
	Column->AddChildToVerticalBox(RegionHeading)->SetPadding(FMargin(0, 0, 0, 12));
	USizeBox* const ListHeight = WidgetTree->ConstructWidget<USizeBox>();
	ListHeight->SetMaxDesiredHeight(160.f);
	Column->AddChildToVerticalBox(ListHeight)->SetPadding(FMargin(0, 0, 0, 10));
	QuestList = WidgetTree->ConstructWidget<UScrollBox>();
	ListHeight->AddChild(QuestList);

	DetailBorder = WidgetTree->ConstructWidget<UBorder>();
	DetailBorder->SetBrushColor(QuestPanelColor);
	DetailBorder->SetPadding(FMargin(0));
	Column->AddChildToVerticalBox(DetailBorder)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UVerticalBox* const DetailColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	DetailBorder->SetContent(DetailColumn);
	UScrollBox* const DetailScroll = WidgetTree->ConstructWidget<UScrollBox>();
	DetailColumn->AddChildToVerticalBox(DetailScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UVerticalBox* const DetailTextColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	DetailScroll->AddChild(DetailTextColumn);
	DetailTitle = NewQuestText(WidgetTree, TEXT("TS_Knight_Subhead"));
	DetailDescription = NewQuestText(WidgetTree, TEXT("TS_Knight_Body"));
	DetailTarget = NewQuestText(WidgetTree, TEXT("TS_Knight_Body"));
	DetailRewards = NewQuestText(WidgetTree, TEXT("TS_Knight_Body"));
	DetailAvailability = NewQuestText(WidgetTree, TEXT("TS_Knight_Caption"));
	DetailTextColumn->AddChildToVerticalBox(DetailTitle)->SetPadding(FMargin(0, 0, 0, 14));
	DetailTextColumn->AddChildToVerticalBox(DetailDescription)->SetPadding(FMargin(0, 0, 0, 15));
	DetailTextColumn->AddChildToVerticalBox(DetailTarget)->SetPadding(FMargin(0, 0, 0, 15));
	DetailTextColumn->AddChildToVerticalBox(DetailRewards)->SetPadding(FMargin(0, 0, 0, 10));
	DetailColumn->AddChildToVerticalBox(DetailAvailability)->SetPadding(FMargin(0, 10, 0, 10));
	LaunchButton = NewQuestButton(GetOwningPlayer());
	if (LaunchButton)
	{
		SetButtonLabel(LaunchButton, LOCTEXT("Launch", "출격"));
		DetailColumn->AddChildToVerticalBox(LaunchButton);
		LaunchButton->OnClicked().AddUObject(this, &ThisClass::HandleLaunchClicked);
	}
	DetailBorder->SetVisibility(ESlateVisibility::Collapsed);
}

void UKnightQuestMenuWidget::SetButtonLabel(UCommonButtonBase* Button, const FText& Label)
{
	UFunction* const Function = Button ? Button->FindFunction(TEXT("SetButtonText")) : nullptr;
	if (!Function) return;
	struct { FText InText; } Params{ Label };
	Button->ProcessEvent(Function, &Params);
}

FText UKnightQuestMenuWidget::GetAvailability(const UKnightQuestDefinition* Quest, bool& bCanLaunch) const
{
	const auto State = KnightOperationMapPresentation::Query(Catalog, Quest, GetGameInstance());
	bCanLaunch = State.bCanLaunch;
	TArray<FText> Lines { State.Availability };
	if (!State.ClearLabel.IsEmpty()) Lines.Add(FText::Format(LOCTEXT("ClearMark", "✓ {0}"), State.ClearLabel));
	if (!bCanLaunch && !State.Reason.IsEmpty()) Lines.Add(State.Reason);
	return FText::Join(FText::FromString(TEXT(" · ")), Lines);
}

void UKnightQuestMenuWidget::RefreshQuests()
{
	Catalog = UKnightQuestSettings::LoadCatalog();
	if (!bPanelChanging) RebuildRegionContent();
}

void UKnightQuestMenuWidget::SetRegionQuests(FName RegionId, const FText& RegionName, const TArray<FName>& QuestIds, bool bAnimate)
{
	const bool bChanged = ActiveRegionId != RegionId;
	ActiveRegionId = RegionId;
	ActiveRegionName = RegionName;
	RegionQuestIds = QuestIds;
	if (!RegionQuestIds.Contains(SelectedQuestId)) SelectedQuestId = NAME_None;
	if (bChanged && bAnimate && ContentColumn)
	{
		bPanelChanging = bFadingOut = true; FadeElapsed = 0.f;
		FadeStartOpacity = ContentColumn->GetRenderOpacity();
		ContentColumn->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (LaunchButton) LaunchButton->SetIsEnabled(false);
		return;
	}
	if (!bPanelChanging || !bAnimate) FinishPanelChange();
}

void UKnightQuestMenuWidget::FinishPanelChange()
{
	bPanelChanging = bFadingOut = false; FadeElapsed = 0.f;
	if (ContentColumn) { ContentColumn->SetRenderOpacity(1.f); ContentColumn->SetVisibility(ESlateVisibility::SelfHitTestInvisible); }
	RebuildRegionContent();
}

void UKnightQuestMenuWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	if (!bPanelChanging || !ContentColumn) return;
	FadeElapsed += FMath::Max(0.f, DeltaTime);
	const float OutDuration = PanelFadeDuration / 3.f;
	if (bFadingOut && FadeElapsed >= OutDuration)
	{
		ContentColumn->SetRenderOpacity(0.f);
		bFadingOut = false;
		RebuildRegionContent();
		ContentColumn->ForceLayoutPrepass();
	}
	if (bFadingOut)
		ContentColumn->SetRenderOpacity(FadeStartOpacity * (1.f - FMath::SmoothStep(0.f, OutDuration, FadeElapsed)));
	else
		ContentColumn->SetRenderOpacity(FMath::SmoothStep(OutDuration, PanelFadeDuration, FadeElapsed));
	if (FadeElapsed >= PanelFadeDuration)
	{
		bPanelChanging = false;
		ContentColumn->SetRenderOpacity(1.f); ContentColumn->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		RefreshDetails();
	}
}

void UKnightQuestMenuWidget::NativeDestruct()
{
	bPanelChanging = bFadingOut = false;
	if (ContentColumn) { ContentColumn->SetRenderOpacity(1.f); ContentColumn->SetVisibility(ESlateVisibility::SelfHitTestInvisible); }
	Super::NativeDestruct();
}

void UKnightQuestMenuWidget::RebuildRegionContent()
{
	const bool bHadFocus = HasFocusedDescendants();
	const float Scroll = QuestList ? QuestList->GetScrollOffset() : 0.f;
	QuestOrder.Reset();
	QuestButtons.Reset();
	if (QuestList) QuestList->ClearChildren();
	if (RegionHeading)
	{
		RegionHeading->SetText(ActiveRegionId.IsNone() ? LOCTEXT("ChooseRegion", "지도에서 구역을 선택하세요")
			: ActiveRegionName.IsEmpty() ? FText::FromName(ActiveRegionId) : ActiveRegionName);
	}
	if (!Catalog || !QuestList || ActiveRegionId.IsNone())
	{
		SelectedQuestId = NAME_None;
		RefreshDetails();
		if (!Catalog) UE_LOG(Logknight, Error, TEXT("Quest menu cannot load QuestCatalog."));
		return;
	}
	for (const FName QuestId : RegionQuestIds)
	{
		UKnightQuestDefinition* const Quest = Catalog->FindQuest(QuestId);
		if (!Quest) continue;
		UCommonButtonBase* const Button = NewQuestButton(GetOwningPlayer(), true);
		if (!Button) continue;
		SetButtonLabel(Button, Quest->DisplayName);
		Button->OnClicked().AddWeakLambda(this, [this, QuestId]() { SelectQuest(QuestId); });
		UVerticalBox* const Row = WidgetTree->ConstructWidget<UVerticalBox>();
		Row->AddChildToVerticalBox(Button);
		bool bCanLaunch = false;
		UCommonTextBlock* const StateText = NewQuestText(WidgetTree, TEXT("TS_Knight_Caption"));
		StateText->SetText(GetAvailability(Quest, bCanLaunch));
		if (!bCanLaunch) StateText->SetColorAndOpacity(FSlateColor(QuestMutedColor));
		Row->AddChildToVerticalBox(StateText)->SetPadding(FMargin(12, 0, 0, 8));
		QuestList->AddChild(Row);
		QuestButtons.Add(Button);
		QuestOrder.Add(QuestId);
		if (!Quest->bContentEnabled)
		{
			FText DataReason;
			Quest->HasValidConfiguration(DataReason);
			UE_LOG(Logknight, Log, TEXT("Quest %s is not available: %s (Stage=%s Boss=%s HardClass=%s)"),
				*QuestId.ToString(), *DataReason.ToString(), *Quest->StageId.ToString(),
				*Quest->BossId.ToString(), *Quest->HardBossClass.ToString());
		}
	}
	if (!QuestOrder.Contains(SelectedQuestId)) SelectedQuestId = NAME_None;
	if (QuestList) QuestList->SetScrollOffset(Scroll);
	RefreshDetails();
	if (bHadFocus && !bPanelChanging) if (UWidget* Focus = GetFocusButton()) Focus->SetUserFocus(GetOwningPlayer());
}

bool UKnightQuestMenuWidget::SelectQuest(FName QuestId)
{
	if (!QuestId.IsNone() && (!Catalog || !RegionQuestIds.Contains(QuestId) || !Catalog->FindQuest(QuestId))) return false;
	const bool bChanged = SelectedQuestId != QuestId;
	SelectedQuestId = QuestId;
	if (!bPanelChanging || !bFadingOut) RefreshDetails();
	if (bChanged) OnSelectionChanged.Broadcast(QuestId);
	return true;
}

bool UKnightQuestMenuWidget::CycleQuest(int32 Direction)
{
	if (RegionQuestIds.IsEmpty()) return false;
	const int32 Current = RegionQuestIds.IndexOfByKey(SelectedQuestId);
	const int32 Next = Current == INDEX_NONE ? (Direction >= 0 ? 0 : RegionQuestIds.Num() - 1)
		: (Current + (Direction >= 0 ? 1 : -1) + RegionQuestIds.Num()) % RegionQuestIds.Num();
	SelectQuest(RegionQuestIds[Next]);
	if (UWidget* const Button = GetFocusButton()) Button->SetUserFocus(GetOwningPlayer());
	return true;
}

UWidget* UKnightQuestMenuWidget::GetFocusButton() const
{
	if (bPanelChanging) return nullptr;
	const int32 Index = QuestOrder.IndexOfByKey(SelectedQuestId);
	return QuestButtons.IsValidIndex(Index) ? QuestButtons[Index].Get()
		: (QuestButtons.IsEmpty() ? nullptr : QuestButtons[0].Get());
}

FText UKnightQuestMenuWidget::FormatReward(const FknightRaidReward& Reward)
{
	const FText Name = Reward.Item && !Reward.Item->DisplayName.IsEmpty()
		? Reward.Item->DisplayName : LOCTEXT("UnknownItem", "미지정 아이템");
	const FText Chance = Reward.Chance >= 1.f - KINDA_SMALL_NUMBER
		? LOCTEXT("Guaranteed", "확정") : FText::AsPercent(Reward.Chance);
	return FText::Format(LOCTEXT("RewardLine", "{0} ×{1} · {2}"), Name, RewardCount(Reward), Chance);
}

void UKnightQuestMenuWidget::RefreshDetails()
{
	const bool bSelectionChanged = DisplayedQuestId != SelectedQuestId;
	DisplayedQuestId = SelectedQuestId;
	const UKnightQuestDefinition* const Quest = Catalog ? Catalog->FindQuest(SelectedQuestId) : nullptr;
	for (int32 Index = 0; Index < QuestButtons.Num(); ++Index)
	{
		UCommonButtonBase* const Button = QuestButtons[Index];
		if (!Button) continue;
		const bool bSelected = QuestOrder.IsValidIndex(Index) && QuestOrder[Index] == SelectedQuestId;
		if (bSelected && !Button->GetSelected()) Button->SetIsSelected(true, false);
		else if (!bSelected && Button->GetSelected()) Button->ClearSelection();
		if (bSelected && bSelectionChanged && QuestList) QuestList->ScrollWidgetIntoView(Button->GetParent(), false);
	}
	if (!Quest)
	{
		if (DetailBorder) DetailBorder->SetVisibility(ESlateVisibility::Collapsed);
		if (DetailTitle) DetailTitle->SetText(LOCTEXT("SelectQuest", "퀘스트를 선택하세요"));
		if (DetailDescription) DetailDescription->SetText(FText::GetEmpty());
		if (DetailTarget) DetailTarget->SetText(FText::GetEmpty());
		if (DetailRewards) DetailRewards->SetText(FText::GetEmpty());
		if (DetailAvailability) DetailAvailability->SetText(FText::GetEmpty());
		if (LaunchButton) LaunchButton->SetIsEnabled(false);
		return;
	}
	if (DetailBorder) DetailBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (DetailTitle) DetailTitle->SetText(Quest->DisplayName);
	if (DetailDescription) DetailDescription->SetText(Quest->Description);
	if (LaunchButton) SetButtonLabel(LaunchButton, Quest->bMapExplorationOnly
		? LOCTEXT("ExploreMap", "맵 탐색") : LOCTEXT("Launch", "출격"));
	if (Quest->bMapExplorationOnly)
	{
		if (DetailTarget) DetailTarget->SetVisibility(ESlateVisibility::Collapsed);
		if (DetailRewards) DetailRewards->SetVisibility(ESlateVisibility::Collapsed);
		bool bCanLaunch = false;
		const FText Availability = GetAvailability(Quest, bCanLaunch);
		if (DetailAvailability) DetailAvailability->SetText(Availability);
		const UKnightGameFlowSubsystem* const Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>() : nullptr;
		if (LaunchButton) LaunchButton->SetIsEnabled(!bPanelChanging && bCanLaunch && Flow && !Flow->IsTravelPending());
		return;
	}
	if (DetailTarget) DetailTarget->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (DetailRewards) DetailRewards->SetVisibility(ESlateVisibility::HitTestInvisible);
	FText BossName = FText::FromName(Quest->BossId);
	if (const UKnightCodexData* const Codex = Catalog->CodexData.LoadSynchronous())
	{
		if (const FKnightCodexBossEntry* const Boss = Codex->Bosses.FindByPredicate(
			[Quest](const FKnightCodexBossEntry& Entry) { return Entry.BossId == Quest->BossId; }))
		{
			if (!Boss->DisplayNameOverride.IsEmpty()) BossName = Boss->DisplayNameOverride;
			else if (Boss->MonsterDefinition && !Boss->MonsterDefinition->DisplayName.IsEmpty())
				BossName = Boss->MonsterDefinition->DisplayName;
		}
	}
	if (DetailTarget)
	{
		DetailTarget->SetText(FText::Format(LOCTEXT("TargetLine", "대상: {0}   ·   난이도: {1}"), BossName,
			Quest->Difficulty == EKnightQuestDifficulty::Hard ? LOCTEXT("Hard", "어려움") : LOCTEXT("Normal", "일반")));
	}
	TArray<FText> Lines;
	Lines.Add(FText::Format(LOCTEXT("Money", "클리어 돈: {0}"), FText::AsNumber(Quest->MoneyReward)));
	Lines.Add(LOCTEXT("ClearRewardHeading", "클리어 보상"));
	for (const FknightRaidReward& Reward : Quest->ClearRewards) Lines.Add(FormatReward(Reward));
	Lines.Add(LOCTEXT("PartRewardHeading", "부위 파괴 후 클리어 보상"));
	for (const FKnightQuestPartBreakReward& Entry : Quest->PartBreakRewards)
	{
		const FText Part = Entry.Part == EKnightQuestBreakPart::Head ? LOCTEXT("Head", "머리")
			: Entry.Part == EKnightQuestBreakPart::Tail ? LOCTEXT("Tail", "꼬리") : LOCTEXT("Other", "기타");
		Lines.Add(FText::Format(LOCTEXT("PartRewardLine", "{0}: {1}"), Part, FormatReward(Entry.Reward)));
	}
	if (Quest->FirstBossSocketSlots > 0)
	{
		Lines.Add(FText::Format(LOCTEXT("FirstBossSocket", "해당 보스 최초 토벌: 공용 소켓 +{0}칸"),
			FText::AsNumber(Quest->FirstBossSocketSlots)));
	}
	if (DetailRewards) DetailRewards->SetText(FText::Join(FText::FromString(TEXT("\n")), Lines));
	bool bCanLaunch = false;
	const FText Availability = GetAvailability(Quest, bCanLaunch);
	if (DetailAvailability) DetailAvailability->SetText(Availability);
	const UKnightGameFlowSubsystem* const Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>() : nullptr;
	if (LaunchButton) LaunchButton->SetIsEnabled(!bPanelChanging && bCanLaunch && Flow && !Flow->IsTravelPending());
}

void UKnightQuestMenuWidget::HandleLaunchClicked()
{
	const auto State = KnightOperationMapPresentation::Query(Catalog, Catalog ? Catalog->FindQuest(SelectedQuestId) : nullptr, GetGameInstance());
	const auto* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>() : nullptr;
	if (!bPanelChanging && DisplayedQuestId == SelectedQuestId && RegionQuestIds.Contains(SelectedQuestId)
		&& State.bCanLaunch && Flow && !Flow->IsTravelPending() && LaunchButton && LaunchButton->GetIsEnabled())
	{
		OnLaunchRequested.Broadcast(SelectedQuestId);
	}
}

#undef LOCTEXT_NAMESPACE
