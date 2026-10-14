#include "Menu/UI/KnightMaintenanceEntryWidget.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Item/knightEquipmentData.h"

#define LOCTEXT_NAMESPACE "KnightMaintenanceEntry"

TSubclassOf<UKnightMaintenanceEntryWidget> UKnightMaintenanceEntryWidget::GetSharedWidgetClass()
{
	return LoadClass<UKnightMaintenanceEntryWidget>(nullptr,
		TEXT("/Game/Knight/UI/Menu/Maintenance/WBP_KnightMaintenanceEntry.WBP_KnightMaintenanceEntry_C"));
}

void UKnightMaintenanceEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsSelectable(true);
}

void UKnightMaintenanceEntryWidget::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	OnEntryFocused.ExecuteIfBound(this);
}

void UKnightMaintenanceEntryWidget::SetEntry(const UknightPartItemDefinition* InPart,
	const UknightCraftRecipeDefinition* InRecipe, EKnightMaintenanceEntryState State, int32 OwnedCount)
{
	Part = InPart;
	Recipe = InRecipe;
	if (!Part) return;
	if (Txt_Name) Txt_Name->SetText(Part->DisplayName.IsEmpty() ? FText::FromString(Part->GetName()) : Part->DisplayName);

	// 장착 중 주황, 가능 하늘색, 불가 흐림 (스타일 가이드).
	FText Label;
	FLinearColor Color;
	switch (State)
	{
	case EKnightMaintenanceEntryState::Equipped:
		Label = LOCTEXT("Equipped", "장착 중");
		Color = FLinearColor(FColor(0xFF, 0x7A, 0x1A));
		break;
	case EKnightMaintenanceEntryState::Craftable:
		Label = LOCTEXT("Craftable", "제작 가능");
		Color = FLinearColor(FColor(0xA8, 0xC8, 0xF0));
		break;
	case EKnightMaintenanceEntryState::MissingMaterials:
		Label = LOCTEXT("MissingMaterials", "재료 부족");
		Color = FLinearColor(FColor(0x4A, 0x5B, 0x75));
		break;
	default:
		Label = FText::Format(LOCTEXT("Owned", "보유 {0}"), FText::AsNumber(OwnedCount));
		Color = FLinearColor(FColor(0x8F, 0xA3, 0xBF));
		break;
	}
	if (Txt_State)
	{
		Txt_State->SetText(Label);
		Txt_State->SetColorAndOpacity(FSlateColor(Color));
	}
	if (Border_Badge)
	{
		// 배지 색 1px 테두리, 채우기 없음.
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor::Transparent);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(0.f, 0.f, 0.f, 0.f), FSlateColor(Color), 1.f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Border_Badge->SetBrush(Brush);
	}
	if (Img_Icon)
	{
		if (UTexture2D* Icon = Part->Icon.LoadSynchronous())
		{
			Img_Icon->SetBrushFromTexture(Icon);
			Img_Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else Img_Icon->SetVisibility(ESlateVisibility::Hidden);
	}
}

#undef LOCTEXT_NAMESPACE
