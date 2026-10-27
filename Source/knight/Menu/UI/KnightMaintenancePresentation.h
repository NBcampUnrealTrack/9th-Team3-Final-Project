#pragma once

#include "CommonButtonBase.h"

class UTexture2D;
class UImage;

namespace KnightMaintenancePresentation
{
	// The bright outer rail begins 55 pixels into the 887-pixel source artwork.
	// Shift the whole side group by that fraction of its width without resizing it.
	inline constexpr float PanelArtworkInset = 55.f / 887.f;
	inline FAnchors ShiftSideAnchors(const FAnchors& Anchors, bool bRight)
	{
		const float Delta = (Anchors.Maximum.X - Anchors.Minimum.X) * PanelArtworkInset * (bRight ? 1.f : -1.f);
		return FAnchors(Anchors.Minimum.X + Delta, Anchors.Minimum.Y, Anchors.Maximum.X + Delta, Anchors.Maximum.Y);
	}
	FSlateBrush MakeEdgePanelBrush(UImage* Owner, UTexture2D* Texture, bool bRight);

	// Shared presentation palette only. Gameplay rarity and success/failure colors are independent.
	inline const FLinearColor Primary(FColor(244, 244, 242));
	inline const FLinearColor Secondary(FColor(174, 176, 178));
	inline const FLinearColor Muted(FColor(116, 119, 123));
	inline const FLinearColor Accent(FColor(255, 134, 42));
	inline const FLinearColor WarmHighlight(FColor(255, 187, 94));
	inline const FLinearColor Divider(0.35f, 0.12f, 0.025f, 0.60f);
	inline const FLinearColor Focus(1.f, 0.48f, 0.13f, 0.75f);
	inline const FLinearColor PanelTint(0.55f, 0.55f, 0.55f, 0.94f);
	inline const FLinearColor ReadoutTint(0.30f, 0.30f, 0.30f, 0.85f);
	inline const FMargin ControlSlice(.055f, .12f, .055f, .12f);
	inline void SetControlUV(FSlateBrush& Brush)
	{
		// Trim only the raster's transparent export padding, never live content.
		Brush.SetUVRegion(FBox2f(FVector2f(.012f, .14f), FVector2f(.988f, .84f)));
	}

	// knightUI depends on knight. Use its reflected presentation API, as with SetButtonText/SetLockedWithReason.
	inline void ConfigureButton(UCommonButtonBase* Button, bool bSelectable, bool bMainTab = false)
	{
		if (!Button) return;
		if (UFunction* Function = Button->FindFunction(TEXT("ConfigureMaintenanceButton")))
		{
			struct { bool bSelectable; bool bMainTab; } Params{bSelectable, bMainTab};
			Button->ProcessEvent(Function, &Params);
		}
	}
}
