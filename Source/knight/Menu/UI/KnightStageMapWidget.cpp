#include "KnightStageMapWidget.h"

#include "CommonTextBlock.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
	// 스타일 가이드 색 (sRGB → 선형).
	const FLinearColor MapFrost(FColor(0xA8, 0xC8, 0xF0));
	const FLinearColor FrostDim(FColor(0x5F, 0x7F, 0xA8));
	const FLinearColor MapWhite(FColor(0xF4, 0xF8, 0xFC));
	const FLinearColor MapEmber(FColor(0xFF, 0x7A, 0x1A));
	const FLinearColor LockedGrey(FColor(0x7A, 0x84, 0x90));

	/** Clicks that move less than this are clicks, not drags. Slate units. */
	constexpr float ClickSlop = 4.f;
	/** Clicks this close to a region's mark or name also count. */
	constexpr float MarkerHitRadius = 22.f;
	constexpr int32 MaxRegions = 8;
	/** The selected region flashes on and off this many seconds after it is picked, switching every BlinkStep. */
	constexpr float BlinkDuration = 0.6f;
	constexpr float BlinkStep = 0.15f;
	constexpr int32 RingSegments = 64;

	FLinearColor WithAlpha(const FLinearColor& Color, float Alpha)
	{
		return FLinearColor(Color.R, Color.G, Color.B, Color.A * Alpha);
	}

	/** Texture coordinates of a location: u from longitude, v from latitude (north up). */
	FVector2D ToUV(const FVector2D& LocationDegrees)
	{
		return FVector2D((LocationDegrees.X + 180.0) / 360.0, (90.0 - LocationDegrees.Y) / 180.0);
	}

	/** Region edge scale at an angle around the centre; same formula as the material so clicks match the drawing. */
	float RegionWobble(float Angle, const FVector2D& CenterUV)
	{
		return 1.f + 0.12f * FMath::Sin(3.f * Angle + static_cast<float>(CenterUV.X) * 40.f)
			+ 0.07f * FMath::Cos(5.f * Angle + static_cast<float>(CenterUV.Y) * 40.f);
	}

	FSlateFontInfo QuestFont(TSubclassOf<UCommonTextStyle> TextStyle)
	{
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14);
		if (const UCommonTextStyle* const Style = TextStyle ? TextStyle->GetDefaultObject<UCommonTextStyle>() : nullptr)
		{
			Style->GetFont(Font);
			Font.Size = FMath::Min(Font.Size, 16);
		}
		return Font;
	}

	FVector2D QuestPinSize(const FText& Label, const FSlateFontInfo& Font)
	{
		const FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Label, Font);
		return FVector2D(FMath::Max(76.0, TextSize.X + 24.0), FMath::Max(30.0, TextSize.Y + 10.0));
	}
}

UKnightStageMapWidget::UKnightStageMapWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// No child widgets, so the widget itself has to take the mouse.
	SetVisibilityInternal(ESlateVisibility::Visible);
	SetIsFocusable(true);
}

void UKnightStageMapWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (MapMaterial)
	{
		MapMID = UMaterialInstanceDynamic::Create(MapMaterial, this);
		MapBrush.SetResourceObject(MapMID);
		MapBrush.DrawAs = ESlateBrushDrawType::Image;
	}
}

void UKnightStageMapWidget::SetMarkers(const TArray<FKnightMapMarker>& InMarkers)
{
	Markers = InMarkers;
	HoveredIndex = INDEX_NONE;
	if (!Markers.ContainsByPredicate([this](const FKnightMapMarker& Marker) { return Marker.StageId == SelectedStageId; }))
	{
		SelectedStageId = NAME_None;
	}
	// Open on the middle of the stages until the player moves the map.
	if (!bUserTouched && !Markers.IsEmpty())
	{
		FVector2D Sum = FVector2D::ZeroVector;
		for (const FKnightMapMarker& Marker : Markers)
		{
			Sum += Marker.Location;
		}
		Center = Sum / Markers.Num();
	}
}

void UKnightStageMapWidget::SetQuestMarkers(const TArray<FKnightMapQuestMarker>& InMarkers)
{
	QuestMarkers = InMarkers;
	HoveredQuestIndex = INDEX_NONE;
	if (!QuestMarkers.ContainsByPredicate([this](const FKnightMapQuestMarker& Marker) { return Marker.QuestId == SelectedQuestId; }))
	{
		SelectedQuestId = NAME_None;
	}
}

void UKnightStageMapWidget::SetSelectedQuest(FName QuestId)
{
	SelectedQuestId = QuestId;
}

void UKnightStageMapWidget::SetSelectedStage(FName StageId)
{
	if (StageId != SelectedStageId)
	{
		SelectTime = Time;
	}
	SelectedStageId = StageId;
}

void UKnightStageMapWidget::FocusStage(FName StageId)
{
	const FKnightMapMarker* const Marker = Markers.FindByPredicate([StageId](const FKnightMapMarker& Entry) { return Entry.StageId == StageId; });
	if (!Marker)
	{
		return;
	}
	FocusCenter = Marker->Location;
	bFocusing = true;
	bUserTouched = true;
	Velocity = FVector2D::ZeroVector;
}

UKnightStageMapWidget::FMapView UKnightStageMapWidget::MakeView(const FVector2D& LocalSize) const
{
	FMapView View;
	View.LocalSize = LocalSize;
	// Zoom from the height, but never so far out that the map is narrower than the widget.
	View.PixelsPerDegree = FMath::Max(static_cast<float>(LocalSize.Y) / FMath::Min(VisibleLatitude, 2.f * LatitudeLimit),
		static_cast<float>(LocalSize.X) / 360.f);
	View.PixelsPerDegree = FMath::Max(View.PixelsPerDegree, UE_KINDA_SMALL_NUMBER);
	View.Center = ClampCenter(Center, View);
	return View;
}

FVector2D UKnightStageMapWidget::ClampCenter(const FVector2D& InCenter, const FMapView& View) const
{
	// The view never leaves the map: half the visible span from each edge.
	const double HalfLon = FMath::Min(View.LocalSize.X * 0.5 / View.PixelsPerDegree, 180.0);
	const double HalfLat = FMath::Min(View.LocalSize.Y * 0.5 / View.PixelsPerDegree, static_cast<double>(LatitudeLimit));
	return FVector2D(FMath::Clamp(InCenter.X, -180.0 + HalfLon, 180.0 - HalfLon), FMath::Clamp(InCenter.Y, -LatitudeLimit + HalfLat, LatitudeLimit - HalfLat));
}

FVector2D UKnightStageMapWidget::ToLocal(const FVector2D& LocationDegrees, const FMapView& View) const
{
	return View.LocalSize * 0.5 + FVector2D(LocationDegrees.X - View.Center.X, View.Center.Y - LocationDegrees.Y) * View.PixelsPerDegree;
}

void UKnightStageMapWidget::ApplyToMaterial(const FVector2D& LocalSize)
{
	if (!MapMID)
	{
		return;
	}
	const FMapView View = MakeView(LocalSize);
	const FVector2D TopLeftUV = ToUV(FVector2D(View.Center.X - LocalSize.X * 0.5 / View.PixelsPerDegree, View.Center.Y + LocalSize.Y * 0.5 / View.PixelsPerDegree));
	const FVector2D SizeUV(LocalSize.X / View.PixelsPerDegree / 360.0, LocalSize.Y / View.PixelsPerDegree / 180.0);
	MapMID->SetVectorParameterValue(TEXT("ViewRect"), FLinearColor(TopLeftUV.X, TopLeftUV.Y, SizeUV.X, SizeUV.Y));
	MapMID->SetVectorParameterValue(TEXT("MapPixels"), FLinearColor(360.f * View.PixelsPerDegree, 180.f * View.PixelsPerDegree, 0.f, 0.f));

	const float SinceSelect = Time - SelectTime;
	const bool bBlinking = !SelectedStageId.IsNone() && SinceSelect < BlinkDuration;
	MapMID->SetScalarParameterValue(TEXT("Blink"), bBlinking ? (FMath::FloorToInt(SinceSelect / BlinkStep) % 2 == 0 ? 1.f : -1.f) : 0.f);

	for (int32 Index = 0; Index < MaxRegions; ++Index)
	{
		FLinearColor Region(0.f, 0.f, 0.f, 0.f);
		if (Markers.IsValidIndex(Index))
		{
			const FKnightMapMarker& Marker = Markers[Index];
			const float State = Marker.StageId == SelectedStageId ? 3.f : (Index == HoveredIndex ? 2.f : 1.f);
			const FVector2D UV = ToUV(Marker.Location);
			Region = FLinearColor(UV.X, UV.Y, Marker.RegionRadius * View.PixelsPerDegree, State + (Marker.bLocked ? 3.f : 0.f));
		}
		static const FName RegionNames[MaxRegions] = { TEXT("Region0"), TEXT("Region1"), TEXT("Region2"), TEXT("Region3"),
			TEXT("Region4"), TEXT("Region5"), TEXT("Region6"), TEXT("Region7") };
		MapMID->SetVectorParameterValue(RegionNames[Index], Region);
	}
}

void UKnightStageMapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Time += InDeltaTime;
	const FVector2D LocalSize = MyGeometry.GetLocalSize();
	const FMapView View = MakeView(LocalSize);

	if (bFocusing)
	{
		// Aim at the nearest centre the edges allow, or the glide never settles on a region near the edge.
		const FVector2D Target = ClampCenter(FocusCenter, View);
		const float Alpha = 1.f - FMath::Exp(-8.f * InDeltaTime);
		Center = FMath::Lerp(View.Center, Target, Alpha);
		if (FVector2D::Distance(Center, Target) < 0.01)
		{
			Center = Target;
			bFocusing = false;
		}
	}
	else if (!bDragging)
	{
		// Let a flick keep sliding for a moment.
		Center += Velocity * InDeltaTime;
		Velocity *= FMath::Exp(-4.f * InDeltaTime);
	}
	const FVector2D Clamped = ClampCenter(Center, View);
	// Hitting an edge stops the slide on that axis instead of pushing against it.
	if (Clamped.X != Center.X) Velocity.X = 0.0;
	if (Clamped.Y != Center.Y) Velocity.Y = 0.0;
	Center = Clamped;
	ApplyToMaterial(LocalSize);
}

int32 UKnightStageMapWidget::FindMarkerAt(const FVector2D& LocalPosition, const FVector2D& LocalSize) const
{
	const FMapView View = MakeView(LocalSize);
	int32 Best = INDEX_NONE;
	float BestRatio = 1.f;
	// Inside a painted region first; the nearest centre (relative to size) wins where two overlap.
	for (int32 Index = 0; Index < FMath::Min(Markers.Num(), MaxRegions); ++Index)
	{
		const FKnightMapMarker& Marker = Markers[Index];
		const FVector2D Offset = LocalPosition - ToLocal(Marker.Location, View);
		const float Angle = FMath::Atan2(static_cast<float>(Offset.Y), static_cast<float>(Offset.X));
		const float Radius = Marker.RegionRadius * View.PixelsPerDegree * RegionWobble(Angle, ToUV(Marker.Location));
		const float Ratio = static_cast<float>(Offset.Size()) / FMath::Max(Radius, UE_KINDA_SMALL_NUMBER);
		if (Ratio < BestRatio)
		{
			BestRatio = Ratio;
			Best = Index;
		}
	}
	if (Best != INDEX_NONE)
	{
		return Best;
	}

	float BestDistance = MarkerHitRadius;
	for (int32 Index = 0; Index < Markers.Num(); ++Index)
	{
		const float Distance = static_cast<float>(FVector2D::Distance(ToLocal(Markers[Index].Location, View), LocalPosition));
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Index;
		}
	}
	return Best;
}

int32 UKnightStageMapWidget::FindQuestMarkerAt(const FVector2D& LocalPosition, const FVector2D& LocalSize) const
{
	if (!FSlateApplication::IsInitialized()) return INDEX_NONE;
	const FMapView View = MakeView(LocalSize);
	const FSlateFontInfo Font = QuestFont(LabelTextStyle);
	for (int32 Index = QuestMarkers.Num() - 1; Index >= 0; --Index)
	{
		const FKnightMapQuestMarker& Marker = QuestMarkers[Index];
		const FVector2D Position = ToLocal(Marker.Location, View);
		const FVector2D HalfSize = QuestPinSize(Marker.Label, Font) * 0.5;
		if (FSlateRect(Position - HalfSize, Position + HalfSize).ContainsPoint(LocalPosition)) return Index;
	}
	return INDEX_NONE;
}

void UKnightStageMapWidget::SetHovered(int32 Index)
{
	if (Index == HoveredIndex)
	{
		return;
	}
	HoveredIndex = Index;
	SetCursor(Index != INDEX_NONE || HoveredQuestIndex != INDEX_NONE ? EMouseCursor::Hand : EMouseCursor::Default);
	if (Index != INDEX_NONE && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().PlaySound(HoverSound);
	}
}

void UKnightStageMapWidget::SetHoveredQuest(int32 Index)
{
	if (Index == HoveredQuestIndex) return;
	HoveredQuestIndex = Index;
	SetCursor(Index != INDEX_NONE || HoveredIndex != INDEX_NONE ? EMouseCursor::Hand : EMouseCursor::Default);
	if (Index != INDEX_NONE && FSlateApplication::IsInitialized()) FSlateApplication::Get().PlaySound(HoverSound);
}

FReply UKnightStageMapWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	bDragging = true;
	bDragMoved = false;
	bFocusing = false;
	Velocity = FVector2D::ZeroVector;
	DragStart = DragLast = InMouseEvent.GetScreenSpacePosition();
	DragLastTime = FPlatformTime::Seconds();
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UKnightStageMapWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D LocalSize = InGeometry.GetLocalSize();
	if (!bDragging)
	{
		const FVector2D Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		SetHoveredQuest(FindQuestMarkerAt(Local, LocalSize));
		SetHovered(HoveredQuestIndex == INDEX_NONE ? FindMarkerAt(Local, LocalSize) : INDEX_NONE);
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
		SetHoveredQuest(INDEX_NONE);
	}
	if (bDragMoved)
	{
		// The map follows the cursor: dragging right shows land further west, dragging down shows land further north.
		const FMapView View = MakeView(LocalSize);
		const FVector2D Step = FVector2D(-Delta.X, Delta.Y) / View.PixelsPerDegree;
		Center = ClampCenter(View.Center + Step, View);
		const double Now = FPlatformTime::Seconds();
		const double Dt = FMath::Max(Now - DragLastTime, 1.0 / 240.0);
		DragLastTime = Now;
		Velocity = FMath::Lerp(Velocity, Step / Dt, 0.5);
	}
	return FReply::Handled();
}

FReply UKnightStageMapWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDragging || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	bDragging = false;
	// A held mouse that stopped moving should not fling the map on release.
	if (FPlatformTime::Seconds() - DragLastTime > 0.08)
	{
		Velocity = FVector2D::ZeroVector;
	}
	if (!bDragMoved)
	{
		const FVector2D Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		const int32 QuestIndex = FindQuestMarkerAt(Local, InGeometry.GetLocalSize());
		if (QuestIndex != INDEX_NONE)
		{
			const FKnightMapQuestMarker& Quest = QuestMarkers[QuestIndex];
			SetSelectedStage(Quest.RegionId);
			SetSelectedQuest(Quest.QuestId);
			FocusStage(Quest.RegionId);
			if (FSlateApplication::IsInitialized()) FSlateApplication::Get().PlaySound(SelectSound);
			OnQuestClicked.Broadcast(Quest.QuestId);
			return FReply::Handled().ReleaseMouseCapture();
		}
		const int32 Index = FindMarkerAt(Local, InGeometry.GetLocalSize());
		if (Index != INDEX_NONE)
		{
			SetSelectedStage(Markers[Index].StageId);
			FocusStage(SelectedStageId);
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().PlaySound(SelectSound);
			}
			OnStageClicked.Broadcast(SelectedStageId);
		}
	}
	return FReply::Handled().ReleaseMouseCapture();
}

FReply UKnightStageMapWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const float Wheel = InMouseEvent.GetWheelDelta();
	if (FMath::IsNearlyZero(Wheel)) return FReply::Unhandled();
	VisibleLatitude = FMath::Clamp(VisibleLatitude * FMath::Pow(0.9f, Wheel), 35.f, 150.f);
	bUserTouched = true;
	Velocity = FVector2D::ZeroVector;
	return FReply::Handled();
}

void UKnightStageMapWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	if (!bDragging)
	{
		SetHovered(INDEX_NONE);
		SetHoveredQuest(INDEX_NONE);
	}
}

int32 UKnightStageMapWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const FMapView View = MakeView(LocalSize);
	const float Opacity = InWidgetStyle.GetColorAndOpacityTint().A;
	const FSlateRect Bounds(FVector2D::ZeroVector, LocalSize);

	// Everything but the leader line stays inside the map.
	OutDrawElements.PushClip(FSlateClippingZone(AllottedGeometry));
	if (MapMID)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, ++LayerId, AllottedGeometry.ToPaintGeometry(), &MapBrush, ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, Opacity));
	}

	FSlateFontInfo Font;
	FLinearColor LabelColor = MapFrost;
	if (const UCommonTextStyle* const Style = LabelTextStyle ? LabelTextStyle->GetDefaultObject<UCommonTextStyle>() : nullptr)
	{
		Style->GetFont(Font);
		LabelColor = Style->Color;
	}
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	++LayerId;
	TArray<FVector2f> Points;
	FVector2D SelectedPosition = FVector2D::ZeroVector;
	bool bSelectedVisible = false;
	for (int32 Index = 0; Index < Markers.Num(); ++Index)
	{
		const FKnightMapMarker& Marker = Markers[Index];
		const FVector2D Position = ToLocal(Marker.Location, View);
		const bool bSelected = Marker.StageId == SelectedStageId;
		const bool bHovered = Index == HoveredIndex;
		const float RegionPixels = Marker.RegionRadius * View.PixelsPerDegree;

		// Sonar rings keep spreading from the selected stage: a new one every PingInterval, thinning and fading as they grow.
		if (bSelected && PingLifetime > 0.f && PingInterval > 0.f)
		{
			const float SinceSelect = Time - SelectTime;
			const FLinearColor RingColor = Marker.bLocked ? LockedGrey : MapEmber;
			const float RingAlpha = Marker.bLocked ? 0.45f : 0.9f;
			const int32 Newest = FMath::FloorToInt(SinceSelect / PingInterval);
			for (int32 Ring = Newest; Ring >= 0 && SinceSelect - Ring * PingInterval < PingLifetime; --Ring)
			{
				const float Age = (SinceSelect - Ring * PingInterval) / PingLifetime;
				const float Radius = 6.f + Age * PingRadius;
				Points.Reset(RingSegments + 1);
				for (int32 Segment = 0; Segment <= RingSegments; ++Segment)
				{
					const float Angle = UE_TWO_PI * Segment / RingSegments;
					Points.Add(FVector2f(Position) + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
				}
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None,
					WithAlpha(RingColor, FMath::Pow(1.f - Age, 1.5f) * RingAlpha * Opacity), true, 1.5f + (1.f - Age) * 1.5f);
			}
		}

		// Diamond mark on the centre.
		const FLinearColor MarkColor = Marker.bLocked ? LockedGrey : (bSelected ? MapEmber : (bHovered ? MapWhite : MapFrost));
		constexpr float Mark = 6.f;
		Points = { FVector2f(Position) + FVector2f(0.f, -Mark), FVector2f(Position) + FVector2f(Mark, 0.f), FVector2f(Position) + FVector2f(0.f, Mark),
			FVector2f(Position) + FVector2f(-Mark, 0.f), FVector2f(Position) + FVector2f(0.f, -Mark) };
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, WithAlpha(MarkColor, Opacity), true, 1.5f);

		if (bSelected)
		{
			SelectedPosition = Position;
			bSelectedVisible = Bounds.ContainsPoint(Position);
		}
		if (!Marker.Label.IsEmpty() && Font.FontObject)
		{
			// The name sits under the region, past the widest wobble (about 1.2x the radius).
			const FVector2D TextSize = Measure->Measure(Marker.Label, Font);
			const FVector2f TextPosition(Position + FVector2D(-TextSize.X * 0.5, RegionPixels * 1.2 + 4.0));
			const FLinearColor TextColor = Marker.bLocked ? FrostDim : (bSelected ? MapEmber : (bHovered ? MapWhite : LabelColor));
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(TextPosition + FVector2f(1.f, 1.f))),
				Marker.Label, Font, ESlateDrawEffect::None, FLinearColor(0.f, 0.01f, 0.03f, 0.85f * Opacity));
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(TextPosition)),
				Marker.Label, Font, ESlateDrawEffect::None, WithAlpha(TextColor, Opacity));
		}
	}

	// Quest pins share the region's map coordinates, so dragging and zooming cannot separate them from the land.
	const FSlateFontInfo PinFont = QuestFont(LabelTextStyle);
	const FSlateBrush* const PinBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	for (int32 Index = 0; Index < QuestMarkers.Num(); ++Index)
	{
		const FKnightMapQuestMarker& Marker = QuestMarkers[Index];
		const FVector2D Position = ToLocal(Marker.Location, View);
		const FVector2D PinSize = QuestPinSize(Marker.Label, PinFont);
		const FVector2D TopLeft = Position - PinSize * 0.5;
		if (!Bounds.ContainsPoint(Position)) continue;
		const bool bSelected = Marker.QuestId == SelectedQuestId;
		const bool bHovered = Index == HoveredQuestIndex;
		const FLinearColor Edge = bSelected ? MapEmber : (Marker.bLocked ? LockedGrey : (bHovered ? MapWhite : MapFrost));
		FSlateDrawElement::MakeBox(OutDrawElements, ++LayerId,
			AllottedGeometry.ToPaintGeometry(FVector2f(PinSize), FSlateLayoutTransform(FVector2f(TopLeft))),
			PinBrush, ESlateDrawEffect::None, WithAlpha(Edge, Opacity));
		FSlateDrawElement::MakeBox(OutDrawElements, ++LayerId,
			AllottedGeometry.ToPaintGeometry(FVector2f(PinSize - FVector2D(2.0)), FSlateLayoutTransform(FVector2f(TopLeft + FVector2D(1.0)))),
			PinBrush, ESlateDrawEffect::None, WithAlpha(FLinearColor(FColor(15, 25, 35)), 0.94f * Opacity));
		const FVector2D TextSize = Measure->Measure(Marker.Label, PinFont);
		const FVector2D TextPosition = Position - TextSize * 0.5;
		FSlateDrawElement::MakeText(OutDrawElements, ++LayerId,
			AllottedGeometry.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(FVector2f(TextPosition))),
			Marker.Label, PinFont, ESlateDrawEffect::None, WithAlpha(Marker.bLocked ? FrostDim : (bSelected ? MapEmber : MapWhite), Opacity));
	}

	// Thin frame with brighter corner ticks.
	const FVector2f Size(LocalSize);
	Points = { FVector2f(0.5f, 0.5f), FVector2f(Size.X - 0.5f, 0.5f), FVector2f(Size.X - 0.5f, Size.Y - 0.5f), FVector2f(0.5f, Size.Y - 0.5f), FVector2f(0.5f, 0.5f) };
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, WithAlpha(MapFrost, 0.3f * Opacity), true, 1.f);
	constexpr float Tick = 18.f;
	for (const FVector2f& Corner : { FVector2f(1.f, 1.f), FVector2f(Size.X - 1.f, 1.f), FVector2f(Size.X - 1.f, Size.Y - 1.f), FVector2f(1.f, Size.Y - 1.f) })
	{
		const FVector2f Inward(Corner.X < Size.X * 0.5f ? 1.f : -1.f, Corner.Y < Size.Y * 0.5f ? 1.f : -1.f);
		Points = { Corner + FVector2f(Inward.X * Tick, 0.f), Corner, Corner + FVector2f(0.f, Inward.Y * Tick) };
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, WithAlpha(MapFrost, 0.9f * Opacity), true, 2.f);
	}
	OutDrawElements.PopClip();

	// Leader line: mark → diagonal → short shelf → panel edge (same shape as the lock-on marker).
	// The panel rectangle must come from its paint-space geometry like AllottedGeometry; its tick-space (cached) geometry is in
	// desktop coordinates and only lines up when the window sits at the desktop origin.
	const UWidget* const Target = LeaderTarget.Get();
	if (bSelectedVisible && Target && Target->IsVisible())
	{
		const FGeometry& TargetGeometry = Target->GetPaintSpaceGeometry();
		const FVector2D TopLeft = AllottedGeometry.AbsoluteToLocal(TargetGeometry.GetAbsolutePosition());
		const FVector2D BottomRight = AllottedGeometry.AbsoluteToLocal(TargetGeometry.GetAbsolutePosition() + TargetGeometry.GetAbsoluteSize());
		if (BottomRight.X > TopLeft.X && BottomRight.Y > TopLeft.Y)
		{
			const bool bTargetOnRight = TopLeft.X > SelectedPosition.X;
			const float Side = bTargetOnRight ? 1.f : -1.f;
			const float EdgeX = static_cast<float>(bTargetOnRight ? TopLeft.X : BottomRight.X);
			const float AnchorY = FMath::Clamp(static_cast<float>(SelectedPosition.Y), static_cast<float>(TopLeft.Y) + 24.f, static_cast<float>(BottomRight.Y) - 24.f);
			Points = { FVector2f(SelectedPosition) + FVector2f(Side * 10.f, 0.f), FVector2f(EdgeX - Side * 36.f, AnchorY), FVector2f(EdgeX, AnchorY) };
			FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, WithAlpha(MapEmber, 0.9f * Opacity), true, 1.5f);
		}
	}
	return LayerId;
}
