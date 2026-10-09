#include "HUD/knightHUDRaidTimer.h"

#include "CommonTextBlock.h"
#include "Engine/LocalPlayer.h"
#include "Foundation/knightUISubsystem.h"
#include "ViewModels/knightVM_Raid.h"

void UknightHUDRaidTimer::NativeConstruct()
{
	Super::NativeConstruct();

	const ULocalPlayer* const LocalPlayer = GetOwningLocalPlayer();
	const UknightUISubsystem* const UI = LocalPlayer ? LocalPlayer->GetSubsystem<UknightUISubsystem>() : nullptr;
	UknightVM_Raid* const VM = UI ? UI->GetRaidViewModel() : nullptr;
	ViewModel = VM;
	if (VM)
	{
		using FFields = UknightVM_Raid::FFieldNotificationClassDescriptor;
		const UE::FieldNotification::FFieldId Fields[] = { FFields::bHasTimeLimit, FFields::RemainingTime, FFields::RemainingTimeText, FFields::ObjectiveText };
		for (const UE::FieldNotification::FFieldId& Field : Fields)
		{
			VM->AddFieldValueChangedDelegate(Field, INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::HandleViewModelChanged));
		}
	}
	Refresh();
}

void UknightHUDRaidTimer::NativeDestruct()
{
	if (UknightVM_Raid* const VM = ViewModel.Get())
	{
		VM->RemoveAllFieldValueChangedDelegates(this);
	}
	ViewModel.Reset();
	Super::NativeDestruct();
}

void UknightHUDRaidTimer::HandleViewModelChanged(UObject* InViewModel, UE::FieldNotification::FFieldId FieldId)
{
	Refresh();
}

void UknightHUDRaidTimer::Refresh()
{
	const UknightVM_Raid* const VM = ViewModel.Get();
	const bool bHasTime = VM && VM->HasTimeLimit();
	const bool bHasObjective = VM && !VM->GetObjectiveText().IsEmpty();
	SetVisibility(bHasTime || bHasObjective ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (!VM)
	{
		return;
	}
	if (Txt_Objective)
	{
		Txt_Objective->SetText(VM->GetObjectiveText());
	}
	if (Txt_Time)
	{
		Txt_Time->SetVisibility(bHasTime ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Txt_Time->SetText(VM->GetRemainingTimeText());
		Txt_Time->SetColorAndOpacity(VM->GetRemainingTime() <= WarningSeconds ? WarningColor : NormalColor);
	}
}
