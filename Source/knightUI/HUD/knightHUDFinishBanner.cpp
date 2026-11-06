#include "HUD/knightHUDFinishBanner.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"

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
	constexpr float StatsTime = 1.6f;
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
	FailLabel = LOCTEXT("Fail", "임무 실패");
	// 프로젝트 UI 폰트(IBM Plex Sans KR). 못 찾으면 엔진 기본 폰트로 그린다.
	static ConstructorHelpers::FObjectFinder<UObject> PlexFont(TEXT("/Game/Knight/UI/Style/Fonts/F_Knight_PlexKR.F_Knight_PlexKR"));
	const auto MakeFont = [](UObject* Font, const FName Typeface, int32 Size, int32 LetterSpacing)
	{
		FSlateFontInfo Info = Font ? FSlateFontInfo(Font, Size, Typeface) : FCoreStyle::GetDefaultFontStyle(Typeface, Size);
		Info.LetterSpacing = LetterSpacing;
		return Info;
	};
	ConfirmFont = MakeFont(PlexFont.Object, TEXT("Bold"), 34, 250);
	LabelFont = MakeFont(PlexFont.Object, TEXT("Medium"), 11, 200);
	TimeFont = MakeFont(PlexFont.Object, TEXT("SemiBold"), 20, 80);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UknightHUDFinishBanner::Setup(const FText& InTargetName, float InClearTime, float InBannerTime, bool bInFailure, const FText& InFailStatus)
{
	TargetName = InTargetName;
	ClearTime = InClearTime;
	BannerTime = InBannerTime;
	bFailure = bInFailure;
	FailStatus = InFailStatus;
}

void UknightHUDFinishBanner::BeginRise()
{
	if (RiseStartTime <= 0.0)
	{
		RiseStartTime = FPlatformTime::Seconds();
	}
}

float UknightHUDFinishBanner::GetRiseAlpha() const
{
	return RiseStartTime > 0.0 ? knightHUDDraw::EaseOut(static_cast<float>(FPlatformTime::Seconds() - RiseStartTime) / RiseTime) : 0.f;
}

void UknightHUDFinishBanner::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// 창 가운데를 축으로 줄이고, 창 위 끝이 RisenTop에 오도록 올린다. 그리는 좌표는 그대로 두고 위젯 전체를 옮긴다.
	const float Rise = GetRiseAlpha();
	if (Rise <= 0.f)
	{
		return;
	}
	const float ScreenHeight = MyGeometry.GetLocalSize().Y;
	if (ScreenHeight <= 0.f)
	{
		return;
	}
	const float Scale = FMath::Lerp(1.f, RisenScale, Rise);
	const float FromCenter = ScreenHeight * PanelCenterY;
	const float ToCenter = RisenTop + PanelSize.Y * RisenScale * 0.5f;
	SetRenderTransformPivot(FVector2D(0.5f, PanelCenterY));
	SetRenderTransform(FWidgetTransform(FVector2D(0.f, (ToCenter - FromCenter) * Rise), FVector2D(Scale), FVector2D::ZeroVector, 0.f));
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
	// 배너 시간이 지나면 옅어진 채 결과 화면이 열릴 때까지 남고, 올라가면서 다시 선명해진다.
	const float Dim = E > BannerTime ? FMath::Lerp(1.f, DimOpacity, FMath::Clamp((E - BannerTime) / DimTime, 0.f, 1.f)) : 1.f;
	const float Opacity = FMath::Lerp(Dim, 1.f, GetRiseAlpha()) * knightHUDDraw::BootFlicker(E, 0.25f);
	// 실패 판독은 창 전체가 빨간 톤이다.
	const FLinearColor BaseAccent = bFailure ? SignalLostColor : AccentColor;
	const FLinearColor Accent = knightHUDDraw::WithOpacity(BaseAccent, Opacity);

	knightHUDDraw::FScreenLook Look;
	Look.GhostColor = GhostColor;
	Look.GhostOffset = 1.5f;

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

	// 2. 대상 이름과 생체 신호. 성공이면 파형이 뛰다가 평평해지며 SIGNAL LOST, 실패면 대상은 살아 있으니 계속 뛰고 실패 사유 코드.
	const FString Target = FString::Printf(TEXT("TARGET  >  %s"), *TargetName.ToString());
	const int32 Typed = FMath::Min(Target.Len(), FMath::FloorToInt32((E - ContentStart) / TypeInterval));
	DrawText(OutDrawElements, Layer + 4, AllottedGeometry, TL + FVector2D(26.f, 18.f), FText::FromString(Target.Left(Typed)), LabelFont, Accent, 1.5f, false);

	const bool bLost = E >= SignalLostTime;
	const FText Status = !bLost ? LOCTEXT("Scanning", "VITAL SCAN") : bFailure ? FailStatus : LOCTEXT("SignalLost", "SIGNAL LOST");
	const float StatusBlink = bLost || FMath::Frac(E * 3.f) < 0.6f ? 1.f : 0.35f;
	const FVector2D StatusSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Status, LabelFont);
	DrawText(OutDrawElements, Layer + 4, AllottedGeometry, FVector2D(TL.X + PanelSize.X - 26.f - StatusSize.X, TL.Y + 18.f), Status, LabelFont,
		knightHUDDraw::WithOpacity(bLost ? SignalLostColor : AccentColor, Opacity * StatusBlink), 1.5f, false);

	const float Flat = bFailure ? 0.f : FMath::SmoothStep(0.f, 1.f, FMath::Clamp((E - FlatlineStart) / FlatlineTime, 0.f, 1.f));
	const FLinearColor WaveColor = knightHUDDraw::WithOpacity(FMath::Lerp(BaseAccent, SignalLostColor, Flat), Opacity * FMath::Lerp(1.f, 0.7f, Flat));
	TArray<FVector2f> Wave;
	const float WaveY = TL.Y + 66.f;
	for (float X = TL.X + 26.f; X <= TL.X + PanelSize.X - 26.f; X += 4.f)
	{
		Wave.Add(FVector2f(X, WaveY - 22.f * (1.f - Flat) * Pulse((X - TL.X) / 180.f - E * 1.4f)));
	}
	knightHUDDraw::ScreenLines(Look, OutDrawElements, Layer + 4, AllottedGeometry, Wave, WaveColor, 1.5f);

	// 3. 토벌 확인(실패는 임무 실패). 처음 잠깐 지직거리며 찍힌다.
	if (E >= ConfirmTime)
	{
		const float Since = E - ConfirmTime;
		const bool bGlitch = Since < ConfirmGlitchTime;
		const float Ghost = bGlitch ? 10.f * FrameNoise(E, 1) : 2.f;
		const FVector2D Jitter = bGlitch ? FVector2D(4.f * FrameNoise(E, 2), 0.f) : FVector2D::ZeroVector;
		const float Flicker = knightHUDDraw::BootFlicker(Since, 0.2f);
		DrawText(OutDrawElements, Layer + 5, AllottedGeometry, FVector2D(Center.X, TL.Y + 116.f) + Jitter, bFailure ? FailLabel : ConfirmLabel, ConfirmFont,
			knightHUDDraw::WithOpacity(FLinearColor::White, Opacity * Flicker), Ghost, true);
	}
	// 4. 소요 시간. 라벨은 작게, 숫자는 크게 한 줄로 가운데에 놓는다.
	if (E >= StatsTime)
	{
		const int32 Minutes = FMath::FloorToInt32(ClearTime / 60.f);
		const float Seconds = ClearTime - Minutes * 60.f;
		const FText Label = bFailure ? LOCTEXT("ElapsedLabel", "ELAPSED") : LOCTEXT("ClearTimeLabel", "CLEAR TIME");
		const FText Value = FText::FromString(FString::Printf(TEXT("%02d:%05.2f"), Minutes, Seconds));
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const FVector2D LabelSize = Measure->Measure(Label, LabelFont);
		const FVector2D ValueSize = Measure->Measure(Value, TimeFont);
		const float Gap = 16.f;
		const float Left = Center.X - (LabelSize.X + Gap + ValueSize.X) * 0.5f;
		const float RowY = TL.Y + PanelSize.Y - 34.f;
		const float Flicker = knightHUDDraw::BootFlicker(E - StatsTime, 0.2f);
		DrawText(OutDrawElements, Layer + 5, AllottedGeometry, FVector2D(Left, RowY - LabelSize.Y * 0.5f), Label, LabelFont,
			knightHUDDraw::WithOpacity(SubLabelColor, Opacity * Flicker), 0.f, false);
		DrawText(OutDrawElements, Layer + 5, AllottedGeometry, FVector2D(Left + LabelSize.X + Gap, RowY - ValueSize.Y * 0.5f), Value, TimeFont,
			knightHUDDraw::WithOpacity(BaseAccent, Opacity * Flicker), 1.5f, false);
	}
	return Layer + 6;
}

#undef LOCTEXT_NAMESPACE
