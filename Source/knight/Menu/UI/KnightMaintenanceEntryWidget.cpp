#include "Menu/UI/KnightMaintenanceEntryWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Item/knightEquipmentData.h"

TSubclassOf<UKnightMaintenanceEntryWidget> UKnightMaintenanceEntryWidget::GetSharedWidgetClass()
{
	return LoadClass<UKnightMaintenanceEntryWidget>(nullptr,
		TEXT("/Game/Knight/UI/Menu/Maintenance/WBP_KnightMaintenanceEntry.WBP_KnightMaintenanceEntry_C"));
}

void UKnightMaintenanceEntryWidget::SetEntry(const UknightPartItemDefinition* InPart,
	const UknightCraftRecipeDefinition* InRecipe, bool bEquipped, int32 OwnedCount)
{
	Part = InPart;
	Recipe = InRecipe;
	if (!Part) return;
	if (Txt_Name) Txt_Name->SetText(Part->DisplayName.IsEmpty() ? FText::FromString(Part->GetName()) : Part->DisplayName);
	if (Txt_State) Txt_State->SetText(bEquipped ? INVTEXT("장착 중") : InRecipe ? INVTEXT("제작")
		: FText::Format(INVTEXT("보유 {0}"), FText::AsNumber(OwnedCount)));
	if (Img_Icon)
	{
		if (UTexture2D* Icon = Part->Icon.LoadSynchronous()) Img_Icon->SetBrushFromTexture(Icon);
		else Img_Icon->SetVisibility(ESlateVisibility::Collapsed);
	}
}
