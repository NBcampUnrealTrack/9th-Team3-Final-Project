#include "HUD/knightHUDFrameStatus.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Engine/LocalPlayer.h"
#include "Foundation/knightUISubsystem.h"
#include "ViewModels/knightVM_PlayerStatus.h"

void UknightHUDFrameStatus::NativeConstruct()
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
	const UE::FieldNotification::FFieldId Fields[] = {
		FFields::HeadHealthPercent, FFields::TorsoHealthPercent, FFields::ArmsHealthPercent, FFields::LegsHealthPercent,
		FFields::Stagger, FFields::bGroggy };
	for (const UE::FieldNotification::FFieldId& Field : Fields)
	{
		VM->AddFieldValueChangedDelegate(Field, INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::HandleViewModelChanged));
	}
	Refresh();
}

void UknightHUDFrameStatus::NativeDestruct()
{
	if (UknightVM_PlayerStatus* const VM = ViewModel.Get())
	{
		VM->RemoveAllFieldValueChangedDelegates(this);
	}
	ViewModel.Reset();
	Super::NativeDestruct();
}

void UknightHUDFrameStatus::HandleViewModelChanged(UObject* InViewModel, UE::FieldNotification::FFieldId FieldId)
{
	Refresh();
}

FLinearColor UknightHUDFrameStatus::GetPartColor(float HealthPercent) const
{
	return HealthPercent <= 0.f ? BrokenColor : HealthPercent <= DamagedThreshold ? DamagedColor : NormalColor;
}

void UknightHUDFrameStatus::Refresh()
{
	const UknightVM_PlayerStatus* const VM = ViewModel.Get();
	if (!VM)
	{
		return;
	}
	const TPair<UImage*, EknightBodyPart> Parts[] = {
		{ Img_PartHead, EknightBodyPart::Head }, { Img_PartTorso, EknightBodyPart::Torso },
		{ Img_PartArms, EknightBodyPart::Arms }, { Img_PartLegs, EknightBodyPart::Legs } };
	for (const TPair<UImage*, EknightBodyPart>& Part : Parts)
	{
		if (Part.Key)
		{
			Part.Key->SetColorAndOpacity(GetPartColor(VM->GetPartHealthPercent(Part.Value)));
		}
	}
	if (Bar_Groggy)
	{
		// Poise sits at 0 for the whole stun, so the silhouette stays full until the Knight recovers.
		Bar_Groggy->SetPercent(VM->IsGroggy() ? 1.f : VM->GetStagger());
		Bar_Groggy->SetFillColorAndOpacity(GroggyColor);
	}
}
