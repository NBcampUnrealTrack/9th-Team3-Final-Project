#include "KnightStageGlobeWidget.h"

#include "CommonTextBlock.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElements.h"

namespace
{
	// 스타일 가이드 색 (sRGB → 선형).
	const FLinearColor GlobeFrost(FColor(0xA8, 0xC8, 0xF0));
	const FLinearColor FrostDim(FColor(0x5F, 0x7F, 0xA8));
	const FLinearColor GlobeWhite(FColor(0xF4, 0xF8, 0xFC));
	const FLinearColor GlobeEmber(FColor(0xFF, 0x7A, 0x1A));

	/** Clicks that move less than this are clicks, not drags. Slate units. */
	constexpr float ClickSlop = 4.f;
	/** Clicks this close to a region's name or centre also count, for regions seen edge-on. */
	constexpr float MarkerHitRadius = 22.f;
	/** The material wobbles region edges by up to about 20%; clicks use the outer edge so a painted pixel is never missed. */
	constexpr float RegionHitScale = 1.2f;
	constexpr int32 MaxRegions = 8;

	FVector DirectionOf(const FVector2D& LocationDegrees)
	{
		const float Lat = FMath::DegreesToRadians(LocationDegrees.Y);
		const float Lon = FMath::DegreesToRadians(LocationDegrees.X);
		return FVector(FMath::Cos(Lat) * FMath::Sin(Lon), FMath::Sin(Lat), FMath::Cos(Lat) * FMath::Cos(Lon));
	}

	FLinearColor WithAlpha(const FLinearColor& Color, float Alpha)
	{
		return FLinearColor(Color.R, Color.G, Color.B, Color.A * Alpha);
	}

}

UKnightStageGlobeWidget::UKnightStageGlobeWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// No child widgets, so the widget itself has to take the mouse.
	SetVisibilityInternal(ESlateVisibility::Visible);
}

void UKnightStageGlobeWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (GlobeMaterial)
	{
		GlobeMID = UMaterialInstanceDynamic::Create(GlobeMaterial, this);
		GlobeBrush.SetResourceObject(GlobeMID);
		GlobeBrush.DrawAs = ESlateBrushDrawType::Image;
	}
	ApplyRotationToMaterial();
}

void UKnightStageGlobeWidget::SetMarkers(const TArray<FKnightGlobeMarker>& InMarkers)
{
	Markers = InMarkers;
	HoveredIndex = INDEX_NONE;
	if (!Markers.ContainsByPredicate([this](const FKnightGlobeMarker& Marker) { return Marker.StageId == SelectedStageId; }))
	{
		SelectedStageId = NAME_None;
	}
}

void UKnightStageGlobeWidget::SetSelectedStage(FName StageId)
{
	SelectedStageId = StageId;
}

void UKnightStageGlobeWidget::FocusStage(FName StageId)
{
	const FKnightGlobeMarker* const Marker = Markers.FindByPredicate([StageId](const FKnightGlobeMarker& Entry) { return Entry.StageId == StageId; });
	if (!Marker)
	{
		return;
	}
	// The material shows the point (lat, lon) at the centre when Yaw = -lon and Pitch = lat.
	FocusYaw = FMath::DegreesToRadians(-Marker->Location.X);
	FocusPitch = FMath::DegreesToRadians(FMath::Clamp(Marker->Location.Y, -MaxPitchDegrees, MaxPitchDegrees));
	bFocusing = true;
	bUserTouched = true;
	YawVelocity = PitchVelocity = 0.f;
}

float UKnightStageGlobeWidget::GetPixelRadius(const FVector2D& LocalSize) const
{
	return 0.5f * FMath::Min(LocalSize.X, LocalSize.Y) * SphereRadius;
}

UKnightStageGlobeWidget::FProjectedMarker UKnightStageGlobeWidget::Project(const FVector2D& LocationDegrees, const FVector2D& LocalSize) const
{
	const FVector Direction = DirectionOf(LocationDegrees);
	const float X = Direction.X;
	const float Y = Direction.Y;
	const float Z = Direction.Z;

	// Inverse of the material: undo the yaw, then the pitch.
	const float CY = FMath::Cos(Yaw), SY = FMath::Sin(Yaw);
	const float X1 = CY * X + SY * Z;
	const float Z1 = -SY * X + CY * Z;
	const float CP = FMath::Cos(Pitch), SP = FMath::Sin(Pitch);
	const float Y2 = CP * Y - SP * Z1;
	const float Z2 = SP * Y + CP * Z1;

	const float Radius = GetPixelRadius(LocalSize);
	FProjectedMarker Result;
	Result.Position = LocalSize * 0.5f + FVector2D(X1, -Y2) * Radius;
	Result.Depth = Z2;
	return Result;
}

bool UKnightStageGlobeWidget::GetMarkerLocalPosition(FName StageId, FVector2D& OutPosition, bool& bOutFront) const
{
	const FKnightGlobeMarker* const Marker = Markers.FindByPredicate([StageId](const FKnightGlobeMarker& Entry) { return Entry.StageId == StageId; });
	if (!Marker)
	{
		return false;
	}
	const FProjectedMarker Projected = Project(Marker->Location, GetCachedGeometry().GetLocalSize());
	OutPosition = Projected.Position;
	bOutFront = Projected.Depth > 0.f;
	return true;
}

void UKnightStageGlobeWidget::ApplyRotationToMaterial()
{
	if (GlobeMID)
	{
		GlobeMID->SetScalarParameterValue(TEXT("Yaw"), Yaw);
		GlobeMID->SetScalarParameterValue(TEXT("Pitch"), Pitch);
		GlobeMID->SetScalarParameterValue(TEXT("Radius"), SphereRadius);
		for (int32 Index = 0; Index < MaxRegions; ++Index)
		{
			FLinearColor Region(0.f, 0.f, 0.f, 0.f);
			if (Markers.IsValidIndex(Index))
			{
				const FKnightGlobeMarker& Marker = Markers[Index];
				const float State = Marker.StageId == SelectedStageId ? 3.f : (Index == HoveredIndex ? 2.f : 1.f);
				Region = FLinearColor(FMath::DegreesToRadians(Marker.Location.X), FMath::DegreesToRadians(Marker.Location.Y),
					FMath::DegreesToRadians(Marker.RegionRadius), State + (Marker.bLocked ? 3.f : 0.f));
			}
			static const FName RegionNames[MaxRegions] = { TEXT("Region0"), TEXT("Region1"), TEXT("Region2"), TEXT("Region3"),
				TEXT("Region4"), TEXT("Region5"), TEXT("Region6"), TEXT("Region7") };
			GlobeMID->SetVectorParameterValue(RegionNames[Index], Region);
		}
	}
}

bool UKnightStageGlobeWidget::LocalToSurface(const FVector2D& LocalPosition, const FVector2D& LocalSize, FVector& OutDirection) const
{
	const float Radius = GetPixelRadius(LocalSize);
	if (Radius <= 0.f)
	{
		return false;
	}
	const FVector2D S = (LocalPosition - LocalSize * 0.5f) / Radius;
	const float SquaredLength = static_cast<float>(S.SizeSquared());
	if (SquaredLength > 1.f)
	{
		return false;
	}
	// Same steps as the material: screen up is +Y, then undo pitch (around X) and yaw (around Y).
	const float X = S.X, Y = -S.Y, Z = FMath::Sqrt(1.f - SquaredLength);
	const float CP = FMath::Cos(Pitch), SP = FMath::Sin(Pitch);
	const float Y1 = CP * Y + SP * Z;
	const float Z1 = -SP * Y + CP * Z;
	const float CY = FMath::Cos(Yaw), SY = FMath::Sin(Yaw);
	OutDirection = FVector(CY * X - SY * Z1, Y1, SY * X + CY * Z1);
	return true;
}

void UKnightStageGlobeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Time += InDeltaTime;
	const float MaxPitch = FMath::DegreesToRadians(MaxPitchDegrees);

	if (bFocusing)
	{
		const float YawDelta = FMath::FindDeltaAngleRadians(Yaw, FocusYaw);
		const float Alpha = 1.f - FMath::Exp(-8.f * InDeltaTime);
		Yaw += YawDelta * Alpha;
		Pitch = FMath::Lerp(Pitch, FocusPitch, Alpha);
		if (FMath::Abs(YawDelta) < 0.001f && FMath::Abs(FocusPitch - Pitch) < 0.001f)
		{
			bFocusing = false;
		}
	}
	else if (!bDragging)
	{
		// Let a flick keep turning for a moment.
		Yaw += YawVelocity * InDeltaTime;
		Pitch += PitchVelocity * InDeltaTime;
		const float Decay = FMath::Exp(-4.f * InDeltaTime);
		YawVelocity *= Decay;
		PitchVelocity *= Decay;
		if (!bUserTouched && SelectedStageId.IsNone())
		{
			Yaw += FMath::DegreesToRadians(IdleSpinDegreesPerSecond) * InDeltaTime;
		}
	}
	Yaw = FMath::UnwindRadians(Yaw);
	Pitch = FMath::Clamp(Pitch, -MaxPitch, MaxPitch);
	ApplyRotationToMaterial();
}

int32 UKnightStageGlobeWidget::FindMarkerAt(const FVector2D& LocalPosition, const FVector2D& LocalSize) const
{
	// Inside a painted region first; the nearest centre wins where two regions overlap.
	FVector Direction;
	if (LocalToSurface(LocalPosition, LocalSize, Direction))
	{
		int32 BestRegion = INDEX_NONE;
		float BestRatio = RegionHitScale;
		for (int32 Index = 0; Index < FMath::Min(Markers.Num(), MaxRegions); ++Index)
		{
			const float Angle = FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(Direction, DirectionOf(Markers[Index].Location))), -1.f, 1.f));
			const float Ratio = Angle / FMath::Max(FMath::DegreesToRadians(Markers[Index].RegionRadius), UE_KINDA_SMALL_NUMBER);
			if (Ratio < BestRatio)
			{
				BestRatio = Ratio;
				BestRegion = Index;
			}
		}
		if (BestRegion != INDEX_NONE)
		{
			return BestRegion;
		}
	}

	int32 Best = INDEX_NONE;
	float BestDistance = MarkerHitRadius;
	for (int32 Index = 0; Index < Markers.Num(); ++Index)
	{
		const FProjectedMarker Projected = Project(Markers[Index].Location, LocalSize);
		if (Projected.Depth <= 0.1f)
		{
			continue;
		}
		const float Distance = FVector2D::Distance(Projected.Position, LocalPosition);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Index;
		}
	}
	return Best;
}

void UKnightStageGlobeWidget::SetHovered(int32 Index)
{
	if (Index == HoveredIndex)
	{
		return;
	}
	HoveredIndex = Index;
	SetCursor(Index != INDEX_NONE ? EMouseCursor::Hand : EMouseCursor::Default);
	if (Index != INDEX_NONE && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().PlaySound(HoverSound);
	}
}

FReply UKnightStageGlobeWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	bDragging = true;
	bDragMoved = false;
	bFocusing = false;
	YawVelocity = PitchVelocity = 0.f;
	DragStart = DragLast = InMouseEvent.GetScreenSpacePosition();
	DragLastTime = FPlatformTime::Seconds();
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UKnightStageGlobeWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D LocalSize = InGeometry.GetLocalSize();
	if (!bDragging)
	{
		SetHovered(FindMarkerAt(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()), LocalSize));
		return FReply::Unhandled();
	}

	const FVector2D Screen = InMouseEvent.GetScreenSpacePosition();
	// Work in local units so the feel does not change with the window size.
	const FVector2D Delta = InGeometry.AbsoluteToLocal(Screen) - InGeometry.AbsoluteToLocal(DragLast);
	DragLast = Screen;
	if (!bDragMoved && FVector2D::Distance(InGeometry.AbsoluteToLocal(Screen), InGeometry.AbsoluteToLocal(DragStart)) > ClickSlop)
	{
		bDragMoved = true;
		bUserTouched = true;
		SetHovered(INDEX_NONE);
	}
	if (bDragMoved)
	{
		const float Step = FMath::DegreesToRadians(DragDegreesPerPixel);
		Yaw += Delta.X * Step;
		Pitch += Delta.Y * Step;
		const double Now = FPlatformTime::Seconds();
		const float Dt = FMath::Max(static_cast<float>(Now - DragLastTime), 1.f / 240.f);
		DragLastTime = Now;
		YawVelocity = FMath::Lerp(YawVelocity, Delta.X * Step / Dt, 0.5f);
		PitchVelocity = FMath::Lerp(PitchVelocity, Delta.Y * Step / Dt, 0.5f);
	}
	return FReply::Handled();
}

FReply UKnightStageGlobeWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDragging || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	bDragging = false;
	// A held mouse that stopped moving should not fling the globe on release.
	if (FPlatformTime::Seconds() - DragLastTime > 0.08)
	{
		YawVelocity = PitchVelocity = 0.f;
	}
	if (!bDragMoved)
	{
		const int32 Index = FindMarkerAt(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()), InGeometry.GetLocalSize());
		if (Index != INDEX_NONE)
		{
			SelectedStageId = Markers[Index].StageId;
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().PlaySound(SelectSound);
			}
			OnStageClicked.Broadcast(SelectedStageId);
		}
	}
	return FReply::Handled().ReleaseMouseCapture();
}

void UKnightStageGlobeWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	if (!bDragging)
	{
		SetHovered(INDEX_NONE);
	}
}

int32 UKnightStageGlobeWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const float Side = FMath::Min(LocalSize.X, LocalSize.Y);
	const FVector2f QuadSize(Side, Side);
	const FVector2f QuadOffset((LocalSize.X - Side) * 0.5f, (LocalSize.Y - Side) * 0.5f);
	const float Opacity = InWidgetStyle.GetColorAndOpacityTint().A;

	if (GlobeMID)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, ++LayerId, AllottedGeometry.ToPaintGeometry(QuadSize, FSlateLayoutTransform(QuadOffset)),
			&GlobeBrush, ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, Opacity));
	}

	FSlateFontInfo Font;
	FLinearColor LabelColor = GlobeFrost;
	if (const UCommonTextStyle* const Style = LabelTextStyle ? LabelTextStyle->GetDefaultObject<UCommonTextStyle>() : nullptr)
	{
		Style->GetFont(Font);
		LabelColor = Style->Color;
	}
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	// Panel rectangle in local space, for the leader line and to keep the selected label off the line's side.
	const UWidget* const Target = LeaderTarget.Get();
	bool bHasTarget = false;
	FVector2D TopLeft = FVector2D::ZeroVector;
	FVector2D BottomRight = FVector2D::ZeroVector;
	if (Target && Target->IsVisible())
	{
		const FGeometry& TargetGeometry = Target->GetCachedGeometry();
		TopLeft = AllottedGeometry.AbsoluteToLocal(TargetGeometry.GetAbsolutePosition());
		BottomRight = AllottedGeometry.AbsoluteToLocal(TargetGeometry.GetAbsolutePosition() + TargetGeometry.GetAbsoluteSize());
		bHasTarget = BottomRight.X > TopLeft.X && BottomRight.Y > TopLeft.Y;
	}

	++LayerId;
	FVector2D SelectedPosition = FVector2D::ZeroVector;
	bool bSelectedVisible = false;
	float SelectedLabelHalfWidth = 0.f;
	TArray<FVector2f> Points;
	for (int32 Index = 0; Index < Markers.Num(); ++Index)
	{
		const FKnightGlobeMarker& Marker = Markers[Index];
		const FProjectedMarker Projected = Project(Marker.Location, LocalSize);
		// Fade out near the edge, gone behind the globe.
		const float Fade = FMath::Clamp(Projected.Depth / 0.25f, 0.f, 1.f) * Opacity;
		if (Fade <= 0.f)
		{
			continue;
		}
		const bool bSelected = Marker.StageId == SelectedStageId;
		const bool bHovered = Index == HoveredIndex;
		if (bSelected)
		{
			SelectedPosition = Projected.Position;
			bSelectedVisible = Projected.Depth > 0.1f;
		}
		if (!Marker.Label.IsEmpty() && Font.FontObject)
		{
			// The region itself shows hover and selection; the name sits on its centre.
			const FVector2D TextSize = Measure->Measure(Marker.Label, Font);
			const FVector2f TextPosition(Projected.Position - TextSize * 0.5f);
			if (bSelected)
			{
				SelectedLabelHalfWidth = static_cast<float>(TextSize.X * 0.5f);
			}
			const FLinearColor TextColor = Marker.bLocked ? FrostDim : (bSelected ? GlobeEmber : (bHovered ? GlobeWhite : LabelColor));
			// Dark shadow so the name stays readable on the light region.
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(TextPosition + FVector2f(1.f, 1.f))),
				Marker.Label, Font, ESlateDrawEffect::None, FLinearColor(0.f, 0.01f, 0.03f, 0.85f * Fade));
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(TextPosition)),
				Marker.Label, Font, ESlateDrawEffect::None, WithAlpha(TextColor, Fade));
		}
	}

	// Leader line: region name → diagonal → short shelf → panel edge (same shape as the lock-on marker).
	if (bSelectedVisible && bHasTarget)
	{
		const bool bTargetOnRight = TopLeft.X > SelectedPosition.X;
		const float EdgeX = bTargetOnRight ? TopLeft.X : BottomRight.X;
		const float AnchorY = FMath::Clamp(SelectedPosition.Y, TopLeft.Y + 24.f, BottomRight.Y - 24.f);
		const FVector2f Anchor(EdgeX, AnchorY);
		const FVector2f Elbow(EdgeX + (bTargetOnRight ? -36.f : 36.f), AnchorY);
		// Start just past the end of the region name, on the panel's side.
		const FVector2f Start(SelectedPosition.X + (bTargetOnRight ? 1.f : -1.f) * (SelectedLabelHalfWidth + 6.f), SelectedPosition.Y);
		Points = { Start, Elbow, Anchor };
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, WithAlpha(GlobeEmber, 0.9f * Opacity), true, 1.5f);
	}
	return LayerId;
}
