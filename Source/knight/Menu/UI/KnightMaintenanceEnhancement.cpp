#include "Menu/UI/KnightMaintenanceWidget.h"
#include "Menu/UI/KnightMaintenancePresentation.h"
#include "Brushes/SlateColorBrush.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Progression/knightProgressionSubsystem.h"

#define LOCTEXT_NAMESPACE "KnightEnhancementPresentation"

namespace
{
	constexpr int32 EnhancementBarCount = 318;
	constexpr float BarRiseDuration = .34f;
	constexpr float BarTrembleDuration = .24f;
	constexpr float BarResultDuration = .36f;

	float EaseOutCubic(float T)
	{
		const float Remaining = 1.f - FMath::Clamp(T, 0.f, 1.f);
		return 1.f - Remaining * Remaining * Remaining;
	}

	FVector2D BarAnchor(int32 Index)
	{
		// A broad field plus a denser central field; deterministic variation never touches gameplay RNG.
		const bool bCenter = Index >= 126;
		const int32 Cell = bCenter ? Index - 126 : Index;
		const int32 Columns = bCenter ? 24 : 18;
		const int32 Rows = bCenter ? 8 : 7;
		const float X = (Cell % Columns + .5f + .18f * ((Index * 7) % 5 - 2)) / Columns;
		const float Y = (Cell / Columns + .5f + .12f * ((Index * 11) % 5 - 2)) / Rows;
		return FVector2D(FMath::Lerp(bCenter ? .30f : .07f, bCenter ? .70f : .93f, X),
			FMath::Lerp(.09f, .91f, Y));
	}

	FVector2D BarJitter(int32 Index, float Time)
	{
		const int32 Beat = FMath::FloorToInt((Time + Index * .017f) * (23.f + (Index % 6) * 5.f));
		return FVector2D(.0025f * ((Beat + Index * 2) % 3 - 1),
			.017f * ((Beat * 2 + Index) % 3 - 1));
	}
}

void UKnightMaintenanceWidget::BuildIndustrialSideSurfaces()
{
	UCanvasPanel* Root = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
	if (!Root || WidgetTree->FindWidget(TEXT("IndustrialSurfaceLeft"))) return;
	for (bool bRight : { false, true })
	{
		UImage* Surface = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
			bRight ? TEXT("IndustrialSurfaceRight") : TEXT("IndustrialSurfaceLeft"));
		Surface->SetBrush(KnightMaintenancePresentation::MakeEdgePanelBrush(Surface, IndustrialSidePanelTexture, bRight));
		Surface->SetColorAndOpacity(KnightMaintenancePresentation::PanelTint);
		Surface->SetVisibility(ESlateVisibility::Collapsed);
		UCanvasPanelSlot* Cell = Root->AddChildToCanvas(Surface);
		Cell->SetAnchors(KnightMaintenancePresentation::ShiftSideAnchors(
			bRight ? FAnchors(.70f, 0.f, 1.f, 1.f) : FAnchors(0.f, 0.f, .26f, 1.f), bRight));
		Cell->SetOffsets(FMargin(0.f)); Cell->SetZOrder(-6);
	}
}

void UKnightMaintenanceWidget::BuildEnhancementPresentation()
{
	UCanvasPanel* Root = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
	if (!Root || EnhancementColumns) return;
	BuildIndustrialSideSurfaces();
	// One opaque scene replacement; decorative artwork never participates in hit testing.
	UScaleBox* Backdrop = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("EnhancementArtwork"));
	Backdrop->SetStretch(EStretch::ScaleToFill);
	Backdrop->SetClipping(EWidgetClipping::ClipToBounds);
	UImage* Background = WidgetTree->ConstructWidget<UImage>();
	Background->SetBrushFromTexture(EnhancementBackgroundTexture, true);
	Background->SetColorAndOpacity(FLinearColor(.65f, .65f, .65f, 1.f));
	Backdrop->AddChild(Background);
	UCanvasPanelSlot* ArtSlot = Root->AddChildToCanvas(Backdrop);
	ArtSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); ArtSlot->SetOffsets(FMargin(0.f)); ArtSlot->SetZOrder(-8);
	Backdrop->SetVisibility(ESlateVisibility::Collapsed);
	EnhancementArtwork = Backdrop;
	const FLinearColor Primary = KnightMaintenancePresentation::Primary;
	const FLinearColor Secondary = KnightMaintenancePresentation::Secondary;
	const FLinearColor Amber = KnightMaintenancePresentation::WarmHighlight;
	const FLinearColor Green(FColor(0xA4, 0xE7, 0x7D));
	const TSubclassOf<UCommonTextStyle> TextStyle = LoadClass<UCommonTextStyle>(nullptr,
		TEXT("/Game/Knight/UI/Menu/Maintenance/Hologram/TS_Holo_Body.TS_Holo_Body_C"));
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
	auto Frame = [this](const TCHAR* Name)
	{
		UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		FSlateBrush Brush;
		// Raster projected-glass frame; no physical machinery or old flat frame.
		Brush.SetResourceObject(EnhancementPanelTexture);
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.ImageSize = FVector2D(256.f, 256.f);
		Brush.Margin = FMargin(.04f);
		Brush.TintColor = FSlateColor(KnightMaintenancePresentation::PanelTint);
		Border->SetBrush(Brush); Border->SetPadding(FMargin(42.f));
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
		Image->SetColorAndOpacity(KnightMaintenancePresentation::Divider);
		Image->SetVisibility(ESlateVisibility::HitTestInvisible); Size->AddChild(Image);
		Add(Column, Size, 14.f);
	};
	UCanvasPanel* Columns = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("EnhancementColumns"));
	EnhancementColumns = Columns;
	UCanvasPanelSlot* ColumnsSlot = Root->AddChildToCanvas(Columns);
	ColumnsSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	ColumnsSlot->SetOffsets(FMargin(0.f));
	ColumnsSlot->SetZOrder(1);
	auto Column = [this, Columns, &Frame](const TCHAR* Name, const FAnchors& Anchors, FMargin ContentPadding)
	{
		UBorder* Border = Frame(Name);
		UCanvasPanelSlot* Cell = Columns->AddChildToCanvas(Border);
		Cell->SetAnchors(Anchors); Cell->SetOffsets(FMargin(0.f));
		FSlateBrush Empty; Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
		Border->SetBrush(Empty); Border->SetPadding(ContentPadding);
		UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(); Border->SetContent(Body);
		return Body;
	};
	// Independent canvas anchors translate the whole side columns without changing
	// their width, padding, or the central apparatus layout.
	UVerticalBox* Left = Column(TEXT("EnhancementOwnedColumn"), KnightMaintenancePresentation::ShiftSideAnchors(FAnchors(0.f, 0.f, .26f, 1.f), false), FMargin(48.f, 176.f, 32.f, 42.f));
	UVerticalBox* Center = Column(TEXT("EnhancementTargetColumn"), FAnchors(.26f, 0.f, .70f, 1.f), FMargin(32.f, 178.f, 32.f, 80.f));
	UVerticalBox* Right = Column(TEXT("EnhancementPreviewColumn"), KnightMaintenancePresentation::ShiftSideAnchors(FAnchors(.70f, 0.f, 1.f, 1.f), true), FMargin(56.f, 176.f, 64.f, 110.f));
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
	UBorder* IconFrame = Frame(TEXT("EnhancementIconFrame"));
	FSlateBrush NoFrame; NoFrame.DrawAs = ESlateBrushDrawType::NoDrawType;
	IconFrame->SetBrush(NoFrame); IconFrame->SetPadding(FMargin(8.f));
	IconFrame->SetContent(BuildEnhancementPreview());
	Add(UpgradePanel, IconFrame, 12.f)->SetHorizontalAlignment(HAlign_Fill);
	// Keep live numbers away from the apparatus's luminous ring on a recessed raster surface.
	UBorder* Readout = Frame(TEXT("EnhancementReadout"));
	FSlateBrush ReadoutBrush; ReadoutBrush.SetResourceObject(EnhancementButtonTexture);
	ReadoutBrush.DrawAs = ESlateBrushDrawType::Box; ReadoutBrush.Margin = KnightMaintenancePresentation::ControlSlice;
	KnightMaintenancePresentation::SetControlUV(ReadoutBrush);
	ReadoutBrush.TintColor = FSlateColor(KnightMaintenancePresentation::ReadoutTint);
	Readout->SetBrush(ReadoutBrush); Readout->SetPadding(FMargin(36.f, 14.f));
	UVerticalBox* ReadoutContent = WidgetTree->ConstructWidget<UVerticalBox>(); Readout->SetContent(ReadoutContent);
	Add(UpgradePanel, Readout, 4.f);
	Add(ReadoutContent, Text(TEXT("EnhancementChanceLabel"), LOCTEXT("Probability", "이번 시도 성공률"), 22, Secondary, ETextJustify::Center), 4.f);
	UpgradeChance = Text(TEXT("EnhancementChance"), FText::GetEmpty(), 36, Secondary, ETextJustify::Center);
	Add(ReadoutContent, UpgradeChance, 12.f);
	Add(ReadoutContent, Text(TEXT("EnhancementMaterialTitle"), LOCTEXT("Materials", "필요 재료"), 24, Secondary), 8.f);
	UHorizontalBox* MaterialRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	USizeBox* MaterialSize = WidgetTree->ConstructWidget<USizeBox>(); MaterialSize->SetWidthOverride(56.f); MaterialSize->SetHeightOverride(56.f);
	UScaleBox* MaterialScale = WidgetTree->ConstructWidget<UScaleBox>(); MaterialScale->SetStretch(EStretch::ScaleToFit);
	EnhancementMaterialImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("EnhancementMaterialImage"));
	MaterialScale->AddChild(EnhancementMaterialImage); MaterialSize->AddChild(MaterialScale);
	MaterialRow->AddChildToHorizontalBox(MaterialSize)->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
	UpgradeGate = Text(TEXT("EnhancementMaterialRequirement"), FText::GetEmpty(), 24, Primary);
	UHorizontalBoxSlot* GateSlot = MaterialRow->AddChildToHorizontalBox(UpgradeGate);
	GateSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); GateSlot->SetVerticalAlignment(VAlign_Center);
	Add(ReadoutContent, MaterialRow, 0.f);
	UpgradeMoney = Text(TEXT("EnhancementMoneyCost"), FText::GetEmpty(), 24, Primary);
	// Keep the quote and eligibility visible above the action even when materials need scrolling.
	Add(Center, UpgradeMoney, 6.f);
	UpgradeResult = Text(TEXT("EnhancementResult"), FText::GetEmpty(), 22, Secondary);
	UpgradeResult->SetVisibility(ESlateVisibility::Collapsed);
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
		if (EnhancementActionStyle) UpgradeButton->SetStyle(EnhancementActionStyle);
		UpgradeButton->OnClicked().AddWeakLambda(this, [this]() { RequestWeaponUpgrade(); });
		USizeBox* ButtonSize = WidgetTree->ConstructWidget<USizeBox>(); ButtonSize->SetMinDesiredHeight(62.f); ButtonSize->AddChild(UpgradeButton);
		Add(Center, ButtonSize, 0.f);
	}
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
	EnhancementStatsText->SetVisibility(ESlateVisibility::Collapsed);
	Add(Preview, EnhancementStatsText, 0.f);
	EnhancementSuccessSummary = Text(TEXT("EnhancementSuccessSummary"), FText::GetEmpty(), 26, Green);
	UBorder* SummaryFrame = Frame(TEXT("EnhancementSuccessFrame"));
	FSlateBrush InsetBrush; InsetBrush.SetResourceObject(EnhancementButtonTexture);
	InsetBrush.DrawAs = ESlateBrushDrawType::Box; InsetBrush.ImageSize = FVector2D(240.f, 80.f); InsetBrush.Margin = KnightMaintenancePresentation::ControlSlice;
	KnightMaintenancePresentation::SetControlUV(InsetBrush);
	InsetBrush.TintColor = FSlateColor(KnightMaintenancePresentation::ReadoutTint);
	SummaryFrame->SetBrush(InsetBrush); SummaryFrame->SetPadding(FMargin(36.f, 16.f));
	SummaryFrame->SetContent(EnhancementSuccessSummary); Add(Preview, SummaryFrame, 0.f);
	Columns->SetVisibility(ESlateVisibility::Collapsed);
}

UWidget* UKnightMaintenanceWidget::BuildEnhancementPreview()
{
	UOverlay* Preview = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("EnhancementPreviewLayers"));
	Preview->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>();
	IconSize->SetWidthOverride(320.f); IconSize->SetHeightOverride(180.f);
	EnhancementIconScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("EnhancementIconScale"));
	EnhancementIconScale->SetStretch(EStretch::ScaleToFit);
	EnhancementWeaponImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("EnhancementWeaponImage"));
	EnhancementWeaponImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	EnhancementIconScale->AddChild(EnhancementWeaponImage);
	IconSize->AddChild(EnhancementIconScale);
	UOverlaySlot* IconSlot = Preview->AddChildToOverlay(IconSize);
	IconSlot->SetHorizontalAlignment(HAlign_Center); IconSlot->SetVerticalAlignment(VAlign_Center);

	// The bars must NEVER participate in the icon's desired size / ScaleToFit calculation.
	// This zero-desired-size wrapper still fills the preview's allotted area, independently of its children.
	USizeBox* EffectArea = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("EnhancementEffectArea"));
	EffectArea->SetWidthOverride(0.f); EffectArea->SetHeightOverride(0.f);
	EnhancementBarCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("EnhancementBarCanvas"));
	EnhancementBarCanvas->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	EnhancementBarCanvas->SetVisibility(ESlateVisibility::Collapsed);
	EnhancementBars.Reserve(EnhancementBarCount);
	for (int32 Index = 0; Index < EnhancementBarCount; ++Index)
	{
		UImage* Bar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
			FName(*FString::Printf(TEXT("EnhancementBar_%03d"), Index)));
		Bar->SetBrush(FSlateColorBrush(FLinearColor::White));
		Bar->SetVisibility(ESlateVisibility::HitTestInvisible);
		UCanvasPanelSlot* BarSlot = EnhancementBarCanvas->AddChildToCanvas(Bar);
		BarSlot->SetAlignment(FVector2D(.5f, .5f));
		EnhancementBars.Add(Bar);
	}
	EffectArea->AddChild(EnhancementBarCanvas);
	UOverlaySlot* EffectSlot = Preview->AddChildToOverlay(EffectArea);
	EffectSlot->SetHorizontalAlignment(HAlign_Fill); EffectSlot->SetVerticalAlignment(VAlign_Fill);
	CaptureEnhancementIdleTransform();
	return Preview;
}

void UKnightMaintenanceWidget::CaptureEnhancementIdleTransform()
{
	if (EnhancementBarPhase != EEnhancementBarPhase::Idle || !EnhancementWeaponImage || !EnhancementIconScale) return;
	EnhancementIdleIconTransform = EnhancementWeaponImage->GetRenderTransform();
	EnhancementIdleScaleTransform = EnhancementIconScale->GetRenderTransform();
	EnhancementIdleIconPivot = EnhancementWeaponImage->GetRenderTransformPivot();
	EnhancementIdleScalePivot = EnhancementIconScale->GetRenderTransformPivot();
	bEnhancementIdleCaptured = true;
}

void UKnightMaintenanceWidget::RestoreEnhancementIdleTransform()
{
	if (!bEnhancementIdleCaptured || !EnhancementWeaponImage || !EnhancementIconScale) return;
	EnhancementWeaponImage->SetRenderTransform(EnhancementIdleIconTransform);
	EnhancementWeaponImage->SetRenderTransformPivot(EnhancementIdleIconPivot);
	EnhancementIconScale->SetRenderTransform(EnhancementIdleScaleTransform);
	EnhancementIconScale->SetRenderTransformPivot(EnhancementIdleScalePivot);
}

void UKnightMaintenanceWidget::ResetEnhancementBars()
{
	EnhancementBarPhase = EEnhancementBarPhase::Idle;
	EnhancementBarElapsed = 0.f;
	EnhancementBarPositions.Reset(); EnhancementBarOpacities.Reset();
	EnhancementBarResultPositions.Reset(); EnhancementBarResultOpacities.Reset();
	RestoreEnhancementIdleTransform();
	bEnhancementBarsSucceeded = false;
	if (EnhancementBarCanvas) EnhancementBarCanvas->SetVisibility(ESlateVisibility::Collapsed);
}

void UKnightMaintenanceWidget::StartEnhancementBars(EKnightWeaponUpgradeOutcome Outcome)
{
	if (Outcome == EKnightWeaponUpgradeOutcome::Rejected) return;
	CaptureEnhancementIdleTransform();
	ResetEnhancementBars();
	bEnhancementBarsSucceeded = Outcome == EKnightWeaponUpgradeOutcome::Success;
	EnhancementBarPhase = EEnhancementBarPhase::Rising;
	if (EnhancementBarCanvas) EnhancementBarCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	ApplyEnhancementBars();
}

void UKnightMaintenanceWidget::TickEnhancementBars(float DeltaTime)
{
	if (EnhancementBarPhase == EEnhancementBarPhase::Idle) return;
	RestoreEnhancementIdleTransform();
	EnhancementBarElapsed += FMath::Max(0.f, DeltaTime);
	switch (EnhancementBarPhase)
	{
	case EEnhancementBarPhase::Rising:
		if (EnhancementBarElapsed >= BarRiseDuration)
		{
			EnhancementBarElapsed = 0.f;
			EnhancementBarPhase = EEnhancementBarPhase::Trembling;
		}
		break;
	case EEnhancementBarPhase::Trembling:
		if (EnhancementBarElapsed >= BarTrembleDuration)
		{
			// Continue from the exact last displayed positions and opacity, without restarting the field.
			EnhancementBarResultPositions = EnhancementBarPositions;
			EnhancementBarResultOpacities = EnhancementBarOpacities;
			EnhancementBarElapsed = 0.f;
			EnhancementBarPhase = bEnhancementBarsSucceeded ? EEnhancementBarPhase::Success : EEnhancementBarPhase::Failure;
		}
		break;
	case EEnhancementBarPhase::Success:
	case EEnhancementBarPhase::Failure:
		if (EnhancementBarElapsed >= BarResultDuration)
		{
			ResetEnhancementBars();
			RefreshWeaponUpgradePanel();
			return;
		}
		break;
	default: break;
	}
	ApplyEnhancementBars();
}

void UKnightMaintenanceWidget::ApplyEnhancementBars()
{
	if (!EnhancementBarCanvas || EnhancementBarPhase == EEnhancementBarPhase::Idle) return;
	const FVector2D Size = EnhancementBarCanvas->GetCachedGeometry().GetLocalSize();
	if (Size.X <= 1.f || Size.Y <= 1.f || !EnhancementWeaponImage || !EnhancementWeaponImage->IsVisible()) return;
	EnhancementBarCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	EnhancementBarPositions.SetNum(EnhancementBars.Num());
	EnhancementBarOpacities.SetNum(EnhancementBars.Num());
	for (int32 Index = 0; Index < EnhancementBars.Num(); ++Index)
	{
		UImage* Bar = EnhancementBars[Index];
		UCanvasPanelSlot* BarSlot = Bar ? Cast<UCanvasPanelSlot>(Bar->Slot) : nullptr;
		if (!BarSlot) continue;
		FVector2D Position = BarAnchor(Index);
		float Opacity = 1.f;
		if (EnhancementBarPhase == EEnhancementBarPhase::Rising)
		{
			const float Delay = (Index % 7) * .011f;
			const float T = FMath::Clamp((EnhancementBarElapsed - Delay) / (BarRiseDuration - Delay), 0.f, 1.f);
			Position.Y = FMath::Lerp(1.08f + .025f * (Index % 5), Position.Y, EaseOutCubic(T));
			Opacity = FMath::Min(1.f, T * 2.f);
		}
		else if (EnhancementBarPhase == EEnhancementBarPhase::Trembling)
		{
			Position += BarJitter(Index, EnhancementBarElapsed);
			Opacity = .72f + .28f * ((Index + FMath::FloorToInt(EnhancementBarElapsed * (17.f + Index % 5))) % 3) / 2.f;
		}
		else
		{
			if (EnhancementBarResultPositions.IsValidIndex(Index)) Position = EnhancementBarResultPositions[Index];
			const float Delay = (Index % 6) * .009f;
			const float T = FMath::Clamp((EnhancementBarElapsed - Delay) / (BarResultDuration - Delay), 0.f, 1.f);
			Position.Y += EnhancementBarPhase == EEnhancementBarPhase::Success
				? -.98f * EaseOutCubic(T) : .98f * T * T;
			Opacity = (EnhancementBarResultOpacities.IsValidIndex(Index) ? EnhancementBarResultOpacities[Index] : 1.f) * (1.f - T);
		}
		EnhancementBarPositions[Index] = Position;
		EnhancementBarOpacities[Index] = Opacity;
		BarSlot->SetPosition(FVector2D(Position.X * Size.X, Position.Y * Size.Y));
		BarSlot->SetSize(FVector2D(Size.X * ((Index >= 126 ? .005f : .003f) + .0014f * (Index % 3)),
			Size.Y * ((Index >= 126 ? .055f : .045f) + .012f * (Index % 7))));
		FLinearColor Color = EnhancementBarPhase == EEnhancementBarPhase::Failure
			? FLinearColor(FColor(0xE5, 0x48, 0x4D)) : KnightMaintenancePresentation::Accent;
		Color.A = Opacity;
		Bar->SetColorAndOpacity(Color);
	}
}

#undef LOCTEXT_NAMESPACE
