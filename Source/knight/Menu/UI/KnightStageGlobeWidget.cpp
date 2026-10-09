#include "KnightStageGlobeWidget.h"

#include "CommonTextBlock.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElements.h"

namespace
{
	// 스타일 가이드 색 (sRGB → 선형).
	const FLinearColor Frost(FColor(0xA8, 0xC8, 0xF0));
	const FLinearColor FrostDim(FColor(0x5F, 0x7F, 0xA8));
	const FLinearColor White(FColor(0xF4, 0xF8, 0xFC));
	const FLinearColor Ember(FColor(0xFF, 0x7A, 0x1A));

	/** Clicks that move less than this are clicks, not drags. Slate units. */
	constexpr float ClickSlop = 4.f;
	constexpr float MarkerHitRadius = 22.f;

	FLinearColor WithAlpha(const FLinearColor& Color, float Alpha)
	{
		return FLinearColor(Color.R, Color.G, Color.B, Color.A * Alpha);
	}

	void AddDiamond(TArray<FVector2f>& Points, const FVector2D& Center, float HalfSize)
	{
		const FVector2f C(Center);
		Points = { C + FVector2f(0.f, -HalfSize), C + FVector2f(HalfSize, 0.f), C + FVector2f(0.f, HalfSize), C + FVector2f(-HalfSize, 0.f), C + FVector2f(0.f, -HalfSize) };
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
	const float Lat = FMath::DegreesToRadians(LocationDegrees.Y);
	const float Lon = FMath::DegreesToRadians(LocationDegrees.X);
	const float X = FMath::Cos(Lat) * FMath::Sin(Lon);
	const float Y = FMath::Sin(Lat);
	const float Z = FMath::Cos(Lat) * FMath::Cos(Lon);

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
	}
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
	FLinearColor LabelColor = Frost;
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
		const FLinearColor Color = Marker.bLocked ? FrostDim : (bSelected ? Ember : (bHovered ? White : Frost));
		const float HalfSize = bSelected || bHovered ? 9.f : 7.f;

		AddDiamond(Points, Projected.Position, HalfSize);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, WithAlpha(Color, Fade), true, 2.f);
		AddDiamond(Points, Projected.Position, 2.5f);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, WithAlpha(Color, Fade), true, 3.f);

		if (bSelected)
		{
			// A ring that keeps opening outward.
			const float Phase = FMath::Fmod(Time, 1.4f) / 1.4f;
			const float RingRadius = FMath::Lerp(12.f, 30.f, Phase);
			TArray<FVector2f> Ring;
			for (int32 Step = 0; Step <= 32; ++Step)
			{
				const float Angle = UE_TWO_PI * Step / 32.f;
				Ring.Add(FVector2f(Projected.Position) + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * RingRadius);
			}
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Ring, ESlateDrawEffect::None,
				WithAlpha(Ember, Fade * (1.f - Phase)), true, 1.5f);
		}

		if (!Marker.Label.IsEmpty() && Font.FontObject)
		{
			const FVector2D TextSize = Measure->Measure(Marker.Label, Font);
			const bool bLabelLeft = bSelected && bHasTarget && TopLeft.X > Projected.Position.X;
			const FVector2f TextPosition(Projected.Position + FVector2D(bLabelLeft ? -16.f - TextSize.X : 16.f, -TextSize.Y * 0.5f));
			const FLinearColor TextColor = bSelected ? Ember : (bHovered ? White : LabelColor);
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(TextPosition)),
				Marker.Label, Font, ESlateDrawEffect::None, WithAlpha(TextColor, Fade * (bSelected || bHovered ? 1.f : 0.75f)));
		}
	}

	// Leader line: marker → diagonal → short shelf → panel edge (same shape as the lock-on marker).
	if (bSelectedVisible && bHasTarget)
	{
		const bool bTargetOnRight = TopLeft.X > SelectedPosition.X;
		const float EdgeX = bTargetOnRight ? TopLeft.X : BottomRight.X;
		const float AnchorY = FMath::Clamp(SelectedPosition.Y, TopLeft.Y + 24.f, BottomRight.Y - 24.f);
		const FVector2f Anchor(EdgeX, AnchorY);
		const FVector2f Elbow(EdgeX + (bTargetOnRight ? -36.f : 36.f), AnchorY);
		const FVector2f Start(SelectedPosition);
		const FVector2f Direction = (Elbow - Start).GetSafeNormal();
		Points = { Start + Direction * 12.f, Elbow, Anchor };
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, WithAlpha(Ember, 0.9f * Opacity), true, 1.5f);
	}
	return LayerId;
}
