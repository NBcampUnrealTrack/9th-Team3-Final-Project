#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "HUD/knightHUDScreenDraw.h"
#include "knightHUDFinishBanner.generated.h"

/**
 * 처치 연출의 토벌 확인 배너. 조종석 화면에 [AI] 판독이 뜨는 느낌으로, 위젯 트리 없이 NativePaint로 화면 가운데에 그린다.
 *
 * 1. 가운데 가로선이 펼쳐지고 모서리 브래킷이 위아래로 열린다(주사선이 흐르는 판독 창).
 * 2. 대상 이름이 한 글자씩 찍히고, 생체 신호 파형이 뛰다가 평평해지며 "SIGNAL LOST".
 * 3. 큰 글씨 "토벌 확인"이 지직거리며 찍히고 아래에 HUNT CONFIRMED, 소요 시간과 부위 파괴 수.
 * 4. 창 아래에 [AI] 음성 자막이 한 글자씩 나온다. BannerTime 뒤에는 옅어진 채 결과 화면이 열릴 때까지 남는다.
 *
 * AknightHUD가 처치 연출 단계 Banner에서 만들어 화면에 올리고, Done에서 지운다. 시간은 실제 시간(슬로모션·일시정지와 상관없음)이다.
 */
UCLASS()
class KNIGHTUI_API UknightHUDFinishBanner : public UUserWidget
{
	GENERATED_BODY()

public:
	UknightHUDFinishBanner(const FObjectInitializer& ObjectInitializer);

	/** 배너 내용. 만든 뒤 화면에 올리기 전에 부른다. */
	void Setup(const FText& InTargetName, float InClearTime, int32 InBrokenParts, float InBannerTime);

protected:
	virtual void NativeConstruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	/** HUD 스타일 가이드의 정보 색(하늘색)과 보조 글자 색, 창 바탕. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Style")
	FLinearColor AccentColor = FLinearColor(FColor(0x7F, 0xE3, 0xFF));

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Style")
	FLinearColor SubLabelColor = FLinearColor(FColor(0x8F, 0xA3, 0xBF));

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Style")
	FLinearColor SignalLostColor = FLinearColor(FColor(0xE5, 0x48, 0x4D));

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Style")
	FLinearColor PlateColor = FLinearColor(FColor(0x0A, 0x11, 0x1D, 0x99));

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Screen")
	FLinearColor GhostColor = FLinearColor(FColor(0x5F, 0xC8, 0xF0, 0x40));

	UPROPERTY(EditAnywhere, Category="Knight|HUD")
	FText ConfirmLabel;

	UPROPERTY(EditAnywhere, Category="Knight|HUD")
	FText ConfirmSubLabel;

	/** [AI] 음성 자막(UknightUISettings::RaidSuccessVoice와 같은 말). */
	UPROPERTY(EditAnywhere, Category="Knight|HUD")
	FText VoiceSubtitle;

	UPROPERTY(EditAnywhere, Category="Knight|HUD")
	FSlateFontInfo ConfirmFont;

	UPROPERTY(EditAnywhere, Category="Knight|HUD")
	FSlateFontInfo LabelFont;

	UPROPERTY(EditAnywhere, Category="Knight|HUD")
	FSlateFontInfo SubtitleFont;

	/** 판독 창 크기(슬레이트 단위)와 화면 높이 대비 가운데 위치. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout")
	FVector2D PanelSize = FVector2D(760.f, 230.f);

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout", meta=(ClampMin=0, ClampMax=1))
	float PanelCenterY = 0.4f;

private:
	float GetElapsed() const;
	void DrawText(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2D& Center, const FText& Label, const FSlateFontInfo& Font,
		const FLinearColor& Color, float GhostOffset, bool bCentered) const;

	FText TargetName;
	float ClearTime = 0.f;
	int32 BrokenParts = 0;
	float BannerTime = 3.5f;
	double StartTime = 0.0;
};
