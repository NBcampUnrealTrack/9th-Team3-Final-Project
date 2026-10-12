#include "Widgets/knightSettingRow.h"

#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "knightUI"

UknightSettingRow::UknightSettingRow(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A full-width row should not grow on hover like a menu button.
	HoverScale = 1.f;
	PressedScale = 1.f;
}

void UknightSettingRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	OnClicked().AddUObject(this, &ThisClass::HandleRowClicked);
	if (Btn_Prev)
	{
		Btn_Prev->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePrevClicked);
	}
	if (Btn_Next)
	{
		Btn_Next->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleNextClicked);
	}
	if (Slider_Value)
	{
		// The builder makes the slider and the arrows unfocusable: focus stays on the row, they only take the mouse.
		Slider_Value->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleSliderChanged);
	}
}

void UknightSettingRow::Setup(const FKnightSettingItem& InItem)
{
	SetButtonText(InItem.Label);
	if (Slider_Value)
	{
		bUpdating = true;
		Slider_Value->SetMinValue(InItem.Min);
		Slider_Value->SetMaxValue(InItem.Max);
		Slider_Value->SetStepSize(1.f);
		bUpdating = false;
	}
	Refresh(InItem);
}

void UknightSettingRow::Refresh(const FKnightSettingItem& InItem)
{
	Item = InItem;
	SetIsEnabled(Item.bEnabled);
	if (Slider_Value)
	{
		bUpdating = true;
		Slider_Value->SetValue(FCString::Atof(*Item.Value));
		bUpdating = false;
	}
	if (Img_Dirty)
	{
		Img_Dirty->SetVisibility(Item.bDirty ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (!bCapturingKey)
	{
		RefreshValueText();
	}
}

void UknightSettingRow::RefreshValueText()
{
	if (!Txt_Value)
	{
		return;
	}
	switch (Item.Type)
	{
	case EKnightSettingControl::Slider:
		Txt_Value->SetText(FText::AsNumber(FMath::RoundToInt(FCString::Atof(*Item.Value))));
		break;
	case EKnightSettingControl::Key:
		Txt_Value->SetText(FKey(*Item.Value).GetDisplayName(false));
		break;
	default:
		Txt_Value->SetText(FText::FromString(Item.Value));
		break;
	}
}

void UknightSettingRow::NativeOnHovered()
{
	Super::NativeOnHovered();
	OnFocused.ExecuteIfBound(this, false);
}

void UknightSettingRow::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	OnFocused.ExecuteIfBound(this, true);
}

void UknightSettingRow::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
	SetCapturingKey(false);
}

FReply UknightSettingRow::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bCapturingKey)
	{
		FinishKeyCapture(InKeyEvent.GetKey());
		return FReply::Handled();
	}
	// Left / right change the value. Caught in preview: otherwise Slate turns them into focus navigation first,
	// which would jump out of the list.
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Left || Key == EKeys::Right)
	{
		if (Item.Type != EKnightSettingControl::Key)
		{
			Step(Key == EKeys::Right ? 1 : -1);
		}
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UknightSettingRow::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bCapturingKey)
	{
		FinishKeyCapture(InMouseEvent.GetEffectingButton());
		return FReply::Handled();
	}
	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

void UknightSettingRow::HandleRowClicked()
{
	if (Item.Type == EKnightSettingControl::Rotator)
	{
		Step(1);
	}
	else if (Item.Type == EKnightSettingControl::Key)
	{
		SetCapturingKey(true);
	}
}

void UknightSettingRow::HandlePrevClicked() { Step(-1); }
void UknightSettingRow::HandleNextClicked() { Step(1); }

void UknightSettingRow::HandleSliderChanged(float Value)
{
	if (bUpdating)
	{
		return;
	}
	const FString NewValue = FString::SanitizeFloat(FMath::RoundToFloat(Value));
	if (NewValue != Item.Value)
	{
		PlayChangeSound();
		Edit(NewValue);
	}
}

float UknightSettingRow::GetSliderStep() const
{
	const float Range = Item.Max - Item.Min;
	return Range <= 50.f ? 1.f : (Range <= 100.f ? 5.f : 10.f);
}

void UknightSettingRow::Step(int32 Direction)
{
	if (!GetIsEnabled())
	{
		return;
	}
	if (Item.Type == EKnightSettingControl::Rotator && Item.Options.Num() > 0)
	{
		// Wraps around, so a two-option switch flips either way.
		const int32 Current = FMath::Max(0, Item.Options.IndexOfByKey(Item.Value));
		const int32 Next = (Current + Direction + Item.Options.Num()) % Item.Options.Num();
		PlayChangeSound();
		Edit(Item.Options[Next]);
	}
	else if (Item.Type == EKnightSettingControl::Slider)
	{
		const float Current = FCString::Atof(*Item.Value);
		const float StepSize = GetSliderStep();
		const float Next = FMath::Clamp(FMath::GridSnap(Current + Direction * StepSize, StepSize), Item.Min, Item.Max);
		if (!FMath::IsNearlyEqual(Next, Current))
		{
			PlayChangeSound();
			Edit(FString::SanitizeFloat(Next));
		}
	}
}

void UknightSettingRow::Edit(const FString& Value)
{
	// Shown at once; the screen's refresh confirms it (or puts back a rejected value).
	Item.Value = Value;
	RefreshValueText();
	OnEdited.ExecuteIfBound(Item.Id, Value);
}

void UknightSettingRow::SetCapturingKey(bool bCapturing)
{
	if (bCapturingKey == bCapturing)
	{
		return;
	}
	bCapturingKey = bCapturing;
	if (bCapturing && Txt_Value)
	{
		Txt_Value->SetText(LOCTEXT("SettingKeyWaiting", "키를 누르세요"));
	}
	else
	{
		RefreshValueText();
	}
}

void UknightSettingRow::FinishKeyCapture(const FKey& Key)
{
	SetCapturingKey(false);
	if (Key == EKeys::Escape || !Key.IsValid())
	{
		return;
	}
	Edit(Key.GetFName().ToString());
}

void UknightSettingRow::PlayChangeSound()
{
	const double Now = FPlatformTime::Seconds();
	if (ChangeSound && Now - LastSoundTime >= ChangeSoundInterval)
	{
		LastSoundTime = Now;
		UGameplayStatics::PlaySound2D(this, ChangeSound);
	}
}

#undef LOCTEXT_NAMESPACE
