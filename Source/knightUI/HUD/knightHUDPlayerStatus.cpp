#include "HUD/knightHUDPlayerStatus.h"

#include "CommonTextBlock.h"
#include "Components/ProgressBar.h"
#include "Engine/LocalPlayer.h"
#include "Foundation/knightUISubsystem.h"
#include "ViewModels/knightVM_PlayerStatus.h"

void UknightHUDPlayerStatus::NativeConstruct()
{
	Super::NativeConstruct();

	if (Txt_Overheat)
	{
		Txt_Overheat->SetVisibility(ESlateVisibility::Collapsed);
	}

	const ULocalPlayer* const LocalPlayer = GetOwningLocalPlayer();
	const UknightUISubsystem* const UI = LocalPlayer ? LocalPlayer->GetSubsystem<UknightUISubsystem>() : nullptr;
	UknightVM_PlayerStatus* const VM = UI ? UI->GetPlayerStatusViewModel() : nullptr;
	if (!VM)
	{
		return;
	}
	ViewModel = VM;

	using FFields = UknightVM_PlayerStatus::FFieldNotificationClassDescriptor;
	const UE::FieldNotification::FFieldId Fields[] = { FFields::Health, FFields::MaxHealth, FFields::HealthPercent, FFields::BoostPercent, FFields::bOverheated };
	for (const UE::FieldNotification::FFieldId& Field : Fields)
	{
		VM->AddFieldValueChangedDelegate(Field, INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::HandleViewModelChanged));
	}
	Refresh(true);
}

void UknightHUDPlayerStatus::NativeDestruct()
{
	if (UknightVM_PlayerStatus* const VM = ViewModel.Get())
	{
		VM->RemoveAllFieldValueChangedDelegates(this);
	}
	ViewModel.Reset();
	if (AnimTicker.IsValid())
	{
		FTSTicker::RemoveTicker(AnimTicker);
		AnimTicker.Reset();
	}
	Super::NativeDestruct();
}

void UknightHUDPlayerStatus::HandleViewModelChanged(UObject* InViewModel, UE::FieldNotification::FFieldId FieldId)
{
	Refresh(false);
}

void UknightHUDPlayerStatus::Refresh(bool bSnapGhost)
{
	const UknightVM_PlayerStatus* const VM = ViewModel.Get();
	if (!VM)
	{
		return;
	}

	const float NewHealth = FMath::Clamp(VM->GetHealthPercent(), 0.f, 1.f);
	if (bSnapGhost || NewHealth >= GhostPercent)
	{
		// Healing (or the first value) moves the ghost with the bar; only damage leaves a trail.
		GhostPercent = NewHealth;
		GhostHoldLeft = 0.f;
	}
	else if (NewHealth < HealthPercent)
	{
		// Each new hit restarts the hold so a combo reads as one chunk.
		GhostHoldLeft = GhostHoldTime;
	}
	HealthPercent = NewHealth;
	bLowHealth = VM->GetMaxHealth() > 0.f && HealthPercent <= LowHealthThreshold;
	bOverheated = VM->IsOverheated();
	bLowBoost = VM->GetMaxBoostGauge() > 0.f && VM->GetBoostPercent() <= LowBoostThreshold;

	if (Bar_Health)
	{
		Bar_Health->SetPercent(HealthPercent);
		Bar_Health->SetFillColorAndOpacity(bLowHealth ? LowHealthColor : HealthColor);
	}
	if (Bar_HealthGhost)
	{
		Bar_HealthGhost->SetPercent(GhostPercent);
		Bar_HealthGhost->SetFillColorAndOpacity(GhostColor);
	}
	if (Txt_Health)
	{
		// Rounded up so a sliver of health never reads as 0.
		Txt_Health->SetText(FText::AsNumber(FMath::CeilToInt(VM->GetHealth())));
		Txt_Health->SetColorAndOpacity(bLowHealth ? LowHealthColor : HealthColor);
	}
	if (Txt_HealthMax)
	{
		Txt_HealthMax->SetText(FText::Format(NSLOCTEXT("knightHUD", "HealthMaxReadout", "/{0}"), FText::AsNumber(FMath::RoundToInt(VM->GetMaxHealth()))));
	}
	if (Img_BoostAlert)
	{
		Img_BoostAlert->SetVisibility(bLowBoost ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (Bar_Boost)
	{
		Bar_Boost->SetPercent(FMath::Clamp(VM->GetBoostPercent(), 0.f, 1.f));
		Bar_Boost->SetFillColorAndOpacity(bOverheated ? OverheatColor : BoostColor);
	}
	if (Txt_Boost)
	{
		Txt_Boost->SetText(FText::AsPercent(FMath::Clamp(VM->GetBoostPercent(), 0.f, 1.f)));
	}
	if (Txt_Overheat)
	{
		Txt_Overheat->SetVisibility(bOverheated ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (NeedsTick())
	{
		UpdateTicker();
	}
	else
	{
		// Nothing left to animate: stop ticking and restore full opacity.
		if (AnimTicker.IsValid())
		{
			FTSTicker::RemoveTicker(AnimTicker);
			AnimTicker.Reset();
		}
		PulseTime = 0.f;
		ApplyAnimation(0.f);
	}
}

bool UknightHUDPlayerStatus::NeedsTick() const
{
	return GhostPercent > HealthPercent || bLowHealth || bOverheated || bLowBoost;
}

void UknightHUDPlayerStatus::UpdateTicker()
{
	if (!AnimTicker.IsValid() && !IsDesignTime())
	{
		AnimTicker = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime) { return TickAnimation(DeltaTime); }));
	}
}

bool UknightHUDPlayerStatus::TickAnimation(float DeltaTime)
{
	ApplyAnimation(FMath::Min(DeltaTime, 0.05f));
	if (!NeedsTick())
	{
		AnimTicker.Reset();
		return false;
	}
	return true;
}

void UknightHUDPlayerStatus::ApplyAnimation(float DeltaTime)
{
	if (GhostPercent > HealthPercent)
	{
		if (GhostHoldLeft > 0.f)
		{
			GhostHoldLeft -= DeltaTime;
		}
		else
		{
			GhostPercent = FMath::Max(HealthPercent, GhostPercent - DeltaTime / GhostDrainTime);
		}
		if (Bar_HealthGhost)
		{
			Bar_HealthGhost->SetPercent(GhostPercent);
		}
	}

	PulseTime = (bLowHealth || bOverheated || bLowBoost) ? FMath::Fmod(PulseTime + DeltaTime, PulsePeriod) : 0.f;
	// Cosine starts at full opacity so a pulse never begins with a sudden dip.
	const float Pulse = FMath::Lerp(PulseMinOpacity, 1.f, 0.5f + 0.5f * FMath::Cos(PulseTime / PulsePeriod * UE_TWO_PI));
	if (Bar_Health)
	{
		Bar_Health->SetRenderOpacity(bLowHealth ? Pulse : 1.f);
	}
	if (Bar_Boost)
	{
		Bar_Boost->SetRenderOpacity(bOverheated ? Pulse : 1.f);
	}
	if (Txt_Overheat)
	{
		Txt_Overheat->SetRenderOpacity(bOverheated ? Pulse : 1.f);
	}
	if (Img_BoostAlert)
	{
		// Blinks fully out rather than dimming, so the warning reads even in the corner of the eye.
		Img_BoostAlert->SetRenderOpacity(bLowBoost ? (Pulse - PulseMinOpacity) / FMath::Max(1.f - PulseMinOpacity, KINDA_SMALL_NUMBER) : 1.f);
	}
}
