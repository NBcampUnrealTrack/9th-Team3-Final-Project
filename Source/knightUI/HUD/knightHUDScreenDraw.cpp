#include "HUD/knightHUDScreenDraw.h"

#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace knightHUDDraw
{
	float EaseOut(float Alpha)
	{
		return 1.f - FMath::Pow(1.f - FMath::Clamp(Alpha, 0.f, 1.f), 3.f);
	}

	FLinearColor WithOpacity(FLinearColor Color, float Opacity)
	{
		Color.A *= Opacity;
		return Color;
	}

	void Lines(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const TArray<FVector2f>& Points, const FLinearColor& Color, float Thickness)
	{
		FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Thickness);
	}

	void Box(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color)
	{
		FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Position))),
			FCoreStyle::Get().GetBrush("GenericWhiteBox"), ESlateDrawEffect::None, Color);
	}

	void Text(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2D& Position, const FText& Label, const FSlateFontInfo& Font,
		const FLinearColor& Color)
	{
		FSlateDrawElement::MakeText(Out, Layer, Geometry.ToPaintGeometry(FVector2f(400.f, 40.f), FSlateLayoutTransform(FVector2f(Position))), Label, Font,
			ESlateDrawEffect::None, Color);
	}

	void ScreenLines(const FScreenLook& Look, FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const TArray<FVector2f>& Points,
		const FLinearColor& Color, float Thickness)
	{
		if (Look.GhostOffset > 0.f)
		{
			TArray<FVector2f> Ghost = Points;
			for (FVector2f& Point : Ghost)
			{
				Point.X += Look.GhostOffset;
			}
			Lines(Out, Layer, Geometry, Ghost, WithOpacity(Look.GhostColor, Color.A), Thickness);
		}
		Lines(Out, Layer, Geometry, Points, Color, Thickness);
	}

	void ScreenText(const FScreenLook& Look, FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2D& Position, const FText& Label,
		const FSlateFontInfo& Font, const FLinearColor& Color)
	{
		if (Look.GhostOffset > 0.f)
		{
			Text(Out, Layer, Geometry, Position + FVector2D(Look.GhostOffset, 0.f), Label, Font, WithOpacity(Look.GhostColor, Color.A));
		}
		Text(Out, Layer, Geometry, Position, Label, Font, Color);
	}

	void Scanlines(const FScreenLook& Look, FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2D& Position, const FVector2D& Extent,
		const FLinearColor& Color, float Opacity, float Time)
	{
		const FLinearColor Row = WithOpacity(Color, Opacity * Look.ScanlineOpacity);
		for (float Y = Position.Y + 1.f; Y < Position.Y + Extent.Y; Y += FMath::Max(Look.ScanlineSpacing, 1.f))
		{
			Lines(Out, Layer, Geometry, { FVector2f(Position.X, Y), FVector2f(Position.X + Extent.X, Y) }, Row, 1.f);
		}
		const float SweepY = Position.Y + FMath::Frac(Time / FMath::Max(Look.SweepPeriod, 0.1f)) * Extent.Y;
		Lines(Out, Layer, Geometry, { FVector2f(Position.X, SweepY), FVector2f(Position.X + Extent.X, SweepY) }, WithOpacity(Color, Opacity * 0.5f), 1.f);
	}

	float BootFlicker(float Elapsed, float FlickerTime)
	{
		if (Elapsed >= FlickerTime)
		{
			return 1.f;
		}
		const uint32 Frame = static_cast<uint32>(Elapsed * 60.f);
		const float Noise = static_cast<float>((Frame * 2654435761u >> 16) & 0xFF) / 255.f;
		return Noise > 0.4f ? 1.f : 0.15f;
	}
}
