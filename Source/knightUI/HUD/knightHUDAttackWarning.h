#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "HUD/knightHUDScreenDraw.h"
#include "knightHUDAttackWarning.generated.h"

class USoundBase;

/**
 * 가드·패링 불가 공격 경고 마커(#198). UknightAttackWarningSubsystem에 떠 있는 예고마다 공격 부위 위에 붉은 모서리 브래킷과
 * "! WARNING" 태그(경고 줄무늬, NO PARRY)를 그린다.
 *
 * - 시작: 브래킷이 크게 나타나며 부위 쪽으로 좁혀지고 경고음이 한 번 난다.
 * - 판정 FinalPhaseTime 전: 깜빡임이 빨라지고 브래킷이 더 조여진다. 피할 타이밍이다.
 * - 판정: 마커가 사라진다.
 * 부위가 화면 밖(뒤 포함)이면 그 방향 화면 가장자리에 겹화살표와 WARNING 글자, 옅은 붉은 띠를 그린다.
 * 조종석 화면에 뜬 표시처럼 보이게 얇은 반투명 선에 색 번짐, 태그 주사선, 켜질 때 깜빡임을 넣는다(Screen 항목).
 *
 * 화면 전체를 덮고 위젯 트리 없이 NativePaint로 직접 그린다. 그리는 순간 카메라로 위치를 잡으므로 마커가 한 프레임 늦지 않는다.
 */
UCLASS(Blueprintable)
class KNIGHTUI_API UknightHUDAttackWarning : public UUserWidget
{
	GENERATED_BODY()

public:
	UknightHUDAttackWarning(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	/** 새 예고가 뜰 때 한 번(2D). */
	UPROPERTY(EditAnywhere, Category="Knight|HUD")
	TObjectPtr<USoundBase> WarningSound;

	/** HUD 스타일 가이드의 alert 빨강. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Style")
	FLinearColor WarningColor = FLinearColor(FColor(0xE5, 0x48, 0x4D));

	/** 태그 바탕(남색). 뒤 화면이 비쳐 보이게 옅게 둔다. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Style")
	FLinearColor PlateColor = FLinearColor(FColor(0x0A, 0x11, 0x1D, 0x66));

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Style")
	FLinearColor SubLabelColor = FLinearColor(FColor(0x8F, 0xA3, 0xBF));

	/** 마커 전체 불투명도. 조종석 화면에 비친 표시처럼 뒤가 비친다. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Screen", meta=(ClampMin=0, ClampMax=1))
	float BaseOpacity = 0.72f;

	/** 선 옆에 살짝 어긋나 겹치는 색(디스플레이 색 번짐). 알파가 세기다. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Screen")
	FLinearColor GhostColor = FLinearColor(FColor(0x5F, 0xC8, 0xF0, 0x40));

	/** 색 번짐이 어긋나는 거리(슬레이트 단위). 0이면 끈다. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Screen", meta=(ClampMin=0))
	float GhostOffset = 1.5f;

	/** 태그 위 주사선 간격과 세기. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Screen", meta=(ClampMin=1))
	float ScanlineSpacing = 3.f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Screen", meta=(ClampMin=0, ClampMax=1))
	float ScanlineOpacity = 0.22f;

	/** 태그 위를 위에서 아래로 훑고 지나가는 밝은 줄의 주기. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Screen", meta=(ClampMin=0.1, Units="s"))
	float SweepPeriod = 0.9f;

	/** 켜질 때 신호가 잡히듯 깜빡이는 시간. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Screen", meta=(ClampMin=0, Units="s"))
	float BootFlickerTime = 0.18f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Style")
	FSlateFontInfo LabelFont;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Style")
	FSlateFontInfo SubLabelFont;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Style")
	FText WarningLabel;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Style")
	FText SubLabel;

	/** 다 좁혀졌을 때 브래킷 한 변(1080p 기준 슬레이트 단위). */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout", meta=(ClampMin=8))
	float BoxSize = 64.f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout", meta=(ClampMin=2))
	float CornerLength = 16.f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout", meta=(ClampMin=0.5))
	float LineThickness = 1.25f;

	/** 브래킷 모서리에서 태그까지 비스듬히 오르는 선 길이와 그 끝의 가로 선 길이. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout")
	FVector2D LeaderLength = FVector2D(18.f, 18.f);

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout")
	FVector2D TagSize = FVector2D(118.f, 22.f);

	/** 부위가 화면 오른쪽 이 비율보다 바깥이면 태그를 왼쪽으로 뒤집는다. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout", meta=(ClampMin=0, ClampMax=1))
	float FlipScreenFraction = 0.62f;

	/** 화면 밖 표시가 가장자리에서 떨어지는 거리. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout")
	FVector2D EdgeMargin = FVector2D(56.f, 48.f);

	/** 등장할 때 브래킷 배율(1로 좁혀진다). */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Motion", meta=(ClampMin=1))
	float BoxStartScale = 2.6f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Motion", meta=(ClampMin=0.01, Units="s"))
	float BoxInTime = 0.25f;

	/** 판정까지 이 시간이 남으면 빠르게 깜빡이고 브래킷을 조인다. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Motion", meta=(ClampMin=0, Units="s"))
	float FinalPhaseTime = 0.3f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Motion", meta=(ClampMin=0.1, ClampMax=1))
	float FinalBoxScale = 0.78f;

	/** 마지막 구간 깜빡임 횟수(초당). */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Motion", meta=(ClampMin=1))
	float FinalBlinkRate = 8.f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Motion", meta=(ClampMin=0, ClampMax=1))
	float FinalBlinkMinOpacity = 0.35f;

private:
	struct FMarker
	{
		FVector2D Position = FVector2D::ZeroVector;
		/** 화면 밖이면 화면 중심에서 부위 쪽으로의 방향(화면 좌표, 정규화). */
		FVector2D OffscreenDirection = FVector2D::ZeroVector;
		bool bOnScreen = false;
		float Elapsed = 0.f;
		float Remaining = 0.f;
	};

	void CollectMarkers(const FVector2D& Size, TArray<FMarker>& OutMarkers) const;
	void PaintBracket(const FMarker& Marker, const FGeometry& Geometry, FSlateWindowElementList& Out, int32 Layer, float Opacity, const FVector2D& Size) const;
	void PaintEdge(const FMarker& Marker, const FGeometry& Geometry, FSlateWindowElementList& Out, int32 Layer, float Opacity, const FVector2D& Size) const;
	float GetOpacity(const FMarker& Marker) const;
	knightHUDDraw::FScreenLook GetScreenLook() const;
	/** 선·글자를 색 번짐(GhostColor)과 함께 그린다. */
	void ScreenLines(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const TArray<FVector2f>& Points, const FLinearColor& Color, float Thickness) const;
	void ScreenText(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2D& Position, const FText& Label, const FSlateFontInfo& Font,
		const FLinearColor& Color) const;
	/** 사각형 안에 주사선과 훑고 지나가는 밝은 줄을 그린다. */
	void PaintScanlines(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2D& Position, const FVector2D& Extent, float Opacity,
		float Time) const;

	TSet<int32> SeenWarnings;
};
