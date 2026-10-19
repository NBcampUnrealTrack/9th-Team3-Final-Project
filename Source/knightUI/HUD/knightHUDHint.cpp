#include "HUD/knightHUDHint.h"

#include "CommonTextBlock.h"

void UknightHUDHint::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);
}

void UknightHUDHint::NativeDestruct()
{
	FTSTicker::RemoveTicker(HideTicker);
	HideTicker.Reset();
	Super::NativeDestruct();
}

void UknightHUDHint::ShowHint(const FText& Key, const FText& Body, float Duration)
{
	if (Txt_HintKey)
	{
		Txt_HintKey->SetText(Key);
		Txt_HintKey->SetVisibility(Key.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (Txt_HintBody)
	{
		Txt_HintBody->SetText(Body);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	FTSTicker::RemoveTicker(HideTicker);
	HideTicker.Reset();
	if (Duration > 0.f)
	{
		HideTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
		{
			HideTicker.Reset();
			HideHint();
			return false;
		}), Duration);
	}
}

void UknightHUDHint::HideHint()
{
	FTSTicker::RemoveTicker(HideTicker);
	HideTicker.Reset();
	SetVisibility(ESlateVisibility::Collapsed);
}
