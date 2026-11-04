#include "HUD/knightHUDAttackWarning.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Combat/knightAttackWarningSubsystem.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "knightHUDAttackWarning"

namespace knightAttackWarningDraw
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
}

UknightHUDAttackWarning::UknightHUDAttackWarning(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LabelFont = FCoreStyle::GetDefaultFontStyle("Mono", 11);
	LabelFont.LetterSpacing = 200;
	SubLabelFont = FCoreStyle::GetDefaultFontStyle("Mono", 9);
	SubLabelFont.LetterSpacing = 150;
	WarningLabel = LOCTEXT("Warning", "! WARNING");
	SubLabel = LOCTEXT("NoParry", "NO PARRY");
}

void UknightHUDAttackWarning::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UknightHUDAttackWarning::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const UWorld* const World = GetWorld();
	const UknightAttackWarningSubsystem* const Warnings = World ? World->GetSubsystem<UknightAttackWarningSubsystem>() : nullptr;
	if (!Warnings)
	{
		return;
	}
	TSet<int32> Active;
	for (const FknightActiveAttackWarning& Warning : Warnings->GetWarnings())
	{
		Active.Add(Warning.Id);
		if (!SeenWarnings.Contains(Warning.Id) && WarningSound)
		{
			UGameplayStatics::PlaySound2D(this, WarningSound);
		}
	}
	SeenWarnings = MoveTemp(Active);
}

void UknightHUDAttackWarning::CollectMarkers(const FVector2D& Size, TArray<FMarker>& OutMarkers) const
{
	APlayerController* const PC = GetOwningPlayer();
	const UWorld* const World = GetWorld();
	const UknightAttackWarningSubsystem* const Warnings = World ? World->GetSubsystem<UknightAttackWarningSubsystem>() : nullptr;
	if (!PC || !PC->PlayerCameraManager || !Warnings)
	{
		return;
	}
	const FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
	const FRotator CameraRotation = PC->PlayerCameraManager->GetCameraRotation();
	// 픽셀을 슬레이트 단위로 바꿔 1080p 기준 값이 어느 해상도에서나 같게 보이게 한다.
	const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
	for (const FknightActiveAttackWarning& Warning : Warnings->GetWarnings())
	{
		FMarker& Marker = OutMarkers.AddDefaulted_GetRef();
		Marker.Elapsed = Warning.Elapsed;
		Marker.Remaining = Warning.GetRemaining();

		const FVector World3D = Warning.GetLocation();
		const FVector Local = CameraRotation.UnrotateVector(World3D - CameraLocation);
		FVector2D Pixels;
		if (Local.X > 0.f && UGameplayStatics::ProjectWorldToScreen(PC, World3D, Pixels, true))
		{
			Marker.Position = Pixels / Scale;
			Marker.bOnScreen = Marker.Position.X >= 0.f && Marker.Position.Y >= 0.f && Marker.Position.X <= Size.X && Marker.Position.Y <= Size.Y;
		}
		if (!Marker.bOnScreen)
		{
			// 카메라 기준 오른쪽(Y)과 위(Z)로 방향을 잡는다. 뒤에 있어도 옆으로 돌아야 할 쪽을 가리킨다.
			FVector2D Direction(Local.Y, -Local.Z);
			if (!Direction.Normalize())
			{
				Direction = FVector2D(0.f, 1.f);
			}
			Marker.OffscreenDirection = Direction;
		}
	}
}

float UknightHUDAttackWarning::GetOpacity(const FMarker& Marker) const
{
	const float Intro = knightAttackWarningDraw::EaseOut(Marker.Elapsed / BoxInTime) * BaseOpacity;
	if (Marker.Elapsed < BootFlickerTime)
	{
		// 신호가 잡히듯 프레임 단위로 들쭉날쭉 켜진다.
		const uint32 Frame = static_cast<uint32>(Marker.Elapsed * 60.f);
		const float Noise = static_cast<float>((Frame * 2654435761u >> 16) & 0xFF) / 255.f;
		return Intro * (Noise > 0.4f ? 1.f : 0.15f);
	}
	if (Marker.Remaining <= FinalPhaseTime)
	{
		const bool bBright = FMath::Frac(Marker.Elapsed * FinalBlinkRate) < 0.5f;
		return Intro * (bBright ? 1.f : FinalBlinkMinOpacity);
	}
	return Intro * (0.85f + 0.15f * FMath::Sin(Marker.Elapsed * 8.f));
}

void UknightHUDAttackWarning::ScreenLines(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const TArray<FVector2f>& Points,
	const FLinearColor& Color, float Thickness) const
{
	// 조종석 디스플레이처럼 옆에 다른 색이 살짝 어긋나 겹친다.
	if (GhostOffset > 0.f)
	{
		TArray<FVector2f> Ghost = Points;
		for (FVector2f& Point : Ghost)
		{
			Point.X += GhostOffset;
		}
		knightAttackWarningDraw::Lines(Out, Layer, Geometry, Ghost, knightAttackWarningDraw::WithOpacity(GhostColor, Color.A), Thickness);
	}
	knightAttackWarningDraw::Lines(Out, Layer, Geometry, Points, Color, Thickness);
}

void UknightHUDAttackWarning::ScreenText(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2D& Position, const FText& Label,
	const FSlateFontInfo& Font, const FLinearColor& Color) const
{
	if (GhostOffset > 0.f)
	{
		knightAttackWarningDraw::Text(Out, Layer, Geometry, Position + FVector2D(GhostOffset, 0.f), Label, Font, knightAttackWarningDraw::WithOpacity(GhostColor, Color.A));
	}
	knightAttackWarningDraw::Text(Out, Layer, Geometry, Position, Label, Font, Color);
}

void UknightHUDAttackWarning::PaintScanlines(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2D& Position, const FVector2D& Extent,
	float Opacity, float Time) const
{
	const FLinearColor Row = knightAttackWarningDraw::WithOpacity(WarningColor, Opacity * ScanlineOpacity);
	for (float Y = Position.Y + 1.f; Y < Position.Y + Extent.Y; Y += ScanlineSpacing)
	{
		knightAttackWarningDraw::Lines(Out, Layer, Geometry, { FVector2f(Position.X, Y), FVector2f(Position.X + Extent.X, Y) }, Row, 1.f);
	}
	// 위에서 아래로 훑고 지나가는 밝은 줄(화면 갱신).
	const float SweepY = Position.Y + FMath::Frac(Time / SweepPeriod) * Extent.Y;
	knightAttackWarningDraw::Lines(Out, Layer, Geometry, { FVector2f(Position.X, SweepY), FVector2f(Position.X + Extent.X, SweepY) }, knightAttackWarningDraw::WithOpacity(WarningColor, Opacity * 0.5f), 1.f);
}

int32 UknightHUDAttackWarning::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const FVector2D Size = FVector2D(AllottedGeometry.GetLocalSize());
	TArray<FMarker> Markers;
	CollectMarkers(Size, Markers);
	const float Tint = InWidgetStyle.GetColorAndOpacityTint().A;
	for (const FMarker& Marker : Markers)
	{
		const float Opacity = GetOpacity(Marker) * Tint;
		if (Marker.bOnScreen)
		{
			PaintBracket(Marker, AllottedGeometry, OutDrawElements, LayerId + 1, Opacity, Size);
		}
		else
		{
			PaintEdge(Marker, AllottedGeometry, OutDrawElements, LayerId + 1, Opacity, Size);
		}
	}
	return LayerId + 2;
}

void UknightHUDAttackWarning::PaintBracket(const FMarker& Marker, const FGeometry& Geometry, FSlateWindowElementList& Out, int32 Layer, float Opacity,
	const FVector2D& Size) const
{
	const FLinearColor Color = knightAttackWarningDraw::WithOpacity(WarningColor, Opacity);
	const float Intro = knightAttackWarningDraw::EaseOut(Marker.Elapsed / BoxInTime);
	const bool bFinal = Marker.Remaining <= FinalPhaseTime;
	const float Half = BoxSize * 0.5f * FMath::Lerp(BoxStartScale, 1.f, Intro) * (bFinal ? FinalBoxScale : 1.f);
	const FVector2f C(Marker.Position);
	const float Corner = FMath::Min(CornerLength, Half);

	// 모서리 브래킷 네 개.
	for (const FVector2f Sign : { FVector2f(-1.f, -1.f), FVector2f(1.f, -1.f), FVector2f(-1.f, 1.f), FVector2f(1.f, 1.f) })
	{
		const FVector2f Tip = C + Sign * Half;
		ScreenLines(Out, Layer, Geometry, { Tip - FVector2f(0.f, Sign.Y * Corner), Tip, Tip - FVector2f(Sign.X * Corner, 0.f) }, Color, LineThickness);
	}
	// 가운데 점.
	knightAttackWarningDraw::Box(Out, Layer, Geometry, FVector2D(C) - FVector2D(1.5f), FVector2D(3.f), Color);

	// 태그는 브래킷이 다 좁혀진 뒤에 뻗어 나온다.
	const float TagAlpha = knightAttackWarningDraw::EaseOut((Marker.Elapsed - BoxInTime * 0.6f) / BoxInTime);
	if (TagAlpha <= 0.f)
	{
		return;
	}
	const float Side = C.X > Size.X * FlipScreenFraction ? -1.f : 1.f;
	const FVector2f Start = C + FVector2f(Side * Half, -Half);
	const FVector2f Elbow = Start + FVector2f(Side * LeaderLength.X, -LeaderLength.X) * TagAlpha;
	const FVector2f End = Elbow + FVector2f(Side * LeaderLength.Y, 0.f) * TagAlpha;
	const FLinearColor TagColor = knightAttackWarningDraw::WithOpacity(WarningColor, Opacity * TagAlpha);
	ScreenLines(Out, Layer, Geometry, { Start, Elbow, End }, TagColor, 1.f);

	const FVector2D TagPosition = FVector2D(Side > 0.f ? End.X : End.X - TagSize.X, End.Y - TagSize.Y * 0.5f);
	knightAttackWarningDraw::Box(Out, Layer, Geometry, TagPosition, TagSize, knightAttackWarningDraw::WithOpacity(PlateColor, Opacity * TagAlpha));
	PaintScanlines(Out, Layer, Geometry, TagPosition, TagSize, Opacity * TagAlpha, Marker.Elapsed);
	const FVector2f TL(TagPosition);
	const FVector2f BR = TL + FVector2f(TagSize);
	ScreenLines(Out, Layer, Geometry, { TL, FVector2f(BR.X, TL.Y), BR, FVector2f(TL.X, BR.Y), TL }, TagColor, 1.f);
	// 테두리는 비쳐도 글자는 밝은 배경에서도 읽혀야 하므로 BaseOpacity를 빼고 그린다.
	const float TextOpacity = FMath::Min(1.f, Opacity / FMath::Max(BaseOpacity, KINDA_SMALL_NUMBER)) * TagAlpha;
	ScreenText(Out, Layer + 1, Geometry, TagPosition + FVector2D(8.f, 2.f), WarningLabel, LabelFont, knightAttackWarningDraw::WithOpacity(WarningColor, TextOpacity));

	// 경고 줄무늬와 NO PARRY.
	const float StripeY = BR.Y + 4.f;
	for (float X = TL.X; X + 6.f <= BR.X; X += 10.f)
	{
		ScreenLines(Out, Layer, Geometry, { FVector2f(X, StripeY + 5.f), FVector2f(X + 5.f, StripeY) }, TagColor, 1.5f);
	}
	ScreenText(Out, Layer + 1, Geometry, FVector2D(TL.X, StripeY + 8.f), SubLabel, SubLabelFont, knightAttackWarningDraw::WithOpacity(SubLabelColor, TextOpacity));
}

void UknightHUDAttackWarning::PaintEdge(const FMarker& Marker, const FGeometry& Geometry, FSlateWindowElementList& Out, int32 Layer, float Opacity,
	const FVector2D& Size) const
{
	const FLinearColor Color = knightAttackWarningDraw::WithOpacity(WarningColor, Opacity);
	const FVector2D Center = Size * 0.5f;
	const FVector2D Reach = (Center - EdgeMargin).ComponentMax(FVector2D(1.f));
	const FVector2D D = Marker.OffscreenDirection;
	// 안쪽 사각형의 가장자리와 만나는 점.
	const float T = FMath::Min(FMath::Abs(D.X) > KINDA_SMALL_NUMBER ? Reach.X / FMath::Abs(D.X) : UE_BIG_NUMBER,
		FMath::Abs(D.Y) > KINDA_SMALL_NUMBER ? Reach.Y / FMath::Abs(D.Y) : UE_BIG_NUMBER);
	const FVector2f P(Center + D * T);
	const FVector2f Forward(D);
	const FVector2f Right(-Forward.Y, Forward.X);

	// 바깥을 가리키는 겹화살표.
	const float Arrow = 14.f;
	for (const float Back : { 0.f, Arrow })
	{
		const FVector2f Tip = P + Forward * (Arrow - Back);
		ScreenLines(Out, Layer, Geometry, { Tip - Forward * Arrow + Right * Arrow, Tip, Tip - Forward * Arrow - Right * Arrow }, Color, 2.f);
	}
	// 글자는 화살표 안쪽(화면 중심 쪽)에 화살표와 겹치지 않게 둔다.
	const FText Label = FText::FromString(TEXT("WARNING"));
	const FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Label, SubLabelFont);
	const FVector2D TextCenter = FVector2D(P) - D * (Arrow * 2.f + 8.f + FMath::Abs(D.X) * TextSize.X * 0.5f + FMath::Abs(D.Y) * TextSize.Y * 0.5f);
	ScreenText(Out, Layer + 1, Geometry, TextCenter - TextSize * 0.5f, Label, SubLabelFont, Color);

	// 그쪽 화면 가장자리에 옅은 띠.
	const FLinearColor Band = knightAttackWarningDraw::WithOpacity(WarningColor, Opacity * 0.35f);
	const float Thickness = 8.f;
	if (FMath::Abs(D.X) * Reach.Y >= FMath::Abs(D.Y) * Reach.X)
	{
		knightAttackWarningDraw::Box(Out, Layer, Geometry, FVector2D(D.X > 0.f ? Size.X - Thickness : 0.f, 0.f), FVector2D(Thickness, Size.Y), Band);
	}
	else
	{
		knightAttackWarningDraw::Box(Out, Layer, Geometry, FVector2D(0.f, D.Y > 0.f ? Size.Y - Thickness : 0.f), FVector2D(Size.X, Thickness), Band);
	}
}

#undef LOCTEXT_NAMESPACE
