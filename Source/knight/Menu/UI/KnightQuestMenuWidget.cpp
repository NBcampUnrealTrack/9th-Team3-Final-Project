#include "Menu/UI/KnightQuestMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/BorderSlot.h"
#include "Components/ScaleBox.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Widgets/Layout/SBorder.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "GameFlow/KnightGameFlowSubsystem.h"
#include "Quest/KnightQuestData.h"
#include "knightItemDefinition.h"
#include "knight.h"
#include "Menu/UI/KnightOperationMapPresentation.h"
#include "Menu/UI/KnightButtonFillExtension.h"

#define LOCTEXT_NAMESPACE "KnightQuestMenu"

namespace
{
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

namespace
{
    // The polygon, its border and its children share exactly the same layout geometry.
    class SOperationQuestFrame : public SBorder
    {
    public:
        virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull,
            FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool Enabled) const override
        {
            const FVector2f Size(G.GetLocalSize());
            const float Cut = FMath::Min(14.f, Size.Y * .18f);
            TArray<FVector2f> Points{{Cut,0},{Size.X-Cut,0},{Size.X,Cut},{Size.X,Size.Y-Cut},
                {Size.X-Cut,Size.Y},{Cut,Size.Y},{0,Size.Y-Cut},{0,Cut}};
            TArray<FSlateVertex> V; TArray<SlateIndex> Indices;
            const FLinearColor Tint = Style.GetColorAndOpacityTint();
            const FColor Fill = (FLinearColor(FColor(9,20,31,239)) * Tint).ToFColor(true);
            for (const auto& Point : Points)
                V.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(), Point, FVector2f::ZeroVector, Fill));
            for (int32 I=1; I<7; ++I) { Indices.Add(0); Indices.Add(I); Indices.Add(I+1); }
            FSlateDrawElement::MakeCustomVerts(Out, Layer, FSlateResourceHandle(), V, Indices, nullptr, 0, 0);
            TArray<FVector2D> Edge; for (const auto& Point : Points) Edge.Add(FVector2D(Point)); Edge.Add(FVector2D(Points[0]));
            FSlateDrawElement::MakeLines(Out, Layer+1, G.ToPaintGeometry(), Edge, ESlateDrawEffect::None,
                FLinearColor(FColor(78,128,165,210)) * Tint, true, 1.f);
            // SCompoundWidget draws only the children, not SBorder's rectangular brush.
            return SCompoundWidget::OnPaint(Args, G, Cull, Out, Layer+2, Style, Enabled);
        }
    };
    void QuestFontSize(UCommonTextBlock* Text, int32 Size)
    {
        auto Font = Text->GetFont(); Font.Size = Size; Text->SetFont(Font);
    }
    FText PartCondition(const FknightRaidReward& Reward, EKnightQuestBreakPart Fallback)
    {
        const FString Tag = Reward.RequiredBrokenPart.ToString();
        if (Tag.EndsWith(TEXT(".Chest"))) return LOCTEXT("ChestBreak", "가슴 파괴");
        if (Tag.EndsWith(TEXT(".Head"))) return LOCTEXT("HeadBreak", "머리 파괴");
        if (Tag.EndsWith(TEXT(".Tail"))) return LOCTEXT("TailBreak", "꼬리 파괴");
        if (!Tag.IsEmpty()) return FText::FromString(Tag);
        return Fallback == EKnightQuestBreakPart::Head ? LOCTEXT("HeadBreak", "머리 파괴")
            : Fallback == EKnightQuestBreakPart::Tail ? LOCTEXT("TailBreak", "꼬리 파괴") : LOCTEXT("PartBreak", "부위 파괴");
    }
}

TSharedRef<SWidget> UKnightQuestPanelFrame::RebuildWidget()
{
    MyBorder = SNew(SOperationQuestFrame);
    if (GetChildrenCount() > 0) CastChecked<UBorderSlot>(GetContentSlot())->BuildSlot(MyBorder.ToSharedRef());
    return MyBorder.ToSharedRef();
}

void UKnightQuestMenuWidget::BuildLayout()
{
    if (!WidgetTree || WidgetTree->RootWidget) return;
    auto* Root = WidgetTree->ConstructWidget<UKnightQuestPanelFrame>();
    Root->SetPadding(FMargin(24,20)); Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    WidgetTree->RootWidget = Root;
    ContentColumn = WidgetTree->ConstructWidget<UVerticalBox>(); Root->SetContent(ContentColumn);
    RegionHeading = NewQuestText(WidgetTree, TEXT("TS_Knight_Subhead")); QuestFontSize(RegionHeading, 26);
    RegionHeading->SetText(LOCTEXT("ChooseRegion", "지도에서 구역을 선택하세요"));
    ContentColumn->AddChildToVerticalBox(RegionHeading)->SetPadding(FMargin(0,0,0,16));
    auto* Divider = WidgetTree->ConstructWidget<UImage>(); Divider->SetColorAndOpacity(FLinearColor(FColor(102,169,211)));
    auto* Line = WidgetTree->ConstructWidget<USizeBox>(); Line->SetHeightOverride(1); Line->SetContent(Divider);
    ContentColumn->AddChildToVerticalBox(Line)->SetPadding(FMargin(0,0,0,14));
    DetailBorder = WidgetTree->ConstructWidget<UKnightQuestPanelFrame>(); DetailBorder->SetPadding(FMargin(12,8));
    ContentColumn->AddChildToVerticalBox(DetailBorder)->SetPadding(FMargin(0,0,0,12));
    auto* Heading = WidgetTree->ConstructWidget<UHorizontalBox>(); DetailBorder->SetContent(Heading);
    DetailTitle = NewQuestText(WidgetTree,TEXT("TS_Knight_Subhead")); QuestFontSize(DetailTitle,22);
    auto* TitleSlot = Heading->AddChildToHorizontalBox(DetailTitle); TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); TitleSlot->SetVerticalAlignment(VAlign_Center);
    auto* BadgeFrame = WidgetTree->ConstructWidget<UKnightQuestPanelFrame>(); BadgeFrame->SetPadding(FMargin(10,4));
    DifficultyBadge = NewQuestText(WidgetTree,TEXT("TS_Knight_Caption")); QuestFontSize(DifficultyBadge,16); DifficultyBadge->SetAutoWrapText(false);
    DifficultyBadge->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(134,200,240)))); BadgeFrame->SetContent(DifficultyBadge);
    auto* BadgeSlot = Heading->AddChildToHorizontalBox(BadgeFrame); BadgeSlot->SetPadding(FMargin(10,0,0,0)); BadgeSlot->SetVerticalAlignment(VAlign_Center);
    RewardScroll = WidgetTree->ConstructWidget<UScrollBox>(); RewardScroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
    RewardScroll->SetScrollbarThickness(FVector2D(3,3)); RewardScroll->SetAnimateWheelScrolling(false);
    FScrollBoxStyle ScrollStyle = RewardScroll->GetWidgetStyle();
    ScrollStyle.TopShadowBrush = ScrollStyle.BottomShadowBrush = ScrollStyle.LeftShadowBrush = ScrollStyle.RightShadowBrush = FSlateNoResource();
    RewardScroll->SetWidgetStyle(ScrollStyle);
    FScrollBarStyle BarStyle = RewardScroll->GetWidgetBarStyle();
    BarStyle.VerticalBackgroundImage = BarStyle.HorizontalBackgroundImage = FSlateNoResource();
    BarStyle.NormalThumbImage = FSlateColorBrush(FLinearColor(FColor(64,105,133,150)));
    BarStyle.HoveredThumbImage = BarStyle.DraggedThumbImage = FSlateColorBrush(FLinearColor(FColor(117,174,209,210)));
    RewardScroll->SetWidgetBarStyle(BarStyle);
    ContentColumn->AddChildToVerticalBox(RewardScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    RewardRows = WidgetTree->ConstructWidget<UVerticalBox>(); RewardScroll->AddChild(RewardRows);
    DetailAvailability = NewQuestText(WidgetTree,TEXT("TS_Knight_Caption")); QuestFontSize(DetailAvailability,15);
    DetailAvailability->SetColorAndOpacity(FSlateColor(QuestMutedColor));
    ContentColumn->AddChildToVerticalBox(DetailAvailability)->SetPadding(FMargin(0,8,0,8));
    LaunchButton = NewQuestButton(GetOwningPlayer());
    if (LaunchButton)
    {
        SetButtonLabel(LaunchButton,LOCTEXT("Launch","출격"));
        auto* ButtonHeight = WidgetTree->ConstructWidget<USizeBox>(); ButtonHeight->SetHeightOverride(54); ButtonHeight->SetContent(LaunchButton);
        ContentColumn->AddChildToVerticalBox(ButtonHeight)->SetPadding(FMargin(0,10,0,0));
        LaunchButton->OnClicked().AddUObject(this,&ThisClass::HandleLaunchClicked);
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
	const auto State = KnightOperationMapPresentation::Query(Catalog, Quest, GetGameInstance());
	bCanLaunch = State.bCanLaunch;
	return bCanLaunch ? FText::GetEmpty() : State.Reason.IsEmpty() ? State.Availability : State.Reason;
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
    if (RegionHeading) RegionHeading->SetText(ActiveRegionId.IsNone() ? LOCTEXT("ChooseRegion", "지도에서 구역을 선택하세요")
        : ActiveRegionName.IsEmpty() ? FText::FromName(ActiveRegionId) : ActiveRegionName);
    if (!Catalog || !RegionQuestIds.Contains(SelectedQuestId)) SelectedQuestId = NAME_None;
    RefreshDetails();
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

UWidget* UKnightQuestMenuWidget::GetFocusButton() const
{
    return !bPanelChanging && LaunchButton && LaunchButton->GetIsEnabled() ? LaunchButton.Get() : nullptr;
}

void UKnightQuestMenuWidget::AddSection(const FText& Label)
{
    auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    auto* Text = NewQuestText(WidgetTree,TEXT("TS_Knight_Subhead")); QuestFontSize(Text,20);
    Text->SetAutoWrapText(false); Text->SetText(Label); Text->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(184,215,239))));
    Row->AddChildToHorizontalBox(Text)->SetPadding(FMargin(0,0,12,0));
    auto* LineBox = WidgetTree->ConstructWidget<USizeBox>(); LineBox->SetHeightOverride(1);
    auto* Line = WidgetTree->ConstructWidget<UImage>(); Line->SetColorAndOpacity(FLinearColor(FColor(72,107,132))); LineBox->SetContent(Line);
    auto* LineSlot = Row->AddChildToHorizontalBox(LineBox); LineSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); LineSlot->SetVerticalAlignment(VAlign_Center);
    RewardRows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0,6,0,6));
}

void UKnightQuestMenuWidget::AddRewardRow(const FknightRaidReward& Reward, const FText& Condition)
{
    auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    auto* IconFrame = WidgetTree->ConstructWidget<UKnightQuestPanelFrame>(); IconFrame->SetPadding(FMargin(4));
    auto* IconSize = WidgetTree->ConstructWidget<USizeBox>(); IconSize->SetWidthOverride(58); IconSize->SetHeightOverride(40); IconSize->SetContent(IconFrame);
    auto* Fit = WidgetTree->ConstructWidget<UScaleBox>(); Fit->SetStretch(EStretch::ScaleToFit); IconFrame->SetContent(Fit);
    auto* Icon = WidgetTree->ConstructWidget<UImage>(); RewardIcons.Add(Icon);
    UTexture2D* Texture = Reward.Item ? Reward.Item->Icon.LoadSynchronous() : nullptr;
    if (Texture) { Icon->SetBrushFromTexture(Texture, true); Fit->SetContent(Icon); }
    else
    {
        // Existing neutral missing-image convention: an em dash, not a fabricated item illustration.
        auto* Missing = NewQuestText(WidgetTree,TEXT("TS_Knight_Caption")); Missing->SetText(FText::FromString(TEXT("—"))); Fit->SetContent(Missing);
        const FString Path = Reward.Item ? Reward.Item->Icon.ToString() : TEXT("null item");
        const FString ItemPath = GetPathNameSafe(Reward.Item);
        if (!ReportedMissingIcons.Contains(ItemPath)) { ReportedMissingIcons.Add(ItemPath); UE_LOG(Logknight, Warning, TEXT("OperationMap missing reward icon: item=%s icon=%s"), *ItemPath,*Path); }
    }
    Row->AddChildToHorizontalBox(IconSize)->SetPadding(FMargin(0,0,10,0));
    auto* Name = NewQuestText(WidgetTree,TEXT("TS_Knight_Body")); QuestFontSize(Name,17);
    Name->SetText(Reward.Item ? Reward.Item->DisplayName : LOCTEXT("UnknownItem","아이템 정보 없음"));
    auto* NameSlot = Row->AddChildToHorizontalBox(Name); NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); NameSlot->SetVerticalAlignment(VAlign_Center);
    auto* Values = NewQuestText(WidgetTree,TEXT("TS_Knight_Body")); QuestFontSize(Values,16); Values->SetAutoWrapText(false); Values->SetJustification(ETextJustify::Right);
    const FText Chance = Reward.Chance >= 1.f - KINDA_SMALL_NUMBER ? LOCTEXT("Guaranteed","확정") : FText::AsPercent(Reward.Chance);
    Values->SetText(Condition.IsEmpty() ? FText::Format(LOCTEXT("RewardValue","×{0} · {1}"),RewardCount(Reward),Chance)
        : FText::Format(LOCTEXT("ConditionalValue","{0} · ×{1} · {2}"),Condition,RewardCount(Reward),Chance));
    RewardValues.Add(Values);
    auto* ValueSlot = Row->AddChildToHorizontalBox(Values); ValueSlot->SetPadding(FMargin(8,0,0,0)); ValueSlot->SetVerticalAlignment(VAlign_Center);
    RewardRows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0,0,0,3));
}

void UKnightQuestMenuWidget::RefreshDetails()
{
    const bool bSelectionChanged = DisplayedQuestId != SelectedQuestId;
    DisplayedQuestId = SelectedQuestId;
    const UKnightQuestDefinition* Quest = Catalog ? Catalog->FindQuest(SelectedQuestId) : nullptr;
    const float Scroll = bSelectionChanged ? 0.f : RewardScroll->GetScrollOffset();
    RewardRows->ClearChildren(); RewardIcons.Reset(); RewardValues.Reset(); CurrencyText = nullptr;
    DetailBorder->SetVisibility(Quest ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (!Quest)
    {
        auto* Empty = NewQuestText(WidgetTree,TEXT("TS_Knight_Body")); Empty->SetText(LOCTEXT("SelectQuest","지도에서 난이도 또는 탐색을 선택하세요.")); RewardRows->AddChildToVerticalBox(Empty);
        DetailAvailability->SetVisibility(ESlateVisibility::Collapsed); LaunchButton->SetIsEnabled(false); return;
    }
    FString MissionName = Quest->DisplayName.ToString();
    MissionName.RemoveFromEnd(Quest->Difficulty == EKnightQuestDifficulty::Hard ? TEXT(" (어려움)") : TEXT(" (일반)"));
    DetailTitle->SetText(FText::FromString(MissionName));
    DifficultyBadge->SetText(Quest->bMapExplorationOnly ? LOCTEXT("ExploreBadge","맵 탐색") : Quest->Difficulty == EKnightQuestDifficulty::Hard ? LOCTEXT("Hard","어려움") : LOCTEXT("Normal","일반"));
    if (Quest->bMapExplorationOnly)
    {
        auto* Empty = NewQuestText(WidgetTree,TEXT("TS_Knight_Body")); QuestFontSize(Empty,18);
        Empty->SetText(LOCTEXT("ExplorationRewards","탐색 출격에는 보상이 없습니다.")); RewardRows->AddChildToVerticalBox(Empty);
    }
    else
    {
        AddSection(LOCTEXT("ClearRewardHeading","클리어 보상"));
        CurrencyText = NewQuestText(WidgetTree,TEXT("TS_Knight_Body")); QuestFontSize(CurrencyText,23);
        CurrencyText->SetText(FText::FromString(FString::Printf(TEXT("%dgold"),Quest->MoneyReward)));
        RewardRows->AddChildToVerticalBox(CurrencyText)->SetPadding(FMargin(12,0,0,4));
        AddSection(LOCTEXT("MaterialHeading","획득 소재"));
        for (const auto& Reward : Quest->ClearRewards)
            AddRewardRow(Reward, Reward.RequiredBrokenPart.IsValid() ? PartCondition(Reward, EKnightQuestBreakPart::Other) : FText::GetEmpty());
        if (Quest->ClearRewards.IsEmpty())
        { auto* Empty = NewQuestText(WidgetTree,TEXT("TS_Knight_Caption")); Empty->SetText(LOCTEXT("NoMaterials","소재 보상 없음")); RewardRows->AddChildToVerticalBox(Empty); }
        if (!Quest->PartBreakRewards.IsEmpty())
        {
            AddSection(LOCTEXT("PartRewardHeading","부위 파괴 보상"));
            for (const auto& Entry : Quest->PartBreakRewards) AddRewardRow(Entry.Reward, PartCondition(Entry.Reward,Entry.Part));
        }
        if (Quest->FirstBossSocketSlots > 0)
        {
            auto* Bonus = NewQuestText(WidgetTree,TEXT("TS_Knight_Caption")); QuestFontSize(Bonus,14);
            Bonus->SetText(FText::Format(LOCTEXT("FirstBossSocket","해당 보스 최초 토벌: 공용 소켓 +{0}칸"),FText::AsNumber(Quest->FirstBossSocketSlots)));
            Bonus->SetColorAndOpacity(FSlateColor(QuestMutedColor)); RewardRows->AddChildToVerticalBox(Bonus)->SetPadding(FMargin(0,8,0,0));
        }
    }
    RewardScroll->SetScrollOffset(Scroll);
    bool CanLaunch = false; const FText Reason = GetAvailability(Quest,CanLaunch);
    DetailAvailability->SetText(Reason); DetailAvailability->SetVisibility(Reason.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    const auto* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>() : nullptr;
    if (LaunchButton) LaunchButton->SetIsEnabled(!bPanelChanging && CanLaunch && Flow && !Flow->IsTravelPending());
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
