#include "Screens/knightLoadingScreen.h"

#include "CommonTextBlock.h"
#include "Engine/World.h"
#include "Raid/knightRaidGameState.h"

#define LOCTEXT_NAMESPACE "knightUI"

UknightLoadingScreen::UknightLoadingScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Sample tips until the final copy arrives; the Widget Blueprint's defaults override these.
	Tips = {
		LOCTEXT("LoadingTip0", "부스트 게이지가 과열되면 회복될 때까지 기다려야 합니다. 게이지를 조금 남겨 두세요."),
		LOCTEXT("LoadingTip1", "게임 시작을 누르면 가장 최근에 플레이한 저장 슬롯으로 이어서 시작합니다."),
		LOCTEXT("LoadingTip2", "레이드 중 Esc를 누르면 일시정지 메뉴에서 설정을 바꿀 수 있습니다."),
		LOCTEXT("LoadingTip3", "레이드에는 제한 시간이 있습니다. 남은 시간이 1분이 되면 타이머가 주황색으로 바뀝니다."),
		LOCTEXT("LoadingTip4", "설정의 화면 보정 탭에서 밝기와 감마를 맞추면 어두운 지역이 잘 보입니다."),
	};
}

void UknightLoadingScreen::NativeConstruct()
{
	Super::NativeConstruct();
	if (Txt_Tip)
	{
		SelectedTip = Tips.Num() > 0 ? Tips[FMath::RandRange(0, Tips.Num() - 1)] : FText::GetEmpty();
		Txt_Tip->SetText(SelectedTip);
	}
	DotTime = 0.f;
	LitDot = -1;
	ShowDots(0);
}

void UknightLoadingScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// The first frame after a map load carries the whole load as its delta; wrapping keeps the dots in step anyway.
	DotTime = FMath::Fmod(DotTime + InDeltaTime, DotInterval * 3.f);
	ShowDots(FMath::FloorToInt(DotTime / DotInterval));
	if (Txt_Tip)
	{
		FText DisplayText = SelectedTip;
		if (const UWorld* const World = GetWorld(); World && World->IsGameWorld())
		{
			if (const AknightRaidGameState* const RaidState = World->GetGameState<AknightRaidGameState>();
				RaidState && RaidState->GetRaidPhase() == EknightRaidPhase::None && !RaidState->GetLoadingStatus().IsEmpty())
			{
				DisplayText = RaidState->GetLoadingStatus();
			}
		}
		if (!Txt_Tip->GetText().EqualTo(DisplayText))
		{
			Txt_Tip->SetText(DisplayText);
		}
	}
}

void UknightLoadingScreen::ShowDots(int32 LitIndex)
{
	if (LitIndex == LitDot)
	{
		return;
	}
	LitDot = LitIndex;
	UWidget* const Dots[] = { Img_Dot_0, Img_Dot_1, Img_Dot_2 };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Dots); ++Index)
	{
		if (UWidget* const Dot = Dots[Index])
		{
			Dot->SetRenderOpacity(Index == LitIndex ? 1.f : DimDotOpacity);
		}
	}
}

#undef LOCTEXT_NAMESPACE
