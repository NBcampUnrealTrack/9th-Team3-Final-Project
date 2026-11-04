#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

class FSlateWindowElementList;
struct FGeometry;

/**
 * 조종석 화면 질감으로 그리는 HUD 마커(공격 경고, 몬스터 상태 판독)가 같이 쓰는 그리기 함수.
 * 얇은 선 옆에 다른 색이 살짝 어긋나 겹치고(색 번짐), 바탕에 주사선과 훑고 지나가는 밝은 줄이 지나간다.
 */
namespace knightHUDDraw
{
	struct FScreenLook
	{
		/** 선 옆에 어긋나 겹치는 색. 알파가 세기다. */
		FLinearColor GhostColor;
		/** 색 번짐이 어긋나는 거리(슬레이트 단위). 0이면 끈다. */
		float GhostOffset = 0.f;
		float ScanlineSpacing = 3.f;
		float ScanlineOpacity = 0.f;
		float SweepPeriod = 1.f;
	};

	float EaseOut(float Alpha);
	FLinearColor WithOpacity(FLinearColor Color, float Opacity);

	void Lines(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const TArray<FVector2f>& Points, const FLinearColor& Color, float Thickness);
	void Box(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color);
	void Text(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2D& Position, const FText& Label, const FSlateFontInfo& Font,
		const FLinearColor& Color);

	/** 선을 색 번짐과 함께 그린다. */
	void ScreenLines(const FScreenLook& Look, FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const TArray<FVector2f>& Points,
		const FLinearColor& Color, float Thickness);
	void ScreenText(const FScreenLook& Look, FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2D& Position, const FText& Label,
		const FSlateFontInfo& Font, const FLinearColor& Color);
	/** 사각형 안에 주사선과 위에서 아래로 훑고 지나가는 밝은 줄을 그린다. */
	void Scanlines(const FScreenLook& Look, FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2D& Position, const FVector2D& Extent,
		const FLinearColor& Color, float Opacity, float Time);

	/** 켜질 때 신호가 잡히듯 프레임 단위로 들쭉날쭉한 밝기(0.15 또는 1). FlickerTime이 지나면 1. */
	float BootFlicker(float Elapsed, float FlickerTime);
}
