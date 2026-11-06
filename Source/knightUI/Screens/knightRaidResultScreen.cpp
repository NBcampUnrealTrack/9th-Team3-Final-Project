#include "Screens/knightRaidResultScreen.h"

#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFlow/KnightGameFlowSubsystem.h"
#include "GameFlow/KnightStageCatalog.h"
#include "HUD/knightHUDScreenDraw.h"
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
	bReturnFocused = false;
	bReturnRequested = false;
	ElapsedTime = 0.f;
	ShownCountdown = -1;

	// 홀로그램 선 색. 시간 숫자도 같은 색으로 강조한다.
	const FLinearColor AccentColor = bSuccess ? SuccessColor : FailureColor;
	if (Img_ReportAccent)
	{
		Img_ReportAccent->SetColorAndOpacity(knightHUDDraw::WithOpacity(AccentColor, 0.45f));
	}

	// Report.
	if (Txt_StageName)
	{
		Txt_StageName->SetText(GetStageName(Result));
	}
	if (Txt_ClearTime)
	{
		Txt_ClearTime->SetText(FormatRaidTime(Result.ElapsedTime));
		ClearTimeSeconds = Result.ElapsedTime;
		Txt_ClearTime->SetColorAndOpacity(AccentColor);
	}
	if (Txt_ClearTimeLabel)
	{
		Txt_ClearTimeLabel->SetText(bSuccess ? INVTEXT("CLEAR TIME") : INVTEXT("ELAPSED"));
	}
	// 실패면 부위 파괴 대신 실패 사유.
	const FText Reason = bSuccess ? FText::GetEmpty() : GetFailReasonText(Result.FailReason);
	if (Row_FailReason)
	{
		Row_FailReason->SetVisibility(Reason.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (Txt_FailReason)
	{
		Txt_FailReason->SetText(Reason);
		Txt_FailReason->SetColorAndOpacity(FailureColor);
	}
	if (Row_BrokenParts)
	{
		Row_BrokenParts->SetVisibility(bSuccess ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
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

	// Motion: 판독 창이 올라가는 동안 기다렸다가 보고 패널을 켠다. 자리는 미리 잡아 둔다(Hidden).
	if (Img_Dim)
	{
		Img_Dim->SetRenderOpacity(0.f);
	}
	if (Report)
	{
		Report->SetVisibility(ESlateVisibility::Hidden);
	}
	GatherRevealTargets();
	TickReveal(-1.f);
	Btn_Return->SetIsEnabled(false);

	if (USoundBase* const Sound = bSuccess ? SuccessSound.Get() : FailureSound.Get())
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}

	StopTicker();
	Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickMotion));
}

void UknightRaidResultScreen::SetReturnStartTime(double WorldSeconds)
{
	StartWorldTime = WorldSeconds;
	ShownCountdown = -1;
	RefreshCountdown();
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
	// 패널이 다 켜지기 전에는 화면 자체에 포커스를 둔다. 버튼에 먼저 주면 포커스 테두리가 투명한 줄 위에 혼자 떠 보인다.
	return bReturnFocused ? Btn_Return.Get() : nullptr;
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
		USizeBox* const RewardHeight = WidgetTree->ConstructWidget<USizeBox>();
		RewardHeight->SetMaxDesiredHeight(110.f);
		UScrollBox* const RewardScroll = WidgetTree->ConstructWidget<UScrollBox>();
		RewardHeight->SetContent(RewardScroll);
		Box_Rewards->AddChild(RewardHeight);
		auto AddRewardRow = [this, RewardScroll, &RowCount](const FText& Text, UTexture2D* Icon)
		{
			UHorizontalBox* const Row = WidgetTree->ConstructWidget<UHorizontalBox>();
			if (Icon)
			{
				UImage* const IconImage = WidgetTree->ConstructWidget<UImage>();
				IconImage->SetBrushFromTexture(Icon);
				IconImage->SetDesiredSizeOverride(FVector2D(32.f, 32.f));
				UHorizontalBoxSlot* const IconSlot = Row->AddChildToHorizontalBox(IconImage);
				IconSlot->SetVerticalAlignment(VAlign_Center);
				IconSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
			}
			UCommonTextBlock* const Label = WidgetTree->ConstructWidget<UCommonTextBlock>();
			if (RewardTextStyle) Label->SetStyle(RewardTextStyle);
			Label->SetText(Text);
			Row->AddChildToHorizontalBox(Label)->SetVerticalAlignment(VAlign_Center);
			UPanelSlot* const RowSlot = RewardScroll->AddChild(Row);
			if (UScrollBoxSlot* const ScrollSlot = Cast<UScrollBoxSlot>(RowSlot); ScrollSlot && RowCount > 0)
			{
				ScrollSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
			}
			++RowCount;
		};
		for (const FknightItemStack& Reward : Result.Rewards)
		{
			if (!Reward.Item || Reward.Count <= 0)
			{
				continue;
			}
			AddRewardRow(FText::Format(LOCTEXT("RewardRow", "{0}  x{1}"), Reward.Item->DisplayName,
				FText::AsNumber(Reward.Count)), Reward.Item->Icon.LoadSynchronous());
		}
		if (Result.MoneyEarned > 0)
		{
			AddRewardRow(FText::Format(LOCTEXT("MoneyReward", "획득 돈  +{0}"),
				FText::AsNumber(Result.MoneyEarned)), nullptr);
		}
		if (Result.FirstBossSocketSlotsEarned > 0)
		{
			AddRewardRow(FText::Format(LOCTEXT("FirstBossSocketReward", "공용 소켓 칸  +{0}"),
				FText::AsNumber(Result.FirstBossSocketSlotsEarned)), nullptr);
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

	if (Img_Dim)
	{
		Img_Dim->SetRenderOpacity(DimInDuration > 0.f ? knightHUDDraw::EaseOut(ElapsedTime / DimInDuration) : 1.f);
	}

	const float ReportTime = GetReportTime();
	if (ReportTime >= 0.f && !bReportShown)
	{
		ShowReport();
	}
	TickReveal(ReportTime);
	// 버튼은 마지막 줄까지 다 올라온 뒤에 누를 수 있게 한다. 포커스 테두리는 줄 투명도와 상관없이 그려져 먼저 떠 보인다.
	if (bReportShown && !bReturnFocused && !bReturnRequested
		&& ReportTime >= RowStartTime + FMath::Max(0, RevealRows.Num() - 1) * RowInterval + RowInDuration)
	{
		bReturnFocused = true;
		Btn_Return->SetIsEnabled(true);
		Btn_Return->SetFocus();
	}

	RefreshCountdown();
	return true;
}

void UknightRaidResultScreen::ShowReport()
{
	bReportShown = true;
	if (Report)
	{
		Report->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void UknightRaidResultScreen::GatherRevealTargets()
{
	RevealRows.Reset();
	DecodeLabels.Reset();
	ClearTimeStart = RowStartTime;
	UPanelWidget* const Column = Cast<UPanelWidget>(GetWidgetFromName(TEXT("VerticalBox_Report")));
	if (!Column)
	{
		return;
	}
	// 접힌 줄(성공의 실패 사유 등)은 건너뛰어 보이는 줄끼리 간격을 맞춘다.
	TMap<const UWidget*, float> RowStart;
	for (UWidget* const Row : Column->GetAllChildren())
	{
		if (Row && Row->GetVisibility() != ESlateVisibility::Collapsed)
		{
			RowStart.Add(Row, RowStartTime + RevealRows.Num() * RowInterval);
			RevealRows.Add(Row);
		}
	}
	const auto FindRowStart = [&](const UWidget* Widget)
	{
		for (const UWidget* Each = Widget; Each; Each = Each->GetParent())
		{
			if (const float* const Start = RowStart.Find(Each))
			{
				return *Start;
			}
		}
		return RowStartTime;
	};
	for (const TCHAR* const Name : { TEXT("Txt_ReportCaption"), TEXT("Txt_ClearTimeLabel"), TEXT("Txt_BestTimeLabel"), TEXT("Txt_BrokenPartsLabel"),
		TEXT("Txt_FailReasonLabel"), TEXT("Txt_RewardsLabel") })
	{
		if (UTextBlock* const Label = Cast<UTextBlock>(GetWidgetFromName(Name)))
		{
			DecodeLabels.Add({ Label, Label->GetText().ToString(), FindRowStart(Label) });
		}
	}
	if (Txt_ClearTime)
	{
		ClearTimeStart = FindRowStart(Txt_ClearTime);
	}
}

void UknightRaidResultScreen::TickReveal(float ReportTime)
{
	// 바탕판: 외곽선이 반쯤 그려졌을 때부터 차오른다.
	if (UBorder* const Frame = Cast<UBorder>(GetWidgetFromName(TEXT("Border_Frame"))))
	{
		const float Plate = knightHUDDraw::EaseOut((ReportTime - TraceDuration * 0.5f) / PlateInDuration);
		Frame->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, ReportTime < 0.f ? 0.f : Plate));
	}
	// 줄: 위에서부터 차례로 살짝 올라오며 나타난다.
	for (int32 Index = 0; Index < RevealRows.Num(); ++Index)
	{
		if (UWidget* const Row = RevealRows[Index].Get())
		{
			const float Alpha = ReportTime < 0.f ? 0.f : knightHUDDraw::EaseOut((ReportTime - RowStartTime - Index * RowInterval) / RowInDuration);
			Row->SetRenderOpacity(Alpha);
			Row->SetRenderTranslation(FVector2D(0.f, (1.f - Alpha) * 8.f));
			// 버튼 틀은 위젯 투명도를 받지 않고 그려져서, 줄이 반쯤 나타날 때까지 숨겨 둔다.
			if (Btn_Return && Btn_Return->IsChildOf(Row))
			{
				const ESlateVisibility ButtonVisibility = Alpha < 0.4f ? ESlateVisibility::Hidden : ESlateVisibility::Visible;
				if (Btn_Return->GetVisibility() != ButtonVisibility)
				{
					Btn_Return->SetVisibility(ButtonVisibility);
				}
			}
		}
	}
	// 라벨: 아직 맞춰지지 않은 글자는 무작위 글자로 바뀌며 돈다. 다 맞춰진 라벨은 다시 건드리지 않는다.
	static const TCHAR Glyphs[] = TEXT("ABCDEFGHJKLMNPQRSTUVWXYZ0123456789");
	for (FDecodeLabel& Entry : DecodeLabels)
	{
		UTextBlock* const Label = Entry.Label.Get();
		if (!Label || Entry.Text.IsEmpty())
		{
			continue;
		}
		const int32 Done = ReportTime < Entry.StartTime ? 0 : FMath::FloorToInt32((ReportTime - Entry.StartTime) / DecodeCharInterval);
		if (Done >= Entry.Text.Len())
		{
			Label->SetText(FText::FromString(Entry.Text));
			Entry.Text.Reset();
			continue;
		}
		FString Shown = Entry.Text;
		for (int32 Char = Done; Char < Shown.Len(); ++Char)
		{
			if (FChar::IsAlnum(Shown[Char]))
			{
				Shown[Char] = Glyphs[FMath::RandRange(0, UE_ARRAY_COUNT(Glyphs) - 2)];
			}
		}
		Label->SetText(FText::FromString(Shown));
	}
	// 소요 시간: 0부터 올라가 멈춘다.
	if (Txt_ClearTime && ReportTime >= 0.f && ReportTime <= ClearTimeStart + CountUpDuration + 0.1f)
	{
		const float Count = knightHUDDraw::EaseOut((ReportTime - ClearTimeStart) / CountUpDuration);
		Txt_ClearTime->SetText(FormatRaidTime(ClearTimeSeconds * Count));
	}
}

int32 UknightRaidResultScreen::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const float ReportTime = GetReportTime();
	if (!Report || ReportTime < 0.f)
	{
		return Layer;
	}

	// 보고 패널의 배치 자리. 바탕판 위에 외곽선과 모서리 브래킷을 그린다.
	// 틱 공간(GetCachedGeometry)은 창 모드에서 데스크톱 좌표라 창 위치만큼 어긋난다. 그리는 공간 기준으로 잡는다.
	const FGeometry& ReportGeometry = Report->GetPaintSpaceGeometry();
	const FVector2D TL = AllottedGeometry.AbsoluteToLocal(ReportGeometry.GetAbsolutePosition());
	const FVector2D FullSize = AllottedGeometry.AbsoluteToLocal(ReportGeometry.GetAbsolutePosition() + ReportGeometry.GetAbsoluteSize()) - TL;
	if (FullSize.X <= 1.f || FullSize.Y <= 1.f)
	{
		return Layer;
	}
	// 외곽선: 왼쪽 위 모서리에서 시계 방향으로 한 바퀴 그려진 뒤 옅게 남는다. 모서리 브래킷은 다 그려지면 나타난다.
	const FLinearColor AccentColor = bSuccess ? SuccessColor : FailureColor;
	const float Trace = knightHUDDraw::EaseOut(ReportTime / TraceDuration);
	const float LineOpacity = ReportTime < TraceDuration ? 0.9f : FMath::Lerp(0.9f, 0.3f, FMath::Clamp((ReportTime - TraceDuration) / 0.4f, 0.f, 1.f));
	const FVector2D BR = TL + FullSize;
	const FVector2D Corners[] = { TL, FVector2D(BR.X, TL.Y), BR, FVector2D(TL.X, BR.Y), TL };
	const float Perimeter = 2.f * (FullSize.X + FullSize.Y);
	float Remaining = Perimeter * Trace;
	TArray<FVector2f> Outline = { FVector2f(TL) };
	for (int32 Edge = 0; Edge < 4 && Remaining > 0.f; ++Edge)
	{
		const FVector2D From = Corners[Edge];
		const FVector2D To = Corners[Edge + 1];
		const float Length = (To - From).Size();
		const float Step = FMath::Min(Remaining, Length);
		Outline.Add(FVector2f(From + (To - From) * (Step / Length)));
		Remaining -= Step;
	}
	knightHUDDraw::Lines(OutDrawElements, Layer + 1, AllottedGeometry, Outline, knightHUDDraw::WithOpacity(AccentColor, LineOpacity), 1.f);

	const float BracketOpacity = knightHUDDraw::EaseOut((ReportTime - TraceDuration * 0.8f) / 0.2f);
	if (BracketOpacity > 0.f)
	{
		const FLinearColor Accent = knightHUDDraw::WithOpacity(AccentColor, BracketOpacity);
		const float Arm = 24.f;
		const FVector2D Mid = TL + FullSize * 0.5;
		for (const FVector2D& Corner : { TL, FVector2D(BR.X, TL.Y), BR, FVector2D(TL.X, BR.Y) })
		{
			const float SX = Corner.X < Mid.X ? 1.f : -1.f;
			const float SY = Corner.Y < Mid.Y ? 1.f : -1.f;
			knightHUDDraw::Lines(OutDrawElements, Layer + 2, AllottedGeometry,
				{ FVector2f(Corner.X + SX * Arm, Corner.Y), FVector2f(Corner.X, Corner.Y), FVector2f(Corner.X, Corner.Y + SY * Arm) }, Accent, 2.f);
		}
	}
	return Layer + 3;
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
		? FText::Format(LOCTEXT("ReturnCountdown", "RETURN IN {0}s"), FText::AsNumber(Remaining))
		: LOCTEXT("Returning", "RETURNING"));
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
