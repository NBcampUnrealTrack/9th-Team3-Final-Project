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
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Progression/knightProgressionSubsystem.h"

#define LOCTEXT_NAMESPACE "KnightEnhancementPresentation"

namespace
{
	constexpr int32 EnhancementBarCount = 318;
	constexpr int32 FragmentPaletteCount = 24;
	constexpr float HorizontalSpread = .88f; // 12% narrower placement, never thinner/cropped fragments.
	constexpr float ChargeTop = 18.f;
	constexpr float ChargeBottom = 20.f;
	constexpr float TravelTop = 230.f;
	constexpr float TravelBottom = 260.f;
	constexpr float TransparentHold = .08f;

	float SmoothUnit(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	float EaseOutCubic(float T)
	{
		const float Remaining = 1.f - FMath::Clamp(T, 0.f, 1.f);
		return 1.f - Remaining * Remaining * Remaining;
	}

	FVector2D BarAnchor(int32 Index)
	{
		// Fixed sunflower distribution: dense center, sparse irregular perimeter, no rectangular rows.
		// This is presentation-only and never consumes gameplay randomness.
		const float Fraction = (Index + .5f) / EnhancementBarCount;
		const float RadiusX = FMath::Pow(Fraction, .95f);
		const float RadiusY = FMath::Pow(Fraction, .48f);
		const float Angle = Index * 2.39996323f;
		const float Edge = .90f + .10f * FMath::Sin(Index * 7.13f);
		return FVector2D(.5f + HorizontalSpread * .47f * RadiusX * FMath::Cos(Angle) * Edge,
			.5f + .46f * RadiusY * FMath::Sin(Angle));
	}

	FVector2D BarJitter(int32 Index, float Time)
	{
		const int32 Beat = FMath::FloorToInt((Time + Index * .017f) * (23.f + (Index % 6) * 5.f));
		return FVector2D(HorizontalSpread * (Beat % 4 == 0 ? .002f : 0.f) * ((Beat + Index * 2) % 3 - 1),
			.009f * ((Beat * 2 + Index) % 3 - 1));
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
	// Keep the original layout footprint. Actual icon/VFX paint outside the scrolling text hierarchy.
	USpacer* PreviewAnchor = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("EnhancementPreviewAnchor"));
	PreviewAnchor->SetSize(FVector2D(320.f, 180.f));
	EnhancementPreviewAnchor = PreviewAnchor;
	IconFrame->SetContent(PreviewAnchor);
	Add(UpgradePanel, IconFrame, 12.f)->SetHorizontalAlignment(HAlign_Fill);
	EnhancementPreviewHost = BuildEnhancementPreview();
	UCanvasPanelSlot* VisualSlot = Root->AddChildToCanvas(EnhancementPreviewHost);
	VisualSlot->SetZOrder(0); VisualSlot->SetSize(FVector2D::ZeroVector);
	EnhancementPreviewHost->SetVisibility(ESlateVisibility::Hidden);
	// Keep live numbers away from the apparatus's luminous ring on a recessed raster surface.
	UBorder* Readout = Frame(TEXT("EnhancementReadout"));
	FSlateBrush ReadoutBrush; ReadoutBrush.SetResourceObject(EnhancementButtonTexture);
	ReadoutBrush.DrawAs = ESlateBrushDrawType::Box; ReadoutBrush.Margin = KnightMaintenancePresentation::ControlSlice;
	KnightMaintenancePresentation::SetControlUV(ReadoutBrush);
	ReadoutBrush.TintColor = FSlateColor(KnightMaintenancePresentation::ReadoutTint);
	// Preserve the panel's original artwork below the VFX, with its text above the VFX.
	// Its separate clip host mirrors the ScrollBox viewport, including when the readout is scrolled.
	Readout->SetBrush(NoFrame); Readout->SetPadding(FMargin(36.f, 14.f));
	EnhancementReadoutAnchor = Readout;
	EnhancementReadoutBackdropLayer = WidgetTree->ConstructWidget<UCanvasPanel>();
	EnhancementReadoutBackdropLayer->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	EnhancementReadoutBackdropLayer->SetVisibility(ESlateVisibility::Hidden);
	Root->AddChildToCanvas(EnhancementReadoutBackdropLayer)->SetZOrder(-1);
	EnhancementReadoutBackdrop = WidgetTree->ConstructWidget<UImage>();
	EnhancementReadoutBackdrop->SetBrush(ReadoutBrush);
	EnhancementReadoutBackdropLayer->AddChildToCanvas(EnhancementReadoutBackdrop);
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
	UpgradeButton = ButtonClass && GetOwningPlayer() ? CreateWidget<UCommonButtonBase>(GetOwningPlayer(), ButtonClass, TEXT("Phase7_UpgradeButton")) : nullptr;
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
	Preview->SetClipping(EWidgetClipping::Inherit);
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
	// A restrained soft central light, behind the individual fragments but in front of the sword.
	if (EnhancementFieldGlowMaterial)
	{
		EnhancementFieldGlow = WidgetTree->ConstructWidget<UImage>();
		EnhancementGlowMID = UMaterialInstanceDynamic::Create(EnhancementFieldGlowMaterial, this);
		EnhancementFieldGlow->SetBrushFromMaterial(EnhancementGlowMID);
		EnhancementFieldGlow->SetRenderOpacity(0.f);
		EnhancementFieldGlow->SetVisibility(ESlateVisibility::HitTestInvisible);
		UCanvasPanelSlot* GlowSlot = EnhancementBarCanvas->AddChildToCanvas(EnhancementFieldGlow);
		GlowSlot->SetAnchors(FAnchors(0.f, 0.f));
	}
	// Shared palettes avoid hundreds of per-frame material parameter updates/draw states.
	for (int32 Index = 0; Index < FragmentPaletteCount; ++Index)
	{
		UMaterialInterface* Parent = Index % 3 == 0 ? EnhancementEchoMaterial : EnhancementFragmentMaterial;
		UMaterialInstanceDynamic* MID = Parent ? UMaterialInstanceDynamic::Create(Parent, this) : nullptr;
		float CoreLength = .24f, TrailLength = .16f, TrailOpacity = .65f;
		if (MID)
		{
			Parent->GetScalarParameterValue(TEXT("CoreLength"), CoreLength);
			Parent->GetScalarParameterValue(TEXT("TrailLength"), TrailLength);
			Parent->GetScalarParameterValue(TEXT("TrailOpacity"), TrailOpacity);
			MID->SetScalarParameterValue(TEXT("CoreLength"), CoreLength * (.70f + .12f * (Index % 6)));
		}
		EnhancementTrailLengths.Add(TrailLength);
		EnhancementTrailOpacities.Add(TrailOpacity);
		EnhancementFragmentPalettes.Add(MID);
	}
	EnhancementBars.Reserve(EnhancementBarCount);
	for (int32 Index = 0; Index < EnhancementBarCount; ++Index)
	{
		UImage* Bar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
			FName(*FString::Printf(TEXT("EnhancementBar_%03d"), Index)));
		Bar->SetBrushFromMaterial(EnhancementFragmentPalettes[Index % FragmentPaletteCount]);
		Bar->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.f));
		Bar->SetVisibility(EnhancementFragmentPalettes[Index % FragmentPaletteCount] ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		UCanvasPanelSlot* BarSlot = EnhancementBarCanvas->AddChildToCanvas(Bar);
		BarSlot->SetAlignment(FVector2D(.5f, .5f));
		EnhancementBars.Add(Bar);
	}
	EffectArea->AddChild(EnhancementBarCanvas);
	UOverlaySlot* EffectSlot = Preview->AddChildToOverlay(EffectArea);
	EffectSlot->SetHorizontalAlignment(HAlign_Fill); EffectSlot->SetVerticalAlignment(VAlign_Fill);
	// Travel space is independent of the 218px charging field and fragment quad dimensions.
	// The host sits outside the ScrollBox, behind the existing foreground name/numbers/readout text.
	EffectSlot->SetPadding(FMargin(0.f, -TravelTop, 0.f, -TravelBottom));
	CaptureEnhancementIdleTransform();
	return Preview;
}

void UKnightMaintenanceWidget::SyncEnhancementVisualLayout()
{
	if (!EnhancementPreviewAnchor || !EnhancementPreviewHost || !WidgetTree || !WidgetTree->RootWidget) return;
	const FGeometry Root = WidgetTree->RootWidget->GetCachedGeometry();
	const FGeometry Anchor = EnhancementPreviewAnchor->GetCachedGeometry();
	const bool bVisible = EnhancementColumns && EnhancementColumns->IsVisible()
		&& Root.GetLocalSize().X > 1.f && Anchor.GetLocalSize().Y > 1.f;
	EnhancementPreviewHost->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	if (EnhancementReadoutBackdropLayer)
		EnhancementReadoutBackdropLayer->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	if (!bVisible) return;
	auto Origin = [&Root](const FGeometry& G) { return Root.AbsoluteToLocal(G.LocalToAbsolute(FVector2D::ZeroVector)); };
	auto Extent = [&Root, &Origin](const FGeometry& G) { return Root.AbsoluteToLocal(G.LocalToAbsolute(G.GetLocalSize())) - Origin(G); };
	if (auto* PreviewSlot = Cast<UCanvasPanelSlot>(EnhancementPreviewHost->Slot))
	{
		PreviewSlot->SetPosition(Origin(Anchor)); PreviewSlot->SetSize(Extent(Anchor));
	}
	if (EnhancementReadoutBackdropLayer && EnhancementReadoutBackdrop && EnhancementReadoutAnchor && EnhancementDetailPage)
	{
		const FGeometry Viewport = EnhancementDetailPage->GetCachedGeometry();
		const FGeometry Readout = EnhancementReadoutAnchor->GetCachedGeometry();
		auto* ClipSlot = CastChecked<UCanvasPanelSlot>(EnhancementReadoutBackdropLayer->Slot);
		ClipSlot->SetPosition(Origin(Viewport)); ClipSlot->SetSize(Extent(Viewport));
		auto* ImageSlot = CastChecked<UCanvasPanelSlot>(EnhancementReadoutBackdrop->Slot);
		ImageSlot->SetPosition(Origin(Readout) - Origin(Viewport)); ImageSlot->SetSize(Extent(Readout));
	}
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
	// A newly shown canvas may not have a cached layout until its first paint. Never expose old/default tints.
	for (UImage* Bar : EnhancementBars)
		if (Bar) Bar->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.f));
	if (EnhancementFieldGlow) EnhancementFieldGlow->SetRenderOpacity(0.f);
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
	SyncEnhancementVisualLayout();
	if (EnhancementBarPhase == EEnhancementBarPhase::Idle) return;
	RestoreEnhancementIdleTransform();
	EnhancementBarElapsed += FMath::Max(0.f, DeltaTime);
	switch (EnhancementBarPhase)
	{
	case EEnhancementBarPhase::Rising:
		if (EnhancementBarElapsed >= FMath::Max(.05f, EnhancementGatherDuration))
		{
			EnhancementBarElapsed = 0.f;
			EnhancementBarPhase = EEnhancementBarPhase::Trembling;
		}
		break;
	case EEnhancementBarPhase::Trembling:
		if (EnhancementBarElapsed >= FMath::Max(.05f, EnhancementHoldDuration))
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
		if (EnhancementBarElapsed >= FMath::Max(.05f, EnhancementReleaseDuration))
		{
			// Submit an explicitly transparent frame before any hide/reset. Even a long frame cannot skip it.
			EnhancementBarElapsed = FMath::Max(.05f, EnhancementReleaseDuration);
			ApplyEnhancementBars();
			EnhancementBarPhase = EEnhancementBarPhase::Transparent;
			EnhancementBarElapsed = 0.f;
			return;
		}
		break;
	case EEnhancementBarPhase::Transparent:
		if (EnhancementBarElapsed >= TransparentHold)
		{
			ResetEnhancementBars();
			RefreshWeaponUpgradePanel();
		}
		return;
	default: break;
	}
	ApplyEnhancementBars();
}

void UKnightMaintenanceWidget::ApplyEnhancementBars()
{
	if (!EnhancementBarCanvas || EnhancementBarPhase == EEnhancementBarPhase::Idle || EnhancementBarPhase == EEnhancementBarPhase::Transparent) return;
	const FVector2D Size = EnhancementBarCanvas->GetCachedGeometry().GetLocalSize();
	if (Size.X <= 1.f || Size.Y <= 1.f || !EnhancementWeaponImage || !EnhancementWeaponImage->IsVisible()) return;
	const FVector2D ChargeSize(Size.X, Size.Y - TravelTop - TravelBottom + ChargeTop + ChargeBottom);
	if (ChargeSize.Y <= 1.f) return;
	const FVector2D ChargeOrigin(0.f, TravelTop - ChargeTop);
	EnhancementBarCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	EnhancementBarPositions.SetNum(EnhancementBars.Num());
	EnhancementBarOpacities.SetNum(EnhancementBars.Num());
	const bool bReleasing = EnhancementBarPhase == EEnhancementBarPhase::Success || EnhancementBarPhase == EEnhancementBarPhase::Failure;
	const float Release = bReleasing ? FMath::Clamp(EnhancementBarElapsed / FMath::Max(.05f, EnhancementReleaseDuration), 0.f, 1.f) : 0.f;
	const float Red = EnhancementBarPhase == EEnhancementBarPhase::Failure ? FMath::Clamp(EnhancementBarElapsed / .09f, 0.f, 1.f) : 0.f;
	const FLinearColor CoreColor = FMath::Lerp(FLinearColor(1.f, .62f, .12f), FLinearColor(1.f, .022f, .008f), Red);
	const FLinearColor GlowColor = FMath::Lerp(FLinearColor(1.f, .19f, .008f), FLinearColor(1.f, .003f, .001f), Red);
	for (int32 Palette = 0; Palette < EnhancementFragmentPalettes.Num(); ++Palette)
	{
		if (UMaterialInstanceDynamic* MID = EnhancementFragmentPalettes[Palette])
		{
			MID->SetVectorParameterValue(TEXT("CoreColor"), CoreColor);
			MID->SetVectorParameterValue(TEXT("GlowColor"), GlowColor);
			MID->SetScalarParameterValue(TEXT("TrailDirection"), 1.f - 2.f * Red);
			const float GatherTrail = EnhancementBarPhase == EEnhancementBarPhase::Rising ? .09f * (1.f - EaseOutCubic(EnhancementBarElapsed / FMath::Max(.05f, EnhancementGatherDuration))) : 0.f;
			MID->SetScalarParameterValue(TEXT("TrailLength"), EnhancementTrailLengths[Palette] * (.65f + .075f * (Palette % 4) + GatherTrail * 6.f + 1.4f * Release * Release));
			MID->SetScalarParameterValue(TEXT("TrailOpacity"), EnhancementTrailOpacities[Palette] * (1.f - .65f * Release));
		}
	}
	if (EnhancementGlowMID && EnhancementFieldGlow)
	{
		EnhancementGlowMID->SetVectorParameterValue(TEXT("GlowColor"), GlowColor);
		const float Gather = EnhancementBarPhase == EEnhancementBarPhase::Rising ? EaseOutCubic(EnhancementBarElapsed / FMath::Max(.05f, EnhancementGatherDuration)) : 1.f;
		EnhancementFieldGlow->SetRenderOpacity(Gather * FMath::Square(1.f - SmoothUnit(Release)));
		if (auto* GlowSlot = Cast<UCanvasPanelSlot>(EnhancementFieldGlow->Slot))
		{
			GlowSlot->SetPosition(ChargeOrigin + FVector2D(Size.X * (1.f - HorizontalSpread) * .5f, 0.f));
			GlowSlot->SetSize(FVector2D(Size.X * HorizontalSpread, ChargeSize.Y));
		}
	}
	for (int32 Index = 0; Index < EnhancementBars.Num(); ++Index)
	{
		UImage* Bar = EnhancementBars[Index];
		UCanvasPanelSlot* BarSlot = Bar ? Cast<UCanvasPanelSlot>(Bar->Slot) : nullptr;
		if (!BarSlot) continue;
		FVector2D Position = BarAnchor(Index);
		float Opacity = 1.f;
		if (EnhancementBarPhase == EEnhancementBarPhase::Rising)
		{
			const float Duration = FMath::Max(.05f, EnhancementGatherDuration);
			const float Delay = (Index % 9) * Duration * .025f;
			const float T = FMath::Clamp((EnhancementBarElapsed - Delay) / (Duration - Delay), 0.f, 1.f);
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
			const float Duration = FMath::Max(.05f, EnhancementReleaseDuration);
			const float Lifetime = Duration * (.76f + .24f * ((Index * 7) % 11) / 10.f);
			const float T = FMath::Clamp(EnhancementBarElapsed / Lifetime, 0.f, 1.f);
			Position.Y += EnhancementBarPhase == EEnhancementBarPhase::Success
				? -1.65f * (.2f * T + .8f * T * T) : 1.65f * (.2f * T + .8f * T * T);
			Opacity = (EnhancementBarResultOpacities.IsValidIndex(Index) ? EnhancementBarResultOpacities[Index] : 1.f)
				* FMath::Pow(1.f - SmoothUnit(T), 1.3f);
		}
		const FVector2D PixelPosition = ChargeOrigin + FVector2D(Position.X * ChargeSize.X, Position.Y * ChargeSize.Y);
		const FVector2D QuadSize(ChargeSize.X * (.018f + .005f * (Index % 4)), ChargeSize.Y * (.24f + .035f * (Index % 6)));
		if (!bReleasing)
			Opacity *= FMath::Square(FMath::Clamp(FMath::Min(Position.Y, 1.f - Position.Y) / .14f, 0.f, 1.f));
		// Fade the COMPLETE padded quad before any core, halo or trail can reach a hard clip edge.
		const float EdgeDistance = FMath::Min(PixelPosition.Y - QuadSize.Y * .5f, Size.Y - PixelPosition.Y - QuadSize.Y * .5f);
		Opacity *= SmoothUnit(EdgeDistance / (ChargeSize.Y * .36f));
		EnhancementBarPositions[Index] = Position;
		EnhancementBarOpacities[Index] = Opacity;
		BarSlot->SetPosition(PixelPosition);
		// Padded quads contain a narrow bright core, a softer halo and room for a directional trail.
		BarSlot->SetSize(QuadSize);
		const float EdgeFade = 1.f - .58f * FMath::Pow(static_cast<float>(Index) / EnhancementBarCount, 1.5f);
		Bar->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, Opacity * EdgeFade));
	}
}

#undef LOCTEXT_NAMESPACE
