#include "Screens/knightRaidResultScreen.h"

#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFlow/KnightGameFlowSubsystem.h"
#include "GameFlow/KnightStageCatalog.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Monster/knightMonsterDefinition.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Raid/knightRaidDefinition.h"
#include "Raid/knightRaidGameMode.h"
#include "Raid/knightRaidSubsystem.h"
#include "Widgets/knightButtonBase.h"
#include "knightItemDefinition.h"
#include "knightUI.h"

#define LOCTEXT_NAMESPACE "knightUI"

UknightRaidResultScreen::UknightRaidResultScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InputMode = EknightWidgetInputMode::Menu;
}

FText UknightRaidResultScreen::FormatRaidTime(float Seconds)
{
	const int32 Centiseconds = FMath::Max(0, FMath::FloorToInt(Seconds * 100.f));
	return FText::FromString(FString::Printf(TEXT("%02d:%02d.%02d"), Centiseconds / 6000, (Centiseconds / 100) % 60, Centiseconds % 100));
}

FText UknightRaidResultScreen::GetFailReasonText(EknightRaidFailReason Reason)
{
	switch (Reason)
	{
	case EknightRaidFailReason::KnightDestroyed: return LOCTEXT("FailKnightDestroyed", "기체 대파");
	case EknightRaidFailReason::TimeUp: return LOCTEXT("FailTimeUp", "제한 시간 초과");
	case EknightRaidFailReason::Abandoned: return LOCTEXT("FailAbandoned", "임무 포기");
	default: return FText::GetEmpty();
	}
}

void UknightRaidResultScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	Btn_Return->OnClicked().AddUObject(this, &ThisClass::HandleReturnClicked);
}

void UknightRaidResultScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	FknightRaidResult Result;
	const UGameInstance* const GameInstance = GetGameInstance();
	const UknightRaidSubsystem* const RaidSubsystem = GameInstance ? GameInstance->GetSubsystem<UknightRaidSubsystem>() : nullptr;
	if (!RaidSubsystem || !RaidSubsystem->GetLastResult(Result))
	{
		UE_LOG(LogknightUI, Warning, TEXT("%s: no raid result recorded; showing an empty report."), *GetName());
	}
	ShowResult(Result);
}

void UknightRaidResultScreen::ShowResult(const FknightRaidResult& Result)
{
	bSuccess = Result.bSuccess;
	bReportShown = false;
	bReturnRequested = false;
	ElapsedTime = 0.f;
	ShownCountdown = -1;

	// Banner.
	const FLinearColor AccentColor = bSuccess ? SuccessColor : FailureColor;
	if (Img_Accent)
	{
		Img_Accent->SetColorAndOpacity(AccentColor);
	}
	if (Txt_BannerCaption)
	{
		Txt_BannerCaption->SetText(bSuccess ? INVTEXT("MISSION COMPLETE") : INVTEXT("MISSION FAILED"));
	}
	if (Txt_BannerTitle)
	{
		Txt_BannerTitle->SetText(bSuccess ? LOCTEXT("ResultSuccess", "토벌 완료") : LOCTEXT("ResultFailed", "임무 실패"));
		Txt_BannerTitle->SetColorAndOpacity(AccentColor);
	}
	if (Txt_BannerReason)
	{
		const FText Reason = bSuccess ? FText::GetEmpty() : GetFailReasonText(Result.FailReason);
		Txt_BannerReason->SetText(Reason);
		Txt_BannerReason->SetVisibility(Reason.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	// Report.
	if (Txt_StageName)
	{
		Txt_StageName->SetText(GetStageName(Result));
	}
	if (Txt_ClearTime)
	{
		Txt_ClearTime->SetText(FormatRaidTime(Result.ElapsedTime));
	}

	// Progression has already applied this result (it listens to the raid subsystem), so a best time equal to
	// this run's time means this run set it.
	const UGameInstance* const GameInstance = GetGameInstance();
	const UknightProgressionSubsystem* const Progression = GameInstance ? GameInstance->GetSubsystem<UknightProgressionSubsystem>() : nullptr;
	const float BestTime = Progression && !Result.StageId.IsNone() ? Progression->GetBestClearTime(Result.StageId) : 0.f;
	if (Txt_BestTime)
	{
		Txt_BestTime->SetText(BestTime > 0.f ? FormatRaidTime(BestTime) : INVTEXT("--:--.--"));
	}
	if (Txt_NewRecord)
	{
		const bool bNewRecord = bSuccess && BestTime > 0.f && FMath::IsNearlyEqual(BestTime, Result.ElapsedTime, 0.001f);
		Txt_NewRecord->SetVisibility(bNewRecord ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Txt_BrokenParts)
	{
		Txt_BrokenParts->SetText(FText::Format(LOCTEXT("BrokenPartsCount", "{0}곳"), FText::AsNumber(Result.BrokenParts.Num())));
	}
	FillRewards(Result);

	// The GameMode returns to base after this delay (none when abandoned, which travels at once).
	ReturnDelay = -1.f;
	const UWorld* const World = GetWorld();
	if (const AknightRaidGameMode* const RaidMode = World ? World->GetAuthGameMode<AknightRaidGameMode>() : nullptr)
	{
		ReturnDelay = bSuccess ? 20.f : 5.f;
		if (const UknightRaidDefinition* const Definition = RaidMode->GetRaidDefinition())
		{
			ReturnDelay = bSuccess ? Definition->SuccessReturnDelay : Definition->FailureReturnDelay;
		}
	}
	StartWorldTime = World ? World->GetTimeSeconds() : 0.0;
	RefreshCountdown();

	// Motion: banner first, report later.
	if (Img_Dim)
	{
		Img_Dim->SetRenderOpacity(0.f);
	}
	if (Banner)
	{
		Banner->SetVisibility(ESlateVisibility::HitTestInvisible);
		Banner->SetRenderOpacity(0.f);
	}
	if (Report)
	{
		Report->SetVisibility(ESlateVisibility::Collapsed);
	}
	Btn_Return->SetIsEnabled(false);

	if (USoundBase* const Sound = bSuccess ? SuccessSound.Get() : FailureSound.Get())
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}

	StopTicker();
	Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickMotion));
}

void UknightRaidResultScreen::NativeOnDeactivated()
{
	StopTicker();
	Super::NativeOnDeactivated();
}

void UknightRaidResultScreen::NativeDestruct()
{
	StopTicker();
	Super::NativeDestruct();
}

UWidget* UknightRaidResultScreen::NativeGetDesiredFocusTarget() const
{
	return Btn_Return;
}

FReply UknightRaidResultScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// The raid is over: Esc must not reach the controller, which would try the pause menu or settings.
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UknightRaidResultScreen::HandleReturnClicked()
{
	if (bReturnRequested)
	{
		return;
	}
	bReturnRequested = true;
	Btn_Return->SetIsEnabled(false);

	UWorld* const World = GetWorld();
	if (AknightRaidGameMode* const RaidMode = World ? World->GetAuthGameMode<AknightRaidGameMode>() : nullptr)
	{
		RaidMode->RequestReturnToLobby();
	}
	else
	{
		// Shown outside a raid (console test): nothing to travel from.
		DeactivateWidget();
	}
}

FText UknightRaidResultScreen::GetStageName(const FknightRaidResult& Result) const
{
	const UGameInstance* const GameInstance = GetGameInstance();
	const UKnightGameFlowSubsystem* const Flow = GameInstance ? GameInstance->GetSubsystem<UKnightGameFlowSubsystem>() : nullptr;
	FKnightStageDefinition Stage;
	if (Flow && !Result.StageId.IsNone() && Flow->FindStageDefinition(Result.StageId, Stage) && !Stage.DisplayName.IsEmpty())
	{
		return Stage.DisplayName;
	}

	// Played directly in the editor: no stage was launched, so name the target monster instead.
	const UWorld* const World = GetWorld();
	const AknightRaidGameMode* const RaidMode = World ? World->GetAuthGameMode<AknightRaidGameMode>() : nullptr;
	const UknightRaidDefinition* const Definition = RaidMode ? RaidMode->GetRaidDefinition() : nullptr;
	if (Definition && Definition->TargetMonster && !Definition->TargetMonster->DisplayName.IsEmpty())
	{
		return FText::Format(LOCTEXT("StageFallback", "{0} 토벌"), Definition->TargetMonster->DisplayName);
	}
	return World ? FText::FromString(UGameplayStatics::GetCurrentLevelName(World)) : FText::GetEmpty();
}

void UknightRaidResultScreen::FillRewards(const FknightRaidResult& Result)
{
	int32 RowCount = 0;
	if (Box_Rewards)
	{
		Box_Rewards->ClearChildren();
		for (const FknightItemStack& Reward : Result.Rewards)
		{
			if (!Reward.Item || Reward.Count <= 0)
			{
				continue;
			}
			UHorizontalBox* const Row = WidgetTree->ConstructWidget<UHorizontalBox>();
			if (UTexture2D* const Icon = Reward.Item->Icon.LoadSynchronous())
			{
				UImage* const IconImage = WidgetTree->ConstructWidget<UImage>();
				IconImage->SetBrushFromTexture(Icon);
				IconImage->SetDesiredSizeOverride(FVector2D(32.f, 32.f));
				UHorizontalBoxSlot* const IconSlot = Row->AddChildToHorizontalBox(IconImage);
				IconSlot->SetVerticalAlignment(VAlign_Center);
				IconSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
			}
			UCommonTextBlock* const Label = WidgetTree->ConstructWidget<UCommonTextBlock>();
			if (RewardTextStyle)
			{
				Label->SetStyle(RewardTextStyle);
			}
			Label->SetText(FText::Format(LOCTEXT("RewardRow", "{0}  x{1}"), Reward.Item->DisplayName, FText::AsNumber(Reward.Count)));
			Row->AddChildToHorizontalBox(Label)->SetVerticalAlignment(VAlign_Center);

			UPanelSlot* const RowSlot = Box_Rewards->AddChild(Row);
			if (UVerticalBoxSlot* const VerticalSlot = Cast<UVerticalBoxSlot>(RowSlot); VerticalSlot && RowCount > 0)
			{
				VerticalSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
			}
			++RowCount;
		}
	}
	if (Txt_NoRewards)
	{
		Txt_NoRewards->SetVisibility(RowCount == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

bool UknightRaidResultScreen::TickMotion(float DeltaTime)
{
	ElapsedTime += DeltaTime;

	const auto EaseOut = [](float Alpha) { return 1.f - FMath::Square(1.f - FMath::Clamp(Alpha, 0.f, 1.f)); };
	const float BannerAlpha = BannerInDuration > 0.f ? EaseOut(ElapsedTime / BannerInDuration) : 1.f;
	if (Img_Dim)
	{
		Img_Dim->SetRenderOpacity(BannerAlpha);
	}

	if (!bReportShown)
	{
		if (Banner)
		{
			// Opens vertically like a shutter.
			Banner->SetRenderOpacity(BannerAlpha);
			Banner->SetRenderScale(FVector2D(1.f, FMath::Lerp(0.4f, 1.f, BannerAlpha)));
		}
		if (ElapsedTime >= BannerInDuration + BannerHoldDuration)
		{
			ShowReport();
		}
	}
	else if (Report)
	{
		const float ReportTime = ElapsedTime - (BannerInDuration + BannerHoldDuration);
		const float ReportAlpha = ReportInDuration > 0.f ? EaseOut(ReportTime / ReportInDuration) : 1.f;
		Report->SetRenderOpacity(ReportAlpha);
		Report->SetRenderTranslation(FVector2D(0.f, (1.f - ReportAlpha) * 16.f));
	}

	RefreshCountdown();
	return true;
}

void UknightRaidResultScreen::ShowReport()
{
	bReportShown = true;
	if (Banner)
	{
		Banner->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Report)
	{
		Report->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Report->SetRenderOpacity(0.f);
	}
	if (!bReturnRequested)
	{
		Btn_Return->SetIsEnabled(true);
		Btn_Return->SetFocus();
	}
}

void UknightRaidResultScreen::RefreshCountdown()
{
	if (!Txt_Countdown)
	{
		return;
	}
	if (ReturnDelay < 0.f)
	{
		Txt_Countdown->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	const UWorld* const World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : StartWorldTime;
	const int32 Remaining = FMath::Max(0, FMath::CeilToInt(ReturnDelay - static_cast<float>(Now - StartWorldTime)));
	if (Remaining == ShownCountdown)
	{
		return;
	}
	ShownCountdown = Remaining;
	Txt_Countdown->SetVisibility(ESlateVisibility::HitTestInvisible);
	Txt_Countdown->SetText(Remaining > 0
		? FText::Format(LOCTEXT("ReturnCountdown", "{0}초 후 기지로 귀환"), FText::AsNumber(Remaining))
		: LOCTEXT("Returning", "기지로 귀환 중"));
}

void UknightRaidResultScreen::StopTicker()
{
	if (Ticker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
		Ticker.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
