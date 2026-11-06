#include "Menu/UI/KnightMaintenanceStateExtension.h"
#include "Menu/UI/KnightMaintenanceWidget.h"
#include "Menu/UI/KnightMaintenancePresentation.h"
#include "Menu/UI/KnightButtonFillExtension.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/ContentWidget.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

void UKnightMaintenanceStateExtension::Enable(UCommonButtonBase* Button)
{
	if (!Button) return;
	UKnightButtonFillExtension::SetEnabled(Button, false);
	Button->GetOrAddExtension<UKnightMaintenanceStateExtension>().EnsureLayers();
}

void UKnightMaintenanceStateExtension::SetEquipped(UCommonButtonBase* Button, bool bInEquipped)
{
	Enable(Button);
	if (Button) Button->GetOrAddExtension<UKnightMaintenanceStateExtension>().bEquipped = bInEquipped;
}

void UKnightMaintenanceStateExtension::NavigationFocus(UCommonButtonBase* Button)
{
	for (UWidget* Parent = Button ? Button->GetParent() : nullptr; Parent; Parent = Parent->GetParent())
		if (UKnightMaintenanceWidget* Screen = Parent->GetTypedOuter<UKnightMaintenanceWidget>())
		{
			Screen->NotifyNavigationInput();
			break;
		}
}

void UKnightMaintenanceStateExtension::SetFocusOutlineEnabled(UCommonButtonBase* Button, bool bEnabled)
{
	Enable(Button);
	if (Button) Button->GetOrAddExtension<UKnightMaintenanceStateExtension>().bDrawFocusOutline = bEnabled;
}

void UKnightMaintenanceStateExtension::Construct()
{
	Super::Construct();
	EnsureLayers();
}

void UKnightMaintenanceStateExtension::EnsureLayers()
{
	if (SelectionEdge) return;
	UUserWidget* Button = GetUserWidget();
	UWidgetTree* Tree = Button ? Button->WidgetTree : nullptr;
	UContentWidget* Root = Tree ? Cast<UContentWidget>(Tree->RootWidget) : nullptr;
	UWidget* Content = Root ? Root->GetContent() : nullptr;
	if (!Content) return;
	const UButtonSlot* OriginalSlot = Cast<UButtonSlot>(Content->Slot);
	const FMargin OriginalPadding = OriginalSlot ? OriginalSlot->GetPadding() : FMargin(0.f);
	UOverlay* Layers = Tree->ConstructWidget<UOverlay>();
	Root->SetContent(Layers);
	if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Layers->Slot))
	{
		ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
		ButtonSlot->SetVerticalAlignment(VAlign_Fill);
		ButtonSlot->SetPadding(OriginalPadding);
	}
	UOverlaySlot* ContentSlot = Layers->AddChildToOverlay(Content);
	ContentSlot->SetHorizontalAlignment(HAlign_Fill);
	ContentSlot->SetVerticalAlignment(VAlign_Fill);
	// Adorners contribute zero desired size. Existing content alone determines row
	// dimensions; a short label never collapses the indicator to the text's bounds.
	USizeBox* AdornerFrame = Tree->ConstructWidget<USizeBox>();
	AdornerFrame->SetWidthOverride(0.f); AdornerFrame->SetHeightOverride(0.f);
	AdornerFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* AdornerSlot = Layers->AddChildToOverlay(AdornerFrame);
	AdornerSlot->SetHorizontalAlignment(HAlign_Fill); AdornerSlot->SetVerticalAlignment(VAlign_Fill);
	UCanvasPanel* Adorners = Tree->ConstructWidget<UCanvasPanel>();
	AdornerFrame->SetContent(Adorners);
	SelectionEdge = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MaintenanceSelectionEdge"));
	FSlateBrush Edge = *FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	Edge.ImageSize = FVector2D(3.f, 1.f);
	SelectionEdge->SetBrush(Edge);
	SelectionEdge->SetColorAndOpacity(KnightMaintenancePresentation::Accent);
	UCanvasPanelSlot* EdgeSlot = Adorners->AddChildToCanvas(SelectionEdge);
	EdgeSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 1.f));
	EdgeSlot->SetOffsets(FMargin(0.f, 8.f, 3.f, 8.f));
	FocusOutline = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MaintenanceFocusOutline"));
	FSlateBrush Outline;
	Outline.DrawAs = ESlateBrushDrawType::RoundedBox;
	Outline.TintColor = FSlateColor(FLinearColor::Transparent);
	Outline.OutlineSettings = FSlateBrushOutlineSettings(FVector4(0.f), FSlateColor(FLinearColor(FColor(174, 176, 178))), 1.f);
	Outline.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	FocusOutline->SetBrush(Outline);
	UCanvasPanelSlot* FocusSlot = Adorners->AddChildToCanvas(FocusOutline);
	FocusSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	FocusSlot->SetOffsets(FMargin(0.f));
	// Overlay-only badge: no changing row width, text alignment or hit region.
	EquippedLabel = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MaintenanceEquippedLabel"));
	if (UTextBlock* Name = Cast<UTextBlock>(Tree->FindWidget(TEXT("Txt_Label"))))
	{
		FSlateFontInfo Font = Name->GetFont(); Font.Size = 12; EquippedLabel->SetFont(Font);
	}
	else if (UTextBlock* ItemName = Cast<UTextBlock>(Tree->FindWidget(TEXT("Txt_Name"))))
	{
		FSlateFontInfo Font = ItemName->GetFont(); Font.Size = 12; EquippedLabel->SetFont(Font);
	}
	EquippedLabel->SetText(NSLOCTEXT("KnightMaintenance", "ActuallyEquipped", "장착 중"));
	EquippedLabel->SetColorAndOpacity(FLinearColor(FColor(187, 207, 199)));
	UCanvasPanelSlot* BadgeSlot = Adorners->AddChildToCanvas(EquippedLabel);
	BadgeSlot->SetAnchors(FAnchors(1.f, 0.f));
	BadgeSlot->SetAlignment(FVector2D(1.f, 0.f));
	BadgeSlot->SetPosition(FVector2D(-6.f, 0.f));
	BadgeSlot->SetAutoSize(true);
	for (UWidget* Layer : { static_cast<UWidget*>(SelectionEdge), static_cast<UWidget*>(FocusOutline), static_cast<UWidget*>(EquippedLabel) })
		Layer->SetVisibility(ESlateVisibility::Hidden);
}

void UKnightMaintenanceStateExtension::Tick(const FGeometry& Geometry, float DeltaTime)
{
	EnsureLayers();
	UCommonButtonBase* Button = Cast<UCommonButtonBase>(GetUserWidget());
	if (!Button || !SelectionEdge) return;
	const UKnightMaintenanceWidget* Screen = nullptr;
	for (UWidget* Parent = Button->GetParent(); Parent && !Screen; Parent = Parent->GetParent())
		Screen = Parent->GetTypedOuter<UKnightMaintenanceWidget>();
	const bool bNavigation = !Screen || Screen->UsesNavigationFeedback();
	const bool bInteractive = Button->GetIsEnabled() && !Button->GetLocked();
	const bool bFocus = bNavigation && (Button->HasAnyUserFocus() || Button->HasFocusedDescendants());
	const bool bHot = bInteractive && (bNavigation ? bFocus || (!bDrawFocusOutline && Button->IsHovered()) : Button->IsHovered());
	SelectionEdge->SetVisibility(Button->GetSelected() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	FocusOutline->SetVisibility(bDrawFocusOutline && bInteractive && bFocus ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	EquippedLabel->SetVisibility(bEquipped ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	const FLinearColor Color(FColor(!bInteractive ? 153 : bHot ? 244 : 205, !bInteractive ? 155 : bHot ? 244 : 207, !bInteractive ? 158 : bHot ? 242 : 210));
	for (const TCHAR* Name : { TEXT("Txt_Label"), TEXT("Txt_Name") })
		if (UTextBlock* Text = Cast<UTextBlock>(Button->GetWidgetFromName(Name)))
		{
			if (UCommonTextBlock* Styled = Cast<UCommonTextBlock>(Text)) Styled->SetStyle(nullptr);
			Text->SetColorAndOpacity(Color);
		}
	if (UImage* Icon = Cast<UImage>(Button->GetWidgetFromName(TEXT("Img_Icon"))))
		Icon->SetColorAndOpacity(FLinearColor(bHot ? 1.f : .82f, bHot ? 1.f : .82f, bHot ? 1.f : .82f, bInteractive ? 1.f : .75f));
}
