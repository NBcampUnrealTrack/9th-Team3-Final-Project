#include "Menu/UI/KnightMaintenancePresentation.h"
#include "Engine/Texture2D.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

FSlateBrush KnightMaintenancePresentation::MakeEdgePanelBrush(UImage* Owner, UTexture2D* Texture, bool bRight)
{
	// Preserve the original artwork size and alpha. This material draws the original
	// panel as one surface, without the later opaque backing underneath it.
	Owner->SetBrushFromTexture(Texture, true);
	FSlateBrush Brush = Owner->GetBrush();
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.TintColor = FSlateColor(FLinearColor::White);
	Brush.SetResourceObject(Texture);
	if (!Texture) return Brush;
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Knight/UI/Menu/Maintenance/Hologram/M_UI_HoloEdgePanel.M_UI_HoloEdgePanel"));
	if (ensureMsgf(Material, TEXT("Maintenance edge panel material is missing")))
	{
		UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Material, Owner);
		Instance->SetTextureParameterValue(TEXT("PanelTexture"), Texture);
		Instance->SetScalarParameterValue(TEXT("RightSide"), bRight ? 1.f : 0.f);
		Brush.SetResourceObject(Instance);
	}
	return Brush;
}
