#include "Menu/UI/KnightMaintenanceEntryWidget.h"
#include "Menu/UI/KnightMaintenancePresentation.h"
#include "Menu/UI/KnightMaintenanceStateExtension.h"
#include "CommonTextBlock.h"
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
	UKnightMaintenanceStateExtension::Enable(this);
	SetIsSelectable(true);
	SetIsInteractableWhenSelected(true);
	bTriggerClickedAfterSelection = true;
	SetShouldSelectUponReceivingFocus(false);
	if (const TSubclassOf<UCommonButtonStyle> StyleClass = LoadClass<UCommonButtonStyle>(nullptr,
		TEXT("/Game/Knight/UI/Style/BS_Knight_CraftingRow.BS_Knight_CraftingRow_C"))) SetStyle(StyleClass);
}

void UKnightMaintenanceEntryWidget::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	if (InFocusEvent.GetCause() == EFocusCause::Navigation) UKnightMaintenanceStateExtension::NavigationFocus(this);
	OnEntryFocused.ExecuteIfBound(this);
}

void UKnightMaintenanceEntryWidget::SetEntry(const UknightPartItemDefinition* InPart,
	const UknightCraftRecipeDefinition* InRecipe, EKnightMaintenanceEntryState State, int32 OwnedCount)
{
	Part = InPart;
	Recipe = InRecipe;
	if (!Part) return;
	if (Txt_Name) Txt_Name->SetText(Part->DisplayName.IsEmpty() ? FText::FromString(Part->GetName()) : Part->DisplayName);

	// 장착 상태는 강조색, 보유/제작 안내는 중립색으로 표시한다.
	FText Label;
	FLinearColor Color;
	switch (State)
	{
	case EKnightMaintenanceEntryState::Equipped:
		Label = LOCTEXT("Equipped", "장착 중");
		Color = KnightMaintenancePresentation::Accent;
		break;
	case EKnightMaintenanceEntryState::Craftable:
		Label = LOCTEXT("Craftable", "제작 가능");
		Color = KnightMaintenancePresentation::Secondary;
		break;
	case EKnightMaintenanceEntryState::MissingMaterials:
		Label = LOCTEXT("MissingMaterials", "재료 부족");
		Color = KnightMaintenancePresentation::Muted;
		break;
	default:
		Label = FText::Format(LOCTEXT("Owned", "보유 {0}"), FText::AsNumber(OwnedCount));
		Color = KnightMaintenancePresentation::Secondary;
		break;
	}
	if (Txt_State)
	{
		// Preserve the font but prevent the legacy blue caption style from
		// overriding the maintenance palette during SynchronizeProperties.
		if (UCommonTextBlock* Styled = Cast<UCommonTextBlock>(Txt_State)) Styled->SetStyle(nullptr);
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
