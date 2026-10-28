#include "HUD/knightHUDRepairKit.h"

#include "CommonTextBlock.h"
#include "Components/ProgressBar.h"
#include "Engine/LocalPlayer.h"
#include "Foundation/knightUISubsystem.h"
#include "ViewModels/knightVM_PlayerStatus.h"

void UknightHUDRepairKit::NativeConstruct()
{
	Super::NativeConstruct();

	const ULocalPlayer* const LocalPlayer = GetOwningLocalPlayer();
	const UknightUISubsystem* const UI = LocalPlayer ? LocalPlayer->GetSubsystem<UknightUISubsystem>() : nullptr;
	UknightVM_PlayerStatus* const VM = UI ? UI->GetPlayerStatusViewModel() : nullptr;
	if (!VM)
	{
		return;
	}
	ViewModel = VM;

	using FFields = UknightVM_PlayerStatus::FFieldNotificationClassDescriptor;
	const UE::FieldNotification::FFieldId Fields[] = { FFields::RepairKitCount, FFields::MaxRepairKitCount, FFields::EnergyPercent };
	for (const UE::FieldNotification::FFieldId& Field : Fields)
	{
		VM->AddFieldValueChangedDelegate(Field, INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::HandleViewModelChanged));
	}
	Refresh();
}

void UknightHUDRepairKit::NativeDestruct()
{
	if (UknightVM_PlayerStatus* const VM = ViewModel.Get())
	{
		VM->RemoveAllFieldValueChangedDelegates(this);
	}
	ViewModel.Reset();
	Super::NativeDestruct();
}

void UknightHUDRepairKit::HandleViewModelChanged(UObject* InViewModel, UE::FieldNotification::FFieldId FieldId)
{
	Refresh();
}

void UknightHUDRepairKit::Refresh()
{
	const UknightVM_PlayerStatus* const VM = ViewModel.Get();
	if (!VM)
	{
		return;
	}
	const int32 Count = VM->GetRepairKitCount();
	if (Bar_RepairGauge)
	{
		// (2026-10-28) The bar is the energy (0..100) at all times: perfect guards fill it and the convert key spends 50 on a kit.
		// It used to stay full while a kit was ready and only showed the charge toward the next automatic kit when there was none.
		Bar_RepairGauge->SetPercent(VM->GetEnergyPercent());
		Bar_RepairGauge->SetFillColorAndOpacity(GaugeColor);
	}
	if (Txt_RepairKitCount)
	{
		Txt_RepairKitCount->SetText(FText::AsNumber(Count));
	}
	SetRenderOpacity(Count > 0 ? 1.f : EmptyOpacity);
}
