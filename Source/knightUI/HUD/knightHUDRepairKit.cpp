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
	if (Bar_RepairGauge)
	{
		// (2026-10-29 사용자 요청) 키트 개수와 상관없이 늘 수리 키트 게이지(0~100)만큼 찬다. 다 차면 키트 +1과 함께 게이지가 0으로 돌아간다.
		// 전에는 키트가 하나라도 있으면 꽉 찬 채로 그려서 게이지가 차는 게 키트 0개일 때만 보였다.
		Bar_RepairGauge->SetPercent(VM->GetRepairKitGaugePercent());
		Bar_RepairGauge->SetFillColorAndOpacity(GaugeColor);
	}
	if (Txt_RepairKitCount)
	{
		Txt_RepairKitCount->SetText(FText::AsNumber(Count));
	}
	SetRenderOpacity(Count > 0 ? 1.f : EmptyOpacity);
}
