#include "HUD/knightHUDGroggyNoise.h"

#include "Components/Image.h"
#include "Engine/LocalPlayer.h"
#include "Foundation/knightUISubsystem.h"
#include "ViewModels/knightVM_PlayerStatus.h"

void UknightHUDGroggyNoise::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	for (int32 Index = 0; Index < 6; ++Index)
	{
		if (UImage* const Bar = Cast<UImage>(GetWidgetFromName(*FString::Printf(TEXT("Img_Scan%d"), Index))))
		{
			ScanBars.Add(Bar);
		}
	}
}

void UknightHUDGroggyNoise::NativeConstruct()
{
	Super::NativeConstruct();
	SetActive(false);

	const ULocalPlayer* const LocalPlayer = GetOwningLocalPlayer();
	const UknightUISubsystem* const UI = LocalPlayer ? LocalPlayer->GetSubsystem<UknightUISubsystem>() : nullptr;
	UknightVM_PlayerStatus* const VM = UI ? UI->GetPlayerStatusViewModel() : nullptr;
	if (!VM)
	{
		return;
	}
	ViewModel = VM;
	VM->AddFieldValueChangedDelegate(UknightVM_PlayerStatus::FFieldNotificationClassDescriptor::bGroggy,
		INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::HandleViewModelChanged));
	SetActive(VM->IsGroggy());
}

void UknightHUDGroggyNoise::NativeDestruct()
{
	if (UknightVM_PlayerStatus* const VM = ViewModel.Get())
	{
		VM->RemoveAllFieldValueChangedDelegates(this);
	}
	ViewModel.Reset();
	SetActive(false);
	Super::NativeDestruct();
}

void UknightHUDGroggyNoise::HandleViewModelChanged(UObject* InViewModel, UE::FieldNotification::FFieldId FieldId)
{
	const UknightVM_PlayerStatus* const VM = ViewModel.Get();
	SetActive(VM && VM->IsGroggy());
}

void UknightHUDGroggyNoise::SetActive(bool bActive)
{
	SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bActive && !Ticker.IsValid() && !IsDesignTime())
	{
		ActiveTime = 0.f;
		NextJump = 0.f;
		Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime) { return Tick(DeltaTime); }));
		Tick(0.f);
	}
	else if (!bActive && Ticker.IsValid())
	{
		FTSTicker::RemoveTicker(Ticker);
		Ticker.Reset();
	}
}

bool UknightHUDGroggyNoise::Tick(float DeltaTime)
{
	ActiveTime += FMath::Min(DeltaTime, 0.05f);
	const float Alpha = BurstTime > 0.f ? FMath::Clamp(ActiveTime / BurstTime, 0.f, 1.f) : 1.f;
	SetRenderOpacity(FMath::Lerp(BurstOpacity, HoldOpacity, Alpha));
	if (ActiveTime < NextJump)
	{
		return true;
	}
	NextJump = ActiveTime + JumpInterval;

	const float Height = FMath::Max(GetCachedGeometry().GetLocalSize().Y, 1.f);
	if (Img_Noise)
	{
		// The image is one tile larger than the screen on each axis, so any shift within a tile still covers it.
		Img_Noise->SetRenderTranslation(FVector2D(-FMath::FRandRange(0.f, 256.f), -FMath::FRandRange(0.f, 256.f)));
	}
	for (UImage* const Bar : ScanBars)
	{
		if (!Bar)
		{
			continue;
		}
		Bar->SetRenderTranslation(FVector2D(0.f, FMath::FRandRange(0.f, Height)));
		Bar->SetRenderScale(FVector2D(1.f, FMath::FRandRange(0.5f, 4.f)));
		Bar->SetRenderOpacity(FMath::FRandRange(0.15f, 0.6f));
	}
	return true;
}
