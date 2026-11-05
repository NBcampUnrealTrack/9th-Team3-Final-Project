#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Foundation/knightActivatableWidget.h"
#include "Raid/knightRaidTypes.h"
#include "knightRaidResultScreen.generated.h"

class UCommonTextBlock;
class UCommonTextStyle;
class UImage;
class UknightButtonBase;
class UPanelWidget;
class USoundBase;
class UTextBlock;
class UWidget;

/**
 * Raid result on UI.Layer.Menu, opened by AknightHUD when the raid phase turns Success or Failed.
 *
 * Shows the report: clear time, best time, broken parts (or the fail reason), rewards, and a countdown to the automatic
 * return. Btn_Return returns right away. The countdown mirrors the GameMode's return delay
 * (UknightRaidDefinition::SuccessReturnDelay / FailureReturnDelay); the GameMode still owns the travel.
 *
 * 제목은 따로 없다. AknightHUD의 판독 창(UknightHUDFinishBanner)이 결과 화면이 열릴 때 위로 올라가 제목이 되고, 보고 패널은 그 아래에서
 * 판독 창과 같은 화면으로 켜진다: ReportDelay 뒤 외곽선이 모서리에서 한 바퀴 그려지고, 바탕판이 차오른 다음 줄이 위에서부터 차례로
 * 올라온다. 영문 라벨은 무작위 글자에서 해독되듯 맞춰지고, 소요 시간은 0부터 올라간다. 바탕판은 WBP가, 외곽선·모서리 브래킷은
 * NativePaint가 Report 위에 그린다.
 *
 * Reads the result from UknightRaidSubsystem::GetLastResult without clearing it; the main menu clears it.
 *
 * Layout lives in the Widget Blueprint designer (WBP_KnightRaidResult). Required: Btn_Return (UknightButtonBase).
 * Optional: Img_Dim, Report, Img_ReportAccent, Txt_StageName, Txt_ClearTimeLabel, Txt_ClearTime, Txt_BestTime, Txt_NewRecord, Row_BrokenParts, Txt_BrokenParts,
 * Row_FailReason, Txt_FailReason, Box_Rewards, Txt_NoRewards, Txt_Countdown.
 */
UCLASS(Abstract, Blueprintable, meta=(DisableNativeTick))
class KNIGHTUI_API UknightRaidResultScreen : public UknightActivatableWidget
{
	GENERATED_BODY()

public:
	UknightRaidResultScreen(const FObjectInitializer& ObjectInitializer);

	/** Fills the screen from a result. Called on activation with the last recorded result; public for tests. */
	void ShowResult(const FknightRaidResult& Result);

	/** 귀환 타이머가 열리기 전부터 돌았을 때(실패 판독 창을 먼저 보여 준 경우) 카운트다운을 그 시각(월드 초)에 맞춘다. */
	void SetReturnStartTime(double WorldSeconds);

	/** "mm:ss.cc". */
	static FText FormatRaidTime(float Seconds);

	static FText GetFailReasonText(EknightRaidFailReason Reason);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	UPROPERTY(EditAnywhere, Category="Knight|Result")
	TObjectPtr<USoundBase> SuccessSound;

	UPROPERTY(EditAnywhere, Category="Knight|Result")
	TObjectPtr<USoundBase> FailureSound;

	/** 홀로그램 선과 시간 숫자 색. 판독 창과 같은 정보 색(하늘색), 실패는 빨강. */
	UPROPERTY(EditAnywhere, Category="Knight|Result")
	FLinearColor SuccessColor = FLinearColor(FColor(0x7F, 0xE3, 0xFF));

	UPROPERTY(EditAnywhere, Category="Knight|Result")
	FLinearColor FailureColor = FLinearColor(FColor(0xE5, 0x48, 0x4D));

	/** Text style of the reward rows created at runtime. */
	UPROPERTY(EditAnywhere, Category="Knight|Result")
	TSubclassOf<UCommonTextStyle> RewardTextStyle;

	UPROPERTY(EditAnywhere, Category="Knight|Result|Motion", meta=(ClampMin=0, Units="s"))
	float DimInDuration = 0.4f;

	/** 판독 창이 위로 올라가는 동안 기다렸다가 보고 패널을 켠다. */
	UPROPERTY(EditAnywhere, Category="Knight|Result|Motion", meta=(ClampMin=0, Units="s"))
	float ReportDelay = 0.5f;

	/** 외곽선이 한 바퀴 그려지는 시간. 그 뒤 옅게 남는다. */
	UPROPERTY(EditAnywhere, Category="Knight|Result|Motion", meta=(ClampMin=0.01, Units="s"))
	float TraceDuration = 0.45f;

	/** 바탕판이 차오르는 시간(외곽선이 반쯤 그려졌을 때 시작). */
	UPROPERTY(EditAnywhere, Category="Knight|Result|Motion", meta=(ClampMin=0.01, Units="s"))
	float PlateInDuration = 0.3f;

	/** 줄이 나타나기 시작하는 때(패널이 켜진 뒤), 줄 사이 간격, 한 줄이 올라오는 시간. */
	UPROPERTY(EditAnywhere, Category="Knight|Result|Motion", meta=(ClampMin=0, Units="s"))
	float RowStartTime = 0.35f;

	UPROPERTY(EditAnywhere, Category="Knight|Result|Motion", meta=(ClampMin=0, Units="s"))
	float RowInterval = 0.06f;

	UPROPERTY(EditAnywhere, Category="Knight|Result|Motion", meta=(ClampMin=0.01, Units="s"))
	float RowInDuration = 0.25f;

	/** 라벨이 한 글자씩 맞춰지는 간격. */
	UPROPERTY(EditAnywhere, Category="Knight|Result|Motion", meta=(ClampMin=0.005, Units="s"))
	float DecodeCharInterval = 0.03f;

	/** 소요 시간이 0부터 올라가는 시간(그 줄이 나타날 때 시작). */
	UPROPERTY(EditAnywhere, Category="Knight|Result|Motion", meta=(ClampMin=0.01, Units="s"))
	float CountUpDuration = 0.6f;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidget))
	TObjectPtr<UknightButtonBase> Btn_Return;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Img_Dim;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Report;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UImage> Img_ReportAccent;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_StageName;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_ClearTimeLabel;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_ClearTime;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_BestTime;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_NewRecord;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Row_BrokenParts;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_BrokenParts;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Row_FailReason;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_FailReason;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UPanelWidget> Box_Rewards;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_NoRewards;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Countdown;

private:
	void HandleReturnClicked();
	FText GetStageName(const FknightRaidResult& Result) const;
	void FillRewards(const FknightRaidResult& Result);
	bool TickMotion(float DeltaTime);
	void StopTicker();
	void ShowReport();
	/** 보고 패널이 켜진 뒤 지난 실제 초. 아직이면 음수. */
	float GetReportTime() const { return ElapsedTime - ReportDelay; }
	/** 패널 안 줄(VerticalBox_Report의 자식)과 해독할 라벨을 모은다. */
	void GatherRevealTargets();
	void TickReveal(float ReportTime);

	struct FDecodeLabel
	{
		TWeakObjectPtr<UTextBlock> Label;
		FString Text;
		float StartTime = 0.f;
	};
	TArray<TWeakObjectPtr<UWidget>> RevealRows;
	TArray<FDecodeLabel> DecodeLabels;
	float ClearTimeSeconds = 0.f;
	float ClearTimeStart = 0.f;
	void RefreshCountdown();

	bool bSuccess = false;
	bool bReportShown = false;
	bool bReturnFocused = false;
	bool bReturnRequested = false;
	float ElapsedTime = 0.f;
	/** World time of activation and the GameMode's return delay; the countdown follows world time like the GameMode timer. */
	double StartWorldTime = 0.0;
	float ReturnDelay = -1.f;
	int32 ShownCountdown = -1;
	FTSTicker::FDelegateHandle Ticker;
};
