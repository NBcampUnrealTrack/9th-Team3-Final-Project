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
	const UE::FieldNotification::FFieldId Fields[] = { FFields::RepairKitCount, FFields::MaxRepairKitCount, FFields::RepairKitGaugePercent };
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
	const bool bFull = VM->GetMaxRepairKitCount() > 0 && Count >= VM->GetMaxRepairKitCount();
	if (Bar_RepairGauge)
	{
		Bar_RepairGauge->SetPercent(bFull ? 0.f : VM->GetRepairKitGaugePercent());
		Bar_RepairGauge->SetFillColorAndOpacity(GaugeColor);
	}
	if (Txt_RepairKitCount)
	{
		Txt_RepairKitCount->SetText(FText::AsNumber(Count));
	}
	SetRenderOpacity(Count > 0 ? 1.f : EmptyOpacity);
}
