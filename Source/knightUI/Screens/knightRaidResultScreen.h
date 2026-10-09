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
class UWidget;

/**
 * Raid result on UI.Layer.Menu, opened by AknightHUD when the raid phase turns Success or Failed.
 *
 * Plays a banner first (success / failure), then shows the report: clear time, best time, broken parts, rewards,
 * and a countdown to the automatic return. Btn_Return returns right away. The countdown mirrors the GameMode's
 * return delay (UknightRaidDefinition::SuccessReturnDelay / FailureReturnDelay); the GameMode still owns the travel.
 *
 * Reads the result from UknightRaidSubsystem::GetLastResult without clearing it; the main menu clears it.
 *
 * Layout lives in the Widget Blueprint designer (WBP_KnightRaidResult). Required: Btn_Return (UknightButtonBase).
 * Optional: Banner, Report, Img_Accent, Txt_BannerCaption, Txt_BannerTitle, Txt_BannerReason, Txt_StageName,
 * Txt_ClearTime, Txt_BestTime, Txt_NewRecord, Txt_BrokenParts, Box_Rewards, Txt_NoRewards, Txt_Countdown.
 */
UCLASS(Abstract, Blueprintable, meta=(DisableNativeTick))
class KNIGHTUI_API UknightRaidResultScreen : public UknightActivatableWidget
{
	GENERATED_BODY()

public:
	UknightRaidResultScreen(const FObjectInitializer& ObjectInitializer);

	/** Fills the screen from a result. Called on activation with the last recorded result; public for tests. */
	void ShowResult(const FknightRaidResult& Result);

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

	UPROPERTY(EditAnywhere, Category="Knight|Result")
	TObjectPtr<USoundBase> SuccessSound;

	UPROPERTY(EditAnywhere, Category="Knight|Result")
	TObjectPtr<USoundBase> FailureSound;

	/** Banner accent and title color. */
	UPROPERTY(EditAnywhere, Category="Knight|Result")
	FLinearColor SuccessColor = FLinearColor(FColor(0xFF, 0x7A, 0x1A));

	UPROPERTY(EditAnywhere, Category="Knight|Result")
	FLinearColor FailureColor = FLinearColor(FColor(0xE5, 0x48, 0x4D));

	/** Text style of the reward rows created at runtime. */
	UPROPERTY(EditAnywhere, Category="Knight|Result")
	TSubclassOf<UCommonTextStyle> RewardTextStyle;

	UPROPERTY(EditAnywhere, Category="Knight|Result|Motion", meta=(ClampMin=0, Units="s"))
	float BannerInDuration = 0.25f;

	/** How long the banner stays alone before the report replaces it. */
	UPROPERTY(EditAnywhere, Category="Knight|Result|Motion", meta=(ClampMin=0, Units="s"))
	float BannerHoldDuration = 1.6f;

	UPROPERTY(EditAnywhere, Category="Knight|Result|Motion", meta=(ClampMin=0, Units="s"))
	float ReportInDuration = 0.2f;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidget))
	TObjectPtr<UknightButtonBase> Btn_Return;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Img_Dim;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Banner;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Report;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UImage> Img_Accent;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_BannerCaption;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_BannerTitle;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_BannerReason;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_StageName;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_ClearTime;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_BestTime;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_NewRecord;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_BrokenParts;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UPanelWidget> Box_Rewards;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_NoRewards;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Result", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_Countdown;

private:
	void HandleReturnClicked();
	FText GetStageName(const FknightRaidResult& Result) const;
	void FillRewards(const FknightRaidResult& Result);
	bool TickMotion(float DeltaTime);
	void StopTicker();
	void ShowReport();
	void RefreshCountdown();

	bool bSuccess = false;
	bool bReportShown = false;
	bool bReturnRequested = false;
	float ElapsedTime = 0.f;
	/** World time of activation and the GameMode's return delay; the countdown follows world time like the GameMode timer. */
	double StartWorldTime = 0.0;
	float ReturnDelay = -1.f;
	int32 ShownCountdown = -1;
	FTSTicker::FDelegateHandle Ticker;
};
