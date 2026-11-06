#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "HUD/knightHUDScreenDraw.h"
#include "knightHUDFinishBanner.generated.h"

/**
 * 레이드 결과 판독 창. 조종석 화면에 판독이 뜨는 느낌으로, 위젯 트리 없이 NativePaint로 그린다.
 *
 * 1. 가운데 가로선이 펼쳐지고 모서리 브래킷이 위아래로 열린다.
 * 2. 대상 이름이 한 글자씩 찍히고 생체 신호 파형이 뛴다. 성공이면 파형이 평평해지며 SIGNAL LOST, 실패면 실패 사유 코드.
 * 3. 큰 글씨 "토벌 확인"(실패는 "임무 실패")이 지직거리며 찍히고, 아래에 소요 시간.
 * 4. BannerTime 뒤에는 옅어진 채 남는다. BeginRise를 부르면 다시 선명해지며 줄어든 채 화면 위쪽으로 올라가 결과 화면의 제목이 된다.
 *
 * [AI] 음성 자막은 여기서 그리지 않는다(HUD 아래 대화창에 나온다).
 * AknightHUD가 만들어 결과 화면보다 위에 올리고, 결과 화면이 닫히거나 HUD가 끝날 때 지운다. 시간은 실제 시간(슬로모션·일시정지와 상관없음)이다.
 */
UCLASS()
class KNIGHTUI_API UknightHUDFinishBanner : public UUserWidget
{
	GENERATED_BODY()

public:
	UknightHUDFinishBanner(const FObjectInitializer& ObjectInitializer);

	/** 배너 내용. 만든 뒤 화면에 올리기 전에 부른다. bFailure면 빨간 "임무 실패" 판독이고 FailStatus가 오른쪽 위 상태 코드다. */
	void Setup(const FText& InTargetName, float InClearTime, float InBannerTime, bool bInFailure = false, const FText& InFailStatus = FText());

	/** 결과 화면이 열릴 때: 줄어들며 화면 위쪽(RisenTop)으로 올라간다. */
	void BeginRise();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
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
	FText FailLabel;

	UPROPERTY(EditAnywhere, Category="Knight|HUD")
	FSlateFontInfo ConfirmFont;

	UPROPERTY(EditAnywhere, Category="Knight|HUD")
	FSlateFontInfo LabelFont;

	/** 아래 소요 시간 숫자. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD")
	FSlateFontInfo TimeFont;

	/** 판독 창 크기(슬레이트 단위)와 화면 높이 대비 가운데 위치. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout")
	FVector2D PanelSize = FVector2D(760.f, 200.f);

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout", meta=(ClampMin=0, ClampMax=1))
	float PanelCenterY = 0.4f;

	/** 올라간 뒤 창 위 끝의 화면 위에서의 거리(슬레이트 단위)와 크기 배율. 결과 화면 보고 패널이 그 아래(WBP의 Report 위 여백)에서 시작한다. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout")
	float RisenTop = 64.f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout", meta=(ClampMin=0.1, ClampMax=1))
	float RisenScale = 0.8f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout", meta=(ClampMin=0.01, Units="s"))
	float RiseTime = 0.6f;

private:
	float GetElapsed() const;
	void DrawText(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2D& Center, const FText& Label, const FSlateFontInfo& Font,
		const FLinearColor& Color, float GhostOffset, bool bCentered) const;

	/** 0 = 가운데, 1 = 다 올라감. */
	float GetRiseAlpha() const;

	FText TargetName;
	FText FailStatus;
	float ClearTime = 0.f;
	float BannerTime = 3.5f;
	bool bFailure = false;
	double StartTime = 0.0;
	/** BeginRise를 부른 실제 시각. 0이면 아직 가운데. */
	double RiseStartTime = 0.0;
};
