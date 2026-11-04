#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FieldNotificationId.h"
#include "Fonts/SlateFontInfo.h"
#include "HUD/knightHUDScreenDraw.h"
#include "Monster/knightMonsterReadout.h"
#include "knightHUDMonsterReadout.generated.h"

class AknightMonsterCharacterBase;
class UknightHUDLockOnMarker;
class UknightHUDMessageLog;
class UknightVM_BossStatus;
class USoundBase;

/** 판독 문구가 뜻하는 것. 색과 앞 기호가 갈린다. */
UENUM()
enum class EknightReadoutKind : uint8
{
	/** 반격 기회(청록 ◇). */
	Opening,
	/** 위험 상승(주황 ▲). */
	Danger,
	/** 체력 저하(노랑 ▽). */
	Vitals,
};

USTRUCT()
struct FknightReadoutEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Readout")
	EknightReadoutKind Kind = EknightReadoutKind::Opening;

	UPROPERTY(EditAnywhere, Category="Readout")
	FText Label;

	UPROPERTY(EditAnywhere, Category="Readout")
	FText Detail;

	/** 떠 있는 시간. */
	UPROPERTY(EditAnywhere, Category="Readout", meta=(ClampMin=0.3, Units="s"))
	float Duration = 2.2f;

	/** 다른 판독이 떠 있을 때 이 값이 같거나 높으면 바로 바꿔 띄운다. 낮으면 앞의 것이 끝난 뒤에 띄운다. */
	UPROPERTY(EditAnywhere, Category="Readout")
	int32 Priority = 0;
};

/**
 * 몬스터 상태 센서 판독(#390). 보스 체력·상태 바 대신, 상태가 바뀌는 순간 락온 이름판 바로 아래에 짧은 판독 태그를 띄웠다가 지운다.
 *   청록 ◇ 반격 기회: 탈진, 다운·쓰러짐(약점 노출), 패링·부위 파괴 경직
 *   주황 ▲ 위험 상승: 분노, 2페이즈
 *   노랑 ▽ 체력 저하: 체력이 VitalsLowRatio, VitalsCriticalRatio 아래로 처음 내려갈 때
 * 판독이 뜰 때 같은 HUD의 메시지 로그에 [AI] 대사를 한 줄 넣는다(UknightHUDMessageLog::AddMonsterReadoutLine).
 * 다운·쓰러짐 동안에는 약점 부위(배율 WeakPointMultiplier 이상, 드러나는 조건이 있으면 드러났을 때)에 마름모 표시가 맥박처럼 깜빡인다.
 *
 * 대상은 BossStatus 뷰모델이 묶은 몬스터다. 락온 중이면 이름판 아래에, 아니면 몬스터 몸 옆에 뜬다. 문구는 몬스터 정의의
 * ReadoutTexts로 몬스터마다 바꾼다. 경고 마커(UknightHUDAttackWarning)와 같은 조종석 화면 질감으로 NativePaint에서 직접 그린다.
 */
UCLASS(Blueprintable)
class KNIGHTUI_API UknightHUDMonsterReadout : public UUserWidget
{
	GENERATED_BODY()

public:
	UknightHUDMonsterReadout(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	UPROPERTY(EditAnywhere, Category="Knight|HUD", meta=(ForceInlineRow))
	TMap<EknightMonsterReadout, FknightReadoutEntry> Entries;

	/** 체력이 이 비율 아래로 처음 내려갈 때 VitalsLow, 둘째 값 아래로 내려갈 때 VitalsCritical. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD", meta=(ClampMin=0, ClampMax=1))
	float VitalsLowRatio = 0.5f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD", meta=(ClampMin=0, ClampMax=1))
	float VitalsCriticalRatio = 0.25f;

	/** 다운 중 마름모를 띄울 부위 배율. 피격 큐가 약점 번개로 바뀌는 기준(WeakpointMultiplierThreshold)과 맞춘다. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD", meta=(ClampMin=1))
	float WeakPointMultiplier = 1.45f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Sound")
	TObjectPtr<USoundBase> OpeningSound;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Sound")
	TObjectPtr<USoundBase> DangerSound;

	/** 비우면 DangerSound. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Sound")
	TObjectPtr<USoundBase> VitalsSound;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Style")
	FLinearColor OpeningColor = FLinearColor(FColor(0x5F, 0xD8, 0xF0));

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Style")
	FLinearColor DangerColor = FLinearColor(FColor(0xFF, 0x8A, 0x3D));

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Style")
	FLinearColor VitalsColor = FLinearColor(FColor(0xE8, 0xD2, 0x7A));

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Style")
	FLinearColor PlateColor = FLinearColor(FColor(0x0A, 0x11, 0x1D, 0x66));

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Style")
	FSlateFontInfo LabelFont;

	/** 풀이 줄. 한글이 들어가므로 한글 글꼴을 쓴다. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Style")
	FSlateFontInfo DetailFont;

	/** 풀이 줄 앞에 붙는 말. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Style")
	FText DetailPrefix;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Screen", meta=(ClampMin=0, ClampMax=1))
	float BaseOpacity = 0.72f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Screen")
	FLinearColor GhostColor = FLinearColor(FColor(0x5F, 0xC8, 0xF0, 0x40));

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Screen", meta=(ClampMin=0))
	float GhostOffset = 1.5f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Screen", meta=(ClampMin=1))
	float ScanlineSpacing = 3.f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Screen", meta=(ClampMin=0, ClampMax=1))
	float ScanlineOpacity = 0.22f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Screen", meta=(ClampMin=0.1, Units="s"))
	float SweepPeriod = 0.9f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Screen", meta=(ClampMin=0, Units="s"))
	float BootFlickerTime = 0.18f;

	/** 끝날 때 옅어지는 시간. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Motion", meta=(ClampMin=0, Units="s"))
	float FadeOutTime = 0.4f;

	/** 이름판 아래 모서리에서 태그까지 거리. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout")
	FVector2D PlateGap = FVector2D(0.f, 10.f);

	/** 락온하지 않았을 때 몬스터 몸(FallbackLockOnSocket) 화면 위치에서 태그까지 거리. 화면 오른쪽이면 X를 뒤집는다. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout")
	FVector2D BodyOffset = FVector2D(90.f, -70.f);

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout", meta=(ClampMin=0, ClampMax=1))
	float FlipScreenFraction = 0.62f;

	/** 화면 가장자리에서 이만큼 안쪽에만 놓는다. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout")
	FVector2D SafeMargin = FVector2D(48.f, 48.f);

	/** 몬스터가 카메라 뒤일 때 태그를 놓는 자리(화면 가로 가운데, 위에서 이만큼). */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout")
	float BehindTop = 150.f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout")
	FVector2D TagPadding = FVector2D(8.f, 3.f);

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout", meta=(ClampMin=4))
	float WeakPointSize = 26.f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Motion", meta=(ClampMin=0.1, Units="s"))
	float WeakPointPulsePeriod = 0.9f;

private:
	struct FShown
	{
		EknightMonsterReadout Type = EknightMonsterReadout::Exhausted;
		FText Label;
		FText Detail;
		EknightReadoutKind Kind = EknightReadoutKind::Opening;
		float Duration = 0.f;
		float Elapsed = 0.f;
		int32 Priority = 0;
	};

	void HandleReadout(EknightMonsterReadout Type);
	void HandleViewModelChanged(UObject* InViewModel, UE::FieldNotification::FFieldId FieldId);
	bool MakeShown(EknightMonsterReadout Type, FShown& OutShown) const;
	void Begin(const FShown& Shown);
	FLinearColor GetKindColor(EknightReadoutKind Kind) const;
	knightHUDDraw::FScreenLook GetScreenLook() const;
	AknightMonsterCharacterBase* GetMonster() const;
	UknightHUDLockOnMarker* FindLockOnMarker() const;
	UknightHUDMessageLog* FindMessageLog() const;
	template <typename T> T* FindInLayout() const;

	/** 태그 왼쪽 위(뒤집히면 오른쪽 위) 자리. false면 그리지 않는다. */
	bool GetTagAnchor(const FVector2D& Size, FVector2D& OutAnchor, bool& bOutFlipped) const;
	void PaintTag(const FGeometry& Geometry, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Size, float Tint) const;
	void PaintWeakPoints(const FGeometry& Geometry, FSlateWindowElementList& Out, int32 Layer, float Tint) const;

	TWeakObjectPtr<UknightVM_BossStatus> ViewModel;
	TWeakObjectPtr<AknightMonsterCharacterBase> TrackedMonster;
	mutable TWeakObjectPtr<UknightHUDLockOnMarker> LockOnMarker;
	mutable TWeakObjectPtr<UknightHUDMessageLog> MessageLog;
	FDelegateHandle ReadoutHandle;

	TOptional<FShown> Current;
	TOptional<FShown> Pending;
	/** 마지막으로 본 체력 비율. 음수면 아직 못 봤다(묶은 직후에는 기준을 넘은 것으로 치지 않는다). */
	float LastHealth = -1.f;
	float Time = 0.f;
};
