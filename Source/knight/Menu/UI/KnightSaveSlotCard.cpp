#include "KnightSaveSlotCard.h"

#include "Components/TextBlock.h"

#define LOCTEXT_NAMESPACE "KnightSaveSlotCard"

namespace
{
	FText FormatPlayTime(float Seconds)
	{
		const int64 Whole = FMath::Max<int64>(0, FMath::FloorToInt64(Seconds));
		return FText::FromString(FString::Printf(TEXT("%02lld:%02lld:%02lld"), Whole / 3600, (Whole / 60) % 60, Whole % 60));
	}

	void SetTextIf(UTextBlock* Text, const FText& Value)
	{
		if (Text)
		{
			Text->SetText(Value);
		}
	}

	void ShowIf(UWidget* Widget, bool bShow)
	{
		if (Widget)
		{
			Widget->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}

void UKnightSaveSlotCard::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	Refresh();
	RefreshFocusVisual();
}

void UKnightSaveSlotCard::NativePreConstruct()
{
	Super::NativePreConstruct();
	// Designer preview shows the empty state.
	Refresh();
	RefreshFocusVisual();
}

void UKnightSaveSlotCard::SetSlotView(const FKnightSaveSlotView& InView)
{
	View = InView;
	Refresh();
}

void UKnightSaveSlotCard::Refresh()
{
	SetTextIf(Txt_SlotName, FText::Format(LOCTEXT("SlotName", "저장 {0}"), FText::AsNumber(View.SlotNumber)));
	SetTextIf(Txt_SlotCaption, FText::FromString(FString::Printf(TEXT("SLOT %02d"), View.SlotNumber)));

	const bool bReady = View.State == EKnightSaveSlotState::Ready;
	ShowIf(Panel_Filled, bReady);
	ShowIf(Panel_Empty, !bReady);
	ShowIf(Panel_Active, bReady && View.bActive);

	if (bReady)
	{
		SetTextIf(Txt_PlayTime, FormatPlayTime(View.PlayTimeSeconds));
		SetTextIf(Txt_LastPlayed, View.LastPlayed > FDateTime::MinValue()
			? FText::FromString(View.LastPlayed.ToString(TEXT("%Y-%m-%d  %H:%M")))
			: LOCTEXT("UnknownTime", "-"));
		SetTextIf(Txt_ClearedStages, FText::Format(LOCTEXT("ClearedStages", "{0}곳"), FText::AsNumber(View.ClearedStages)));
		SetTextIf(Txt_TotalClears, FText::Format(LOCTEXT("TotalClears", "{0}회"), FText::AsNumber(View.TotalClears)));
	}
	else if (View.State == EKnightSaveSlotState::Unreadable)
	{
		SetTextIf(Txt_EmptyTitle, LOCTEXT("ErrorTitle", "저장 파일 오류"));
		SetTextIf(Txt_EmptyBody, LOCTEXT("ErrorBody", "파일을 읽을 수 없습니다.\n원본은 그대로 보존됩니다."));
	}
	else
	{
		SetTextIf(Txt_EmptyTitle, LOCTEXT("EmptyTitle", "빈 슬롯"));
		SetTextIf(Txt_EmptyBody, LOCTEXT("EmptyBody", "선택하면 새 게임을 시작합니다."));
	}
}

void UKnightSaveSlotCard::NativeOnHovered()
{
	Super::NativeOnHovered();
	// Mouse and keyboard share one highlight: hovering moves focus here.
	// Focus sits on the inner Slate button, so check descendants too.
	if (!HasAnyUserFocus() && !HasFocusedDescendants() && GetIsEnabled())
	{
		SetFocus();
	}
	RefreshFocusVisual();
}

void UKnightSaveSlotCard::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();
	RefreshFocusVisual();
}

void UKnightSaveSlotCard::HandleFocusReceived()
{
	Super::HandleFocusReceived();
	bFocused = true;
	RefreshFocusVisual();
}

void UKnightSaveSlotCard::HandleFocusLost()
{
	Super::HandleFocusLost();
	bFocused = false;
	RefreshFocusVisual();
}

void UKnightSaveSlotCard::RefreshFocusVisual()
{
	ShowIf(Img_FocusFrame, bFocused || IsHovered());
}

#undef LOCTEXT_NAMESPACE
