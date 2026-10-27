#include "Menu/UI/KnightMaintenanceWidget.h"
#include "Menu/UI/KnightMaintenancePresentation.h"
#include "CommonTextBlock.h"
#include "Menu/UI/KnightMaintenanceEntryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Item/knightItemDefinitions.h"

namespace
{
	FSlateBrush PaintedSurface(UTexture2D* Texture, FLinearColor Tint)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.DrawAs = Texture ? ESlateBrushDrawType::Box : ESlateBrushDrawType::NoDrawType;
		Brush.ImageSize = FVector2D(512.f, 170.f);
		Brush.Margin = KnightMaintenancePresentation::ControlSlice;
		KnightMaintenancePresentation::SetControlUV(Brush);
		Brush.TintColor = FSlateColor(Tint);
		return Brush;
	}

	void RemoveFlatFrame(UBorder* Border)
	{
		if (!Border) return;
		FSlateBrush Brush; Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
		Border->SetBrush(Brush);
	}
}

void UKnightMaintenanceWidget::ApplyCraftingPresentation()
{
	if (!WidgetTree) return;
	// Neutralize static legacy blue labels/dividers only in this widget tree.
	// Dynamic item-rarity and stat-delta colors are assigned by their existing handlers.
	WidgetTree->ForEachWidget([](UWidget* Widget)
	{
		if (UTextBlock* Text = Cast<UTextBlock>(Widget))
		{
			const FLinearColor Old = Text->GetColorAndOpacity().GetSpecifiedColor();
			if (Old.B > Old.R * 1.08f)
			{
				if (UCommonTextBlock* Common = Cast<UCommonTextBlock>(Text)) Common->SetStyle(nullptr);
				Text->SetColorAndOpacity(FSlateColor(KnightMaintenancePresentation::Secondary));
			}
		}
		if (UImage* Line = Cast<UImage>(Widget))
			if (Line->GetName().Contains(TEXT("Line"))) Line->SetColorAndOpacity(KnightMaintenancePresentation::Divider);
	});
	// Restyle only the Crafting-owned subtree. Enhancement, Socket, footer and shared
	// button CDOs retain their existing presentation and interaction configuration.
	for (const TCHAR* Name : { TEXT("Panel_Left"), TEXT("Panel_Right"), TEXT("VerticalBox_Confirm") })
		if (UWidget* Subtree = WidgetTree->FindWidget(Name))
			UWidgetTree::ForWidgetAndChildren(Subtree, [this](UWidget* Widget)
			{
				if (UCommonButtonBase* Button = Cast<UCommonButtonBase>(Widget))
				{
					const bool bMainTab = Button == Btn_Craft || Button == Btn_Weapon || Button == Btn_Armor;
					const auto Style = bMainTab ? CraftingMainTabStyle : CraftingControlStyle;
					if (Style) Button->SetStyle(Style);
				}
			});
	if (UBorder* Modal = Cast<UBorder>(WidgetTree->FindWidget(TEXT("Border_Confirm"))))
	{
		Modal->SetBrush(PaintedSurface(CraftingButtonTexture, KnightMaintenancePresentation::ReadoutTint));
		Modal->SetPadding(FMargin(36.f));
	}
	// No nested blue frame under the selected item's real artwork.
	if (UBorder* ImageBorder = Cast<UBorder>(WidgetTree->FindWidget(TEXT("Border_SelectedImage"))))
	{
		RemoveFlatFrame(ImageBorder);
		if (Img_Selected && Img_Selected->GetParent() == ImageBorder)
		{
			Img_Selected->RemoveFromParent();
			UScaleBox* Fit = WidgetTree->ConstructWidget<UScaleBox>();
			Fit->SetStretch(EStretch::ScaleToFit);
			Fit->SetVisibility(ESlateVisibility::HitTestInvisible);
			ImageBorder->SetContent(Fit); Fit->AddChild(Img_Selected);
		}
	}
	if (MaterialQuantityInput && CraftingButtonTexture)
	{
		FEditableTextBoxStyle Style = MaterialQuantityInput->GetWidgetStyle();
		Style.BackgroundImageNormal = Style.BackgroundImageReadOnly =
			PaintedSurface(CraftingButtonTexture, FLinearColor(.30f, .30f, .30f, 1.f));
		Style.BackgroundImageHovered = Style.BackgroundImageFocused =
			PaintedSurface(CraftingButtonTexture, FLinearColor(.70f, .70f, .70f, 1.f));
		MaterialQuantityInput->SetWidgetStyle(Style);
	}
}

void UKnightMaintenanceWidget::StyleCraftingEntry(UKnightMaintenanceEntryWidget* Entry)
{
	if (!Entry || !Entry->WidgetTree) return;
	if (CraftingRowStyle) Entry->SetStyle(CraftingRowStyle);
	RemoveFlatFrame(Cast<UBorder>(Entry->GetWidgetFromName(TEXT("Border_Badge"))));
	if (UTextBlock* Name = Cast<UTextBlock>(Entry->GetWidgetFromName(TEXT("Txt_Name"))))
		Name->SetAutoWrapText(true);
	UImage* Icon = Cast<UImage>(Entry->GetWidgetFromName(TEXT("Img_Icon")));
	UTexture2D* Texture = Entry->GetPart() ? Entry->GetPart()->Icon.LoadSynchronous() : nullptr;
	if (USizeBox* Size = Cast<USizeBox>(Entry->GetWidgetFromName(TEXT("IconFrame"))))
	{
		Size->SetWidthOverride(60.f); Size->SetHeightOverride(60.f);
		Size->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Icon)
		if (UBorder* IconBorder = Cast<UBorder>(Icon->GetParent()))
		{
			RemoveFlatFrame(IconBorder);
			Icon->RemoveFromParent();
			UScaleBox* Fit = Entry->WidgetTree->ConstructWidget<UScaleBox>(); Fit->SetStretch(EStretch::ScaleToFit);
			Fit->SetVisibility(ESlateVisibility::HitTestInvisible);
			IconBorder->SetContent(Fit); Fit->AddChild(Icon);
			if (Texture) Icon->SetBrushFromTexture(Texture, true);
		}
}
