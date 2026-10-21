#include "Menu/UI/KnightQuestMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Codex/KnightCodexData.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
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

	UCommonButtonBase* NewQuestButton(APlayerController* Owner)
	{
		static const TSubclassOf<UCommonButtonBase> ButtonClass = LoadClass<UCommonButtonBase>(nullptr,
			TEXT("/Game/Knight/UI/Common/WBP_KnightButton.WBP_KnightButton_C"));
		return Owner && ButtonClass ? CreateWidget<UCommonButtonBase>(Owner, ButtonClass) : nullptr;
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
	UHorizontalBox* const Root = WidgetTree->ConstructWidget<UHorizontalBox>();
	WidgetTree->RootWidget = Root;

	USizeBox* const ListWidth = WidgetTree->ConstructWidget<USizeBox>();
	ListWidth->SetWidthOverride(310.f);
	UHorizontalBoxSlot* const ListRootSlot = Root->AddChildToHorizontalBox(ListWidth);
	ListRootSlot->SetPadding(FMargin(0, 0, 18, 0));
	ListRootSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	UBorder* const ListBorder = WidgetTree->ConstructWidget<UBorder>();
	ListBorder->SetBrushColor(QuestPanelColor);
	ListBorder->SetPadding(FMargin(18));
	ListWidth->AddChild(ListBorder);
	UVerticalBox* const ListColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	ListBorder->SetContent(ListColumn);
	UCommonTextBlock* const ListHeading = NewQuestText(WidgetTree, TEXT("TS_Knight_Subhead"));
	ListHeading->SetText(LOCTEXT("QuestList", "토벌 퀘스트"));
	ListColumn->AddChildToVerticalBox(ListHeading)->SetPadding(FMargin(0, 0, 0, 14));
	QuestList = WidgetTree->ConstructWidget<UScrollBox>();
	ListColumn->AddChildToVerticalBox(QuestList)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UBorder* const DetailBorder = WidgetTree->ConstructWidget<UBorder>();
	DetailBorder->SetBrushColor(QuestPanelColor);
	DetailBorder->SetPadding(FMargin(22, 18));
	Root->AddChildToHorizontalBox(DetailBorder)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
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
	bCanLaunch = false;
	if (!Quest || !Catalog) return LOCTEXT("QuestNotReady", "준비 중");
	if (!Quest->bContentEnabled) return LOCTEXT("QuestNotReady", "준비 중");
	FText Reason;
	bCanLaunch = Catalog->CanSelectQuest(Quest->QuestId, GetGameInstance(), Reason);
	return bCanLaunch ? LOCTEXT("QuestReady", "출격 가능") :
		(Reason.IsEmpty() ? LOCTEXT("QuestNotReady", "준비 중") : Reason);
}

void UKnightQuestMenuWidget::RefreshQuests()
{
	Catalog = UKnightQuestSettings::LoadCatalog();
	QuestOrder.Reset();
	QuestButtons.Reset();
	if (QuestList) QuestList->ClearChildren();
	if (!Catalog || !QuestList)
	{
		SelectedQuestId = NAME_None;
		RefreshDetails();
		UE_LOG(Logknight, Error, TEXT("Quest menu cannot load QuestCatalog."));
		return;
	}
	TArray<UKnightQuestDefinition*> Quests;
	Catalog->GetQuestDefinitions(Quests);
	for (UKnightQuestDefinition* const Quest : Quests)
	{
		if (!Quest) continue;
		UCommonButtonBase* const Button = NewQuestButton(GetOwningPlayer());
		if (!Button) continue;
		SetButtonLabel(Button, Quest->DisplayName);
		const FName QuestId = Quest->QuestId;
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
	if (!QuestOrder.Contains(SelectedQuestId)) SelectedQuestId = QuestOrder.IsEmpty() ? NAME_None : QuestOrder[0];
	RefreshDetails();
}

bool UKnightQuestMenuWidget::SelectQuest(FName QuestId)
{
	if (!QuestId.IsNone() && (!Catalog || !Catalog->FindQuest(QuestId))) return false;
	SelectedQuestId = QuestId;
	RefreshDetails();
	return true;
}

bool UKnightQuestMenuWidget::CycleQuest(int32 Direction)
{
	if (QuestOrder.IsEmpty()) return false;
	const int32 Current = QuestOrder.IndexOfByKey(SelectedQuestId);
	const int32 Next = Current == INDEX_NONE ? (Direction >= 0 ? 0 : QuestOrder.Num() - 1)
		: (Current + (Direction >= 0 ? 1 : -1) + QuestOrder.Num()) % QuestOrder.Num();
	SelectQuest(QuestOrder[Next]);
	if (UWidget* const Button = GetFocusButton()) Button->SetUserFocus(GetOwningPlayer());
	return true;
}

UWidget* UKnightQuestMenuWidget::GetFocusButton() const
{
	const int32 Index = QuestOrder.IndexOfByKey(SelectedQuestId);
	return QuestButtons.IsValidIndex(Index) ? QuestButtons[Index].Get()
		: (QuestButtons.IsEmpty() ? LaunchButton.Get() : QuestButtons[0].Get());
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
	const UKnightQuestDefinition* const Quest = Catalog ? Catalog->FindQuest(SelectedQuestId) : nullptr;
	for (int32 Index = 0; Index < QuestButtons.Num(); ++Index)
	{
		UCommonButtonBase* const Button = QuestButtons[Index];
		if (!Button) continue;
		const bool bSelected = QuestOrder.IsValidIndex(Index) && QuestOrder[Index] == SelectedQuestId;
		if (bSelected && !Button->GetSelected()) Button->SetIsSelected(true, false);
		else if (!bSelected && Button->GetSelected()) Button->ClearSelection();
	}
	if (!Quest)
	{
		if (DetailTitle) DetailTitle->SetText(LOCTEXT("SelectQuest", "퀘스트를 선택하세요"));
		if (DetailDescription) DetailDescription->SetText(FText::GetEmpty());
		if (DetailTarget) DetailTarget->SetText(FText::GetEmpty());
		if (DetailRewards) DetailRewards->SetText(FText::GetEmpty());
		if (DetailAvailability) DetailAvailability->SetText(FText::GetEmpty());
		if (LaunchButton) LaunchButton->SetIsEnabled(false);
		return;
	}
	if (DetailTitle) DetailTitle->SetText(Quest->DisplayName);
	if (DetailDescription) DetailDescription->SetText(Quest->Description);
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
	if (LaunchButton) LaunchButton->SetIsEnabled(bCanLaunch && Flow && !Flow->IsTravelPending());
}

void UKnightQuestMenuWidget::HandleLaunchClicked()
{
	if (!SelectedQuestId.IsNone() && LaunchButton && LaunchButton->GetIsEnabled())
	{
		OnLaunchRequested.Broadcast(SelectedQuestId);
	}
}

#undef LOCTEXT_NAMESPACE
