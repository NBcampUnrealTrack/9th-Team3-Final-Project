#include "HUD/knightHUDFinishBanner.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "knightHUDFinishBanner"

namespace knightFinishBanner
{
	/** 단계 시각(배너가 뜬 뒤 실제 초). */
	constexpr float LineTime = 0.35f;
	constexpr float OpenStart = 0.2f;
	constexpr float OpenTime = 0.35f;
	constexpr float ContentStart = 0.5f;
	constexpr float TypeInterval = 0.03f;
	constexpr float FlatlineStart = 0.85f;
	constexpr float FlatlineTime = 0.3f;
	constexpr float SignalLostTime = 1.15f;
	constexpr float ConfirmTime = 1.3f;
	constexpr float ConfirmGlitchTime = 0.35f;
	constexpr float SubLabelTime = 1.45f;
	constexpr float StatsTime = 1.7f;
	constexpr float SubtitleStart = 0.35f;
	constexpr float SubtitleInterval = 0.045f;
	constexpr float DimTime = 0.5f;
	constexpr float DimOpacity = 0.55f;

	/** 프레임마다 바뀌는 -1..1 잡음(지직거림). */
	float FrameNoise(float Elapsed, uint32 Salt)
	{
		const uint32 Frame = static_cast<uint32>(Elapsed * 60.f) + Salt * 7919u;
		return static_cast<float>((Frame * 2654435761u >> 16) & 0xFF) / 127.5f - 1.f;
	}

	/** 심전도처럼 한 주기(0..1)에 한 번 뛰는 파형. */
	float Pulse(float Phase)
	{
		const float U = FMath::Frac(Phase);
		if (U < 0.08f) return 0.f;
		if (U < 0.11f) return -0.25f * (U - 0.08f) / 0.03f;
		if (U < 0.14f) return FMath::Lerp(-0.25f, 1.f, (U - 0.11f) / 0.03f);
		if (U < 0.18f) return FMath::Lerp(1.f, -0.45f, (U - 0.14f) / 0.04f);
		if (U < 0.22f) return FMath::Lerp(-0.45f, 0.f, (U - 0.18f) / 0.04f);
		if (U < 0.34f) return 0.18f * FMath::Sin((U - 0.22f) / 0.12f * PI);
		return 0.f;
	}
}

UknightHUDFinishBanner::UknightHUDFinishBanner(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConfirmLabel = LOCTEXT("Confirm", "토벌 확인");
	ConfirmSubLabel = LOCTEXT("ConfirmSub", "HUNT CONFIRMED");
	VoiceSubtitle = LOCTEXT("Voice", "[AI] 목표 생명 반응 소실 확인. 토벌 성공입니다.");
	ConfirmFont = FCoreStyle::GetDefaultFontStyle("Bold", 44);
	ConfirmFont.LetterSpacing = 250;
	LabelFont = FCoreStyle::GetDefaultFontStyle("Mono", 11);
	LabelFont.LetterSpacing = 200;
	SubtitleFont = FCoreStyle::GetDefaultFontStyle("Regular", 15);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UknightHUDFinishBanner::Setup(const FText& InTargetName, float InClearTime, int32 InBrokenParts, float InBannerTime)
{
	TargetName = InTargetName;
	ClearTime = InClearTime;
	BrokenParts = InBrokenParts;
	BannerTime = InBannerTime;
}

void UknightHUDFinishBanner::NativeConstruct()
{
	Super::NativeConstruct();
	StartTime = FPlatformTime::Seconds();
}

float UknightHUDFinishBanner::GetElapsed() const
{
	return static_cast<float>(FPlatformTime::Seconds() - StartTime);
}

void UknightHUDFinishBanner::DrawText(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2D& Position, const FText& Label,
	const FSlateFontInfo& Font, const FLinearColor& Color, float GhostOffset, bool bCentered) const
{
	const FVector2D Size = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Label, Font);
	const FVector2D TopLeft = bCentered ? Position - Size * 0.5 : Position;
	const auto Draw = [&](const FVector2D& At, const FLinearColor& Tint)
	{
		FSlateDrawElement::MakeText(Out, Layer, Geometry.ToPaintGeometry(FVector2f(Size + FVector2D(8.f, 4.f)), FSlateLayoutTransform(FVector2f(At))), Label, Font,
			ESlateDrawEffect::None, Tint);
	};
	if (GhostOffset != 0.f)
	{
		Draw(TopLeft + FVector2D(GhostOffset, 0.f), knightHUDDraw::WithOpacity(GhostColor, Color.A));
	}
	Draw(TopLeft, Color);
}

int32 UknightHUDFinishBanner::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	using namespace knightFinishBanner;
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const float E = GetElapsed();
	const FVector2D Screen = AllottedGeometry.GetLocalSize();
	const FVector2D Center(Screen.X * 0.5, Screen.Y * PanelCenterY);
	// 배너 시간이 지나면 옅어진 채 결과 화면이 열릴 때까지 남는다.
	const float Dim = E > BannerTime ? FMath::Lerp(1.f, DimOpacity, FMath::Clamp((E - BannerTime) / DimTime, 0.f, 1.f)) : 1.f;
	const float Opacity = Dim * knightHUDDraw::BootFlicker(E, 0.25f);
	const FLinearColor Accent = knightHUDDraw::WithOpacity(AccentColor, Opacity);

	knightHUDDraw::FScreenLook Look;
	Look.GhostColor = GhostColor;
	Look.GhostOffset = 1.5f;
	Look.ScanlineSpacing = 3.f;
	Look.ScanlineOpacity = 0.18f;
	Look.SweepPeriod = 1.6f;

	// 1. 가운데 가로선이 펼쳐지고, 판독 창이 위아래로 열린다.
	const float LineWidth = PanelSize.X * knightHUDDraw::EaseOut(E / LineTime);
	const float Height = PanelSize.Y * knightHUDDraw::EaseOut((E - OpenStart) / OpenTime);
	const FVector2D TL(Center.X - PanelSize.X * 0.5, Center.Y - Height * 0.5);
	if (Height < PanelSize.Y)
	{
		knightHUDDraw::ScreenLines(Look, OutDrawElements, Layer + 3, AllottedGeometry,
			{ FVector2f(Center.X - LineWidth * 0.5, Center.Y), FVector2f(Center.X + LineWidth * 0.5, Center.Y) }, Accent, 1.5f);
	}
	if (Height > 1.f)
	{
		knightHUDDraw::Box(OutDrawElements, Layer + 1, AllottedGeometry, TL, FVector2D(PanelSize.X, Height), knightHUDDraw::WithOpacity(PlateColor, Opacity));
		knightHUDDraw::Scanlines(Look, OutDrawElements, Layer + 2, AllottedGeometry, TL, FVector2D(PanelSize.X, Height), AccentColor, Opacity, E);
		const float Arm = 28.f;
		const FVector2D BR = TL + FVector2D(PanelSize.X, Height);
		for (const FVector2D& Corner : { TL, FVector2D(BR.X, TL.Y), BR, FVector2D(TL.X, BR.Y) })
		{
			const float SX = Corner.X < Center.X ? 1.f : -1.f;
			const float SY = Corner.Y < Center.Y ? 1.f : -1.f;
			knightHUDDraw::ScreenLines(Look, OutDrawElements, Layer + 3, AllottedGeometry,
				{ FVector2f(Corner.X + SX * Arm, Corner.Y), FVector2f(Corner.X, Corner.Y), FVector2f(Corner.X, Corner.Y + SY * FMath::Min(Arm, Height * 0.5f)) }, Accent, 2.f);
		}
	}
	if (E < ContentStart)
	{
		return Layer + 4;
	}

	// 2. 대상 이름과 생체 신호. 파형이 뛰다가 평평해지면 SIGNAL LOST.
	const FString Target = FString::Printf(TEXT("TARGET  >  %s"), *TargetName.ToString());
	const int32 Typed = FMath::Min(Target.Len(), FMath::FloorToInt32((E - ContentStart) / TypeInterval));
	DrawText(OutDrawElements, Layer + 4, AllottedGeometry, TL + FVector2D(26.f, 18.f), FText::FromString(Target.Left(Typed)), LabelFont, Accent, 1.5f, false);

	const bool bLost = E >= SignalLostTime;
	const FText Status = bLost ? LOCTEXT("SignalLost", "SIGNAL LOST") : LOCTEXT("Scanning", "VITAL SCAN");
	const float StatusBlink = bLost || FMath::Frac(E * 3.f) < 0.6f ? 1.f : 0.35f;
	const FVector2D StatusSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Status, LabelFont);
	DrawText(OutDrawElements, Layer + 4, AllottedGeometry, FVector2D(TL.X + PanelSize.X - 26.f - StatusSize.X, TL.Y + 18.f), Status, LabelFont,
		knightHUDDraw::WithOpacity(bLost ? SignalLostColor : AccentColor, Opacity * StatusBlink), 1.5f, false);

	const float Flat = FMath::SmoothStep(0.f, 1.f, FMath::Clamp((E - FlatlineStart) / FlatlineTime, 0.f, 1.f));
	const FLinearColor WaveColor = knightHUDDraw::WithOpacity(FMath::Lerp(AccentColor, SignalLostColor, Flat), Opacity * FMath::Lerp(1.f, 0.7f, Flat));
	TArray<FVector2f> Wave;
	const float WaveY = TL.Y + 66.f;
	for (float X = TL.X + 26.f; X <= TL.X + PanelSize.X - 26.f; X += 4.f)
	{
		Wave.Add(FVector2f(X, WaveY - 22.f * (1.f - Flat) * Pulse((X - TL.X) / 180.f - E * 1.4f)));
	}
	knightHUDDraw::ScreenLines(Look, OutDrawElements, Layer + 4, AllottedGeometry, Wave, WaveColor, 1.5f);

	// 3. 토벌 확인. 처음 잠깐 지직거리며 찍힌다.
	if (E >= ConfirmTime)
	{
		const float Since = E - ConfirmTime;
		const bool bGlitch = Since < ConfirmGlitchTime;
		const float Ghost = bGlitch ? 10.f * FrameNoise(E, 1) : 2.f;
		const FVector2D Jitter = bGlitch ? FVector2D(4.f * FrameNoise(E, 2), 0.f) : FVector2D::ZeroVector;
		const float Flicker = knightHUDDraw::BootFlicker(Since, 0.2f);
		DrawText(OutDrawElements, Layer + 5, AllottedGeometry, FVector2D(Center.X, TL.Y + 128.f) + Jitter, ConfirmLabel, ConfirmFont,
			knightHUDDraw::WithOpacity(FLinearColor::White, Opacity * Flicker), Ghost, true);
	}
	if (E >= SubLabelTime)
	{
		DrawText(OutDrawElements, Layer + 5, AllottedGeometry, FVector2D(Center.X, TL.Y + 172.f), ConfirmSubLabel, LabelFont, Accent, 1.5f, true);
	}
	if (E >= StatsTime)
	{
		const int32 Minutes = FMath::FloorToInt32(ClearTime / 60.f);
		const float Seconds = ClearTime - Minutes * 60.f;
		const FText Stats = FText::FromString(FString::Printf(TEXT("CLEAR TIME %02d:%04.1f     PARTS BROKEN %d"), Minutes, Seconds, BrokenParts));
		DrawText(OutDrawElements, Layer + 5, AllottedGeometry, FVector2D(Center.X, TL.Y + PanelSize.Y - 22.f), Stats, LabelFont,
			knightHUDDraw::WithOpacity(SubLabelColor, Opacity), 0.f, true);
	}

	// 4. [AI] 음성 자막. 가운데 맞춤은 전체 문장 폭으로 정해 두고 한 글자씩 찍는다.
	if (E >= SubtitleStart)
	{
		const FString Line = VoiceSubtitle.ToString();
		const int32 Shown = FMath::Min(Line.Len(), FMath::FloorToInt32((E - SubtitleStart) / SubtitleInterval));
		const FVector2D Full = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(VoiceSubtitle, SubtitleFont);
		const FVector2D At(Center.X - Full.X * 0.5, Center.Y + PanelSize.Y * 0.5 + 22.f);
		DrawText(OutDrawElements, Layer + 5, AllottedGeometry, At, FText::FromString(Line.Left(Shown)), SubtitleFont,
			knightHUDDraw::WithOpacity(FLinearColor::White, Opacity * 0.92f), 0.f, false);
	}
	return Layer + 6;
}

#undef LOCTEXT_NAMESPACE
