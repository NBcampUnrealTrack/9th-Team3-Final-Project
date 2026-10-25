#include "Menu/UI/KnightMaintenanceWidget.h"
#include "Menu/UI/KnightMaintenancePresentation.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#define LOCTEXT_NAMESPACE "KnightEnhancementPresentation"

void UKnightMaintenanceWidget::BuildEnhancementPresentation()
{
	UCanvasPanel* Root = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
	if (!Root || EnhancementColumns) return;
	const FLinearColor Primary(FColor(0xED, 0xF3, 0xFA));
	const FLinearColor Secondary(FColor(0xA0, 0xB8, 0xD0));
	const FLinearColor Amber(FColor(0xF1, 0xBD, 0x5E));
	const FLinearColor Green(FColor(0xA4, 0xE7, 0x7D));
	const TSubclassOf<UCommonTextStyle> TextStyle = LoadClass<UCommonTextStyle>(nullptr,
		TEXT("/Game/Knight/UI/Style/TS_Knight_Body.TS_Knight_Body_C"));
	auto Text = [this, TextStyle](const TCHAR* Name, const FText& Value, int32 Size,
		FLinearColor Color, ETextJustify::Type Justify = ETextJustify::Left)
	{
		UCommonTextBlock* Label = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), Name);
		Label->SetStyle(TextStyle);
		// Copy the project's font, then allow this destination's hierarchy/colors to survive
		// CommonTextBlock::SynchronizeProperties (which otherwise reapplies the style CDO).
		Label->SetStyle(nullptr);
		// Font sizes are points (96-DPI Slate pixels use 4/3), while the design sizes below are pixels.
		FSlateFontInfo Font = Label->GetFont(); Font.Size = FMath::RoundToInt(Size * .75f); Label->SetFont(Font);
		Label->SetText(Value); Label->SetAutoWrapText(true); Label->SetJustification(Justify);
		Label->SetColorAndOpacity(FSlateColor(Color));
		Label->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Label;
	};
	auto Frame = [this](const TCHAR* Name, FLinearColor Fill, FLinearColor Edge)
	{
		UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(0.f), FSlateColor(Edge), 1.f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Border->SetBrush(Brush); Border->SetPadding(FMargin(18.f));
		Border->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return Border;
	};
	auto Add = [](UVerticalBox* Column, UWidget* Widget, float Bottom = 12.f)
	{
		UVerticalBoxSlot* Cell = Column->AddChildToVerticalBox(Widget);
		Cell->SetPadding(FMargin(0.f, 0.f, 0.f, Bottom));
		return Cell;
	};
	auto Line = [this, &Add](UVerticalBox* Column)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(); Size->SetHeightOverride(1.f);
		UImage* Image = WidgetTree->ConstructWidget<UImage>();
		Image->SetColorAndOpacity(FLinearColor(0.13f, 0.23f, 0.34f, 1.f));
		Image->SetVisibility(ESlateVisibility::HitTestInvisible); Size->AddChild(Image);
		Add(Column, Size, 14.f);
	};
	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("EnhancementColumns"));
	EnhancementColumns = Columns;
	UCanvasPanelSlot* ColumnsSlot = Root->AddChildToCanvas(Columns);
	ColumnsSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	ColumnsSlot->SetOffsets(FMargin(48.f, 152.f, 48.f, 100.f));
	ColumnsSlot->SetZOrder(1);
	auto Column = [this, Columns, &Frame](const TCHAR* Name, float Ratio, float LeftGap, float RightGap)
	{
		UBorder* Border = Frame(Name, FLinearColor(.007f, .013f, .022f, 1.f), FLinearColor(.10f, .19f, .29f, 1.f));
		UHorizontalBoxSlot* Cell = Columns->AddChildToHorizontalBox(Border);
		FSlateChildSize Size(ESlateSizeRule::Fill); Size.Value = Ratio; Cell->SetSize(Size);
		Cell->SetPadding(FMargin(LeftGap, 0.f, RightGap, 0.f));
		UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(); Border->SetContent(Body);
		return Body;
	};
	UVerticalBox* Left = Column(TEXT("EnhancementOwnedColumn"), .25f, 0.f, 6.f);
	UVerticalBox* Center = Column(TEXT("EnhancementTargetColumn"), .45f, 6.f, 6.f);
	UVerticalBox* Right = Column(TEXT("EnhancementPreviewColumn"), .30f, 6.f, 0.f);
	Add(Left, Text(TEXT("EnhancementOwnedTitle"), LOCTEXT("Owned", "보유 무기"), 28, Primary)); Line(Left);
	EnhancementOwnedList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("EnhancementOwnedWeapons"));
	Add(Left, EnhancementOwnedList)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	EnhancementListStatus = Text(TEXT("EnhancementListStatus"), LOCTEXT("Choose", "강화할 무기를 선택하세요."), 22, Secondary);
	Add(Left, EnhancementListStatus, 0.f);
	Add(Center, Text(TEXT("EnhancementTargetTitle"), LOCTEXT("Target", "강화 대상"), 28, Primary)); Line(Center);
	EnhancementDetailPage = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Phase7_EnhancementDetailPage"));
	Add(Center, EnhancementDetailPage, 10.f)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UpgradePanel = WidgetTree->ConstructWidget<UVerticalBox>(); EnhancementDetailPage->AddChild(UpgradePanel);
	UpgradeHeading = Text(TEXT("EnhancementWeaponName"), FText::GetEmpty(), 30, Primary, ETextJustify::Center);
	Add(UpgradePanel, UpgradeHeading, 8.f);
	EnhancementLevels = Text(TEXT("EnhancementLevels"), FText::GetEmpty(), 28, Amber, ETextJustify::Center);
	Add(UpgradePanel, EnhancementLevels, 12.f);
	UBorder* IconFrame = Frame(TEXT("EnhancementIconFrame"), FLinearColor(.009f, .019f, .033f, 1.f), FLinearColor(.12f, .26f, .39f, 1.f));
	IconFrame->SetPadding(FMargin(12.f));
	USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>();
	IconSize->SetWidthOverride(320.f); IconSize->SetHeightOverride(180.f);
	UScaleBox* IconScale = WidgetTree->ConstructWidget<UScaleBox>(); IconScale->SetStretch(EStretch::ScaleToFit);
	EnhancementWeaponImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("EnhancementWeaponImage"));
	EnhancementWeaponImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	IconScale->AddChild(EnhancementWeaponImage); IconSize->AddChild(IconScale); IconFrame->SetContent(IconSize);
	Add(UpgradePanel, IconFrame, 12.f)->SetHorizontalAlignment(HAlign_Center);
	Add(UpgradePanel, Text(TEXT("EnhancementChanceLabel"), LOCTEXT("Probability", "이번 시도 성공률"), 22, Secondary, ETextJustify::Center), 4.f);
	UpgradeChance = Text(TEXT("EnhancementChance"), FText::GetEmpty(), 36, Secondary, ETextJustify::Center);
	Add(UpgradePanel, UpgradeChance, 16.f); Line(UpgradePanel);
	Add(UpgradePanel, Text(TEXT("EnhancementMaterialTitle"), LOCTEXT("Materials", "필요 재료"), 24, Secondary), 8.f);
	UHorizontalBox* MaterialRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	USizeBox* MaterialSize = WidgetTree->ConstructWidget<USizeBox>(); MaterialSize->SetWidthOverride(56.f); MaterialSize->SetHeightOverride(56.f);
	UScaleBox* MaterialScale = WidgetTree->ConstructWidget<UScaleBox>(); MaterialScale->SetStretch(EStretch::ScaleToFit);
	EnhancementMaterialImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("EnhancementMaterialImage"));
	MaterialScale->AddChild(EnhancementMaterialImage); MaterialSize->AddChild(MaterialScale);
	MaterialRow->AddChildToHorizontalBox(MaterialSize)->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
	UpgradeGate = Text(TEXT("EnhancementMaterialRequirement"), FText::GetEmpty(), 24, Primary);
	UHorizontalBoxSlot* GateSlot = MaterialRow->AddChildToHorizontalBox(UpgradeGate);
	GateSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); GateSlot->SetVerticalAlignment(VAlign_Center);
	Add(UpgradePanel, MaterialRow, 10.f);
	UpgradeMoney = Text(TEXT("EnhancementMoneyCost"), FText::GetEmpty(), 24, Primary);
	// Keep the quote and eligibility visible above the action even when materials need scrolling.
	Add(Center, UpgradeMoney, 6.f);
	UpgradeResult = Text(TEXT("EnhancementResult"), FText::GetEmpty(), 22, Secondary);
	Add(Center, UpgradeResult, 8.f);
	const TSubclassOf<UCommonButtonBase> ButtonClass = LoadClass<UCommonButtonBase>(nullptr,
		TEXT("/Game/Knight/UI/Common/WBP_KnightButton.WBP_KnightButton_C"));
	UpgradeButton = ButtonClass ? CreateWidget<UCommonButtonBase>(GetOwningPlayer(), ButtonClass, TEXT("Phase7_UpgradeButton")) : nullptr;
	if (UpgradeButton)
	{
		if (UFunction* Function = UpgradeButton->FindFunction(TEXT("SetButtonText")))
		{
			struct { FText InText; } Params{ LOCTEXT("Upgrade", "강화") }; UpgradeButton->ProcessEvent(Function, &Params);
		}
		KnightMaintenancePresentation::ConfigureButton(UpgradeButton, false);
		UpgradeButton->OnClicked().AddWeakLambda(this, [this]() { RequestWeaponUpgrade(); });
		USizeBox* ButtonSize = WidgetTree->ConstructWidget<USizeBox>(); ButtonSize->SetMinDesiredHeight(62.f); ButtonSize->AddChild(UpgradeButton);
		Add(Center, ButtonSize, 0.f);
	}
	Add(Right, Text(TEXT("EnhancementPreviewTitle"), LOCTEXT("Preview", "강화 미리보기"), 28, Primary)); Line(Right);
	UScrollBox* PreviewScroll = WidgetTree->ConstructWidget<UScrollBox>();
	Add(Right, PreviewScroll, 0.f)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UVerticalBox* Preview = WidgetTree->ConstructWidget<UVerticalBox>(); PreviewScroll->AddChild(Preview);
	EnhancementPreviewLevels = Text(TEXT("EnhancementPreviewLevels"), FText::GetEmpty(), 26, Amber);
	Add(Preview, EnhancementPreviewLevels, 20.f); Line(Preview);
	UHorizontalBox* AttackRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	UHorizontalBoxSlot* AttackLabel = AttackRow->AddChildToHorizontalBox(Text(TEXT("EnhancementAttackLabel"), LOCTEXT("Attack", "공격력"), 24, Secondary));
	AttackLabel->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UpgradeAttack = Text(TEXT("EnhancementCurrentAttack"), FText::GetEmpty(), 28, Primary, ETextJustify::Right);
	UpgradeAttack->SetAutoWrapText(false);
	AttackRow->AddChildToHorizontalBox(UpgradeAttack)->SetPadding(FMargin(8.f, 0.f, 12.f, 0.f));
	EnhancementIncrease = Text(TEXT("EnhancementAttackIncrease"), FText::GetEmpty(), 28, Green, ETextJustify::Right);
	EnhancementIncrease->SetAutoWrapText(false);
	AttackRow->AddChildToHorizontalBox(EnhancementIncrease);
	Add(Preview, AttackRow, 20.f); Line(Preview);
	EnhancementStatsText = Text(TEXT("EnhancementOwnStats"), FText::GetEmpty(), 22, Secondary);
	Add(Preview, EnhancementStatsText, 20.f);
	EnhancementSuccessSummary = Text(TEXT("EnhancementSuccessSummary"), FText::GetEmpty(), 26, Green);
	UBorder* SummaryFrame = Frame(TEXT("EnhancementSuccessFrame"), FLinearColor(.025f, .027f, .016f, 1.f), FLinearColor(.31f, .25f, .09f, 1.f));
	SummaryFrame->SetContent(EnhancementSuccessSummary); Add(Preview, SummaryFrame, 32.f);
	Add(Preview, Text(TEXT("EnhancementGuideTitle"), LOCTEXT("Guide", "강화 안내"), 28, Primary)); Line(Preview);
	EnhancementGuidance = Text(TEXT("EnhancementGuidance"), FText::GetEmpty(), 22, Secondary);
	Add(Preview, EnhancementGuidance, 0.f);
	UHorizontalBox* Hint = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("EnhancementInputHints"));
	EnhancementInputHints = Hint;
	UBorder* Key = Frame(TEXT("EnhancementEnterKey"), FLinearColor::Transparent, FLinearColor(.22f, .38f, .52f, 1.f));
	Key->SetPadding(FMargin(6.f, 2.f));
	UCommonTextBlock* KeyLabel = Text(TEXT("EnhancementEnterLabel"), INVTEXT("Enter"), 20, Secondary);
	KeyLabel->SetAutoWrapText(false); Key->SetContent(KeyLabel);
	Hint->AddChildToHorizontalBox(Key);
	UCommonTextBlock* HintLabel = Text(TEXT("EnhancementActionHint"), LOCTEXT("Input", "선택 / 강화"), 20, Secondary);
	HintLabel->SetAutoWrapText(false);
	UHorizontalBoxSlot* HintTextSlot = Hint->AddChildToHorizontalBox(HintLabel);
	HintTextSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f)); HintTextSlot->SetVerticalAlignment(VAlign_Center);
	UCanvasPanelSlot* HintSlot = Root->AddChildToCanvas(Hint);
	HintSlot->SetAnchors(FAnchors(0.f, 1.f)); HintSlot->SetAlignment(FVector2D(0.f, 1.f));
	HintSlot->SetOffsets(FMargin(48.f, -60.f, 0.f, 0.f)); HintSlot->SetAutoSize(true);
	Hint->SetVisibility(ESlateVisibility::Collapsed);
	Columns->SetVisibility(ESlateVisibility::Collapsed);
}

#undef LOCTEXT_NAMESPACE
