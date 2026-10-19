#include "HUD/knightHUDLowHealth.h"

#include "Components/Image.h"
#include "Engine/LocalPlayer.h"
#include "Foundation/knightUISettings.h"
#include "Foundation/knightUISubsystem.h"
#include "ViewModels/knightVM_PlayerStatus.h"

void UknightHUDLowHealth::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);

	const ULocalPlayer* const LocalPlayer = GetOwningLocalPlayer();
	const UknightUISubsystem* const UI = LocalPlayer ? LocalPlayer->GetSubsystem<UknightUISubsystem>() : nullptr;
	UknightVM_PlayerStatus* const VM = UI ? UI->GetPlayerStatusViewModel() : nullptr;
	if (!VM)
	{
		return;
	}
	ViewModel = VM;
	const auto Delegate = INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::HandleViewModelChanged);
	VM->AddFieldValueChangedDelegate(UknightVM_PlayerStatus::FFieldNotificationClassDescriptor::HealthPercent, Delegate);
	VM->AddFieldValueChangedDelegate(UknightVM_PlayerStatus::FFieldNotificationClassDescriptor::MaxHealth, Delegate);
	Refresh();
}

void UknightHUDLowHealth::NativeDestruct()
{
	if (UknightVM_PlayerStatus* const VM = ViewModel.Get())
	{
		VM->RemoveAllFieldValueChangedDelegates(this);
	}
	ViewModel.Reset();
	if (Ticker.IsValid())
	{
		FTSTicker::RemoveTicker(Ticker);
		Ticker.Reset();
	}
	Super::NativeDestruct();
}

void UknightHUDLowHealth::HandleViewModelChanged(UObject* InViewModel, UE::FieldNotification::FFieldId FieldId)
{
	Refresh();
}

void UknightHUDLowHealth::Refresh()
{
	const UknightVM_PlayerStatus* const VM = ViewModel.Get();
	// Same ratio as the HealthWarning voice, so the voice, the red health bar and the edges all start together.
	const float Threshold = GetDefault<UknightUISettings>()->HealthWarningRatio;
	const float Percent = VM ? VM->GetHealthPercent() : 1.f;
	bLow = VM && VM->GetMaxHealth() > 0.f && Threshold > 0.f && Percent <= Threshold;
	if (bLow)
	{
		Severity = FMath::Clamp(1.f - Percent / Threshold, 0.f, 1.f);
	}

	if ((bLow || Shown > 0.f) && !Ticker.IsValid() && !IsDesignTime())
	{
		if (Shown <= 0.f)
		{
			BeatTime = 0.f;
		}
		SetVisibility(ESlateVisibility::HitTestInvisible);
		Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime) { return Tick(DeltaTime); }));
		Tick(0.f);
	}
}

bool UknightHUDLowHealth::Tick(float DeltaTime)
{
	DeltaTime = FMath::Min(DeltaTime, 0.05f);
	Shown = bLow ? 1.f : (FadeOutTime > 0.f ? FMath::Max(0.f, Shown - DeltaTime / FadeOutTime) : 0.f);
	if (Shown <= 0.f)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		Ticker.Reset();
		return false;
	}

	const float Period = FMath::Lerp(SlowBeatPeriod, FastBeatPeriod, Severity);
	BeatTime = FMath::Fmod(BeatTime + DeltaTime, Period);
	// Two quick bumps per period, the second weaker: a heartbeat rather than a smooth blink.
	const float Phase = BeatTime / Period;
	const float Beat = FMath::Exp(-FMath::Square((Phase - 0.08f) / 0.06f)) + 0.6f * FMath::Exp(-FMath::Square((Phase - 0.3f) / 0.06f));
	const float Opacity = Shown * FMath::Min(1.f, FMath::Lerp(BaseOpacity.X, BaseOpacity.Y, Severity) + FMath::Lerp(BeatOpacity.X, BeatOpacity.Y, Severity) * Beat);

	if (Img_Vignette)
	{
		Img_Vignette->SetRenderOpacity(Opacity);
	}
	if (Img_Scanlines)
	{
		Img_Scanlines->SetRenderOpacity(Opacity * ScanlineOpacity);
		ScanOffset = FMath::Fmod(ScanOffset + ScanlineSpeed * DeltaTime, ScanlineTile);
		// The image starts one tile above the screen, so sliding it down within a tile still covers the top edge.
		Img_Scanlines->SetRenderTranslation(FVector2D(0.f, ScanOffset));
	}
	return true;
}
