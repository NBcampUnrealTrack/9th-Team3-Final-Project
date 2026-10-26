#include "Widgets/knightButtonBase.h"

#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Framework/Application/SlateApplication.h"

void UknightButtonBase::ConfigureMaintenanceButton(bool bInSelectable, bool bMainTab)
{
	bMaintenancePresentation = true;
	// CommonUI otherwise auto-selects AFTER the owner has cleared/toggled its selection.
	// Dispatch selection first, then let the screen be the final authority.
	bTriggerClickedAfterSelection = true;
	SetShouldSelectUponReceivingFocus(false);
	SetIsSelectable(bInSelectable);
	SetIsInteractableWhenSelected(true);
	FocusedTextStyle = nullptr;
	HoverScale = 1.f;
	PressedScale = 0.99f;
	const TCHAR* Path = bMainTab
		? TEXT("/Game/Knight/UI/Style/BS_Knight_CraftingMainTab.BS_Knight_CraftingMainTab_C")
		: TEXT("/Game/Knight/UI/Style/BS_Knight_CraftingControl.BS_Knight_CraftingControl_C");
	if (const TSubclassOf<UCommonButtonStyle> StyleClass = LoadClass<UCommonButtonStyle>(nullptr, Path)) SetStyle(StyleClass);
	const auto TintFocusImage = [](UWidget* Widget)
	{
		if (UImage* Image = Cast<UImage>(Widget)) Image->SetColorAndOpacity(FLinearColor(0.32f, 0.57f, 0.76f, 0.65f));
	};
	for (UWidget* Bracket : { Bracket_L.Get(), Bracket_R.Get() })
	{
		TintFocusImage(Bracket);
		// The shipped bracket is a nested WBP with three Images, not an Image itself.
		if (UUserWidget* Nested = Cast<UUserWidget>(Bracket))
			if (Nested->WidgetTree) Nested->WidgetTree->ForEachWidget(TintFocusImage);
	}
	RefreshTargets();
	ApplyLabelStyle();
}

void UknightButtonBase::SetButtonText(const FText& InText)
{
	ButtonText = InText;
	if (Txt_Label)
	{
		Txt_Label->SetText(ButtonText);
	}
}

void UknightButtonBase::SetLockedWithReason(bool bInLocked, const FText& InReason)
{
	LockReason = InReason;
	SetIsLocked(bInLocked);
	RefreshLockVisual();
	RefreshTargets();
}

void UknightButtonBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	OnLockClicked().AddUObject(this, &UknightButtonBase::HandleLockClicked);
}

void UknightButtonBase::NativePreConstruct()
{
	Super::NativePreConstruct();

	SetButtonText(ButtonText);
	CurrentScale = TargetScale = 1.f;
	BracketAlpha = TargetBracketAlpha = 0.f;
	ShakeTimeLeft = 0.f;
	bPressedVisual = bFocusedVisual = false;
	ApplyMotion();
	RefreshLockVisual();
}

void UknightButtonBase::NativeDestruct()
{
	StopAnimating();
	Super::NativeDestruct();
}

void UknightButtonBase::NativeOnHovered()
{
	Super::NativeOnHovered();
	// Focus sits on the inner Slate button, so check descendants too.
	if (bFocusOnHover && GetIsEnabled() && !HasAnyUserFocus() && !HasFocusedDescendants())
	{
		SetFocus();
	}
	RefreshTargets();
}

void UknightButtonBase::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();
	bPressedVisual = false;
	RefreshTargets();
}

void UknightButtonBase::NativeOnPressed()
{
	Super::NativeOnPressed();
	bPressedVisual = true;
	RefreshTargets();
}

void UknightButtonBase::NativeOnReleased()
{
	Super::NativeOnReleased();
	bPressedVisual = false;
	RefreshTargets();
}

void UknightButtonBase::NativeOnClicked()
{
	// CommonUI selects before routing the locked-click notification too. A locked slot is not a selected slot.
	if (bMaintenancePresentation && GetLocked()) ClearSelection();
	Super::NativeOnClicked();
}

void UknightButtonBase::NativeOnEnabled()
{
	Super::NativeOnEnabled();
	RefreshTargets();
}

void UknightButtonBase::NativeOnDisabled()
{
	Super::NativeOnDisabled();
	bPressedVisual = false;
	RefreshTargets();
}

void UknightButtonBase::NativeOnCurrentTextStyleChanged()
{
	Super::NativeOnCurrentTextStyleChanged();
	ApplyLabelStyle();
}

void UknightButtonBase::HandleFocusReceived()
{
	Super::HandleFocusReceived();
	bFocusedVisual = true;
	RefreshTargets();
	ApplyLabelStyle();
}

void UknightButtonBase::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);

	// The style plays the hover sound for the mouse only. Keyboard navigation gets the same sound here; focus set
	// by code (a screen opening) or by a click stays silent, so nothing plays twice.
	if (InFocusEvent.GetCause() != EFocusCause::Navigation || IsHovered())
	{
		return;
	}
	if (const UCommonButtonStyle* const ButtonStyle = GetStyleCDO())
	{
		const FSlateSound& Sound = GetLocked() && ButtonStyle->LockedHoveredSlateSound ? ButtonStyle->LockedHoveredSlateSound.Sound : ButtonStyle->HoveredSlateSound;
		FSlateApplication::Get().PlaySound(Sound);
	}
}

void UknightButtonBase::HandleFocusLost()
{
	Super::HandleFocusLost();
	bFocusedVisual = false;
	bPressedVisual = false;
	RefreshTargets();
	ApplyLabelStyle();
}

void UknightButtonBase::HandleLockClicked()
{
	ShakeTimeLeft = ShakeDuration;
	StartAnimating();
}

void UknightButtonBase::RefreshTargets()
{
	// Locked buttons still react to hover (cursor, tooltip) but never grow or open their brackets.
	const bool bInteractive = GetIsEnabled() && !GetLocked();
	const bool bHot = bInteractive && (IsHovered() || bFocusedVisual);

	TargetScale = !bInteractive ? 1.f : (bPressedVisual ? PressedScale : (bHot ? HoverScale : 1.f));
	TargetBracketAlpha = (bMaintenancePresentation ? bInteractive && bFocusedVisual : bHot) ? 1.f : 0.f;

	if (IsDesignTime())
	{
		CurrentScale = TargetScale;
		BracketAlpha = TargetBracketAlpha;
		ApplyMotion();
		return;
	}
	StartAnimating();
}

void UknightButtonBase::RefreshLockVisual()
{
	const bool bLockedNow = GetLocked();
	SetRenderOpacity(bLockedNow ? LockedOpacity : 1.f);
	SetCursor(bLockedNow ? EMouseCursor::SlashedCircle : EMouseCursor::Hand);
	SetToolTipText(bLockedNow ? LockReason : FText::GetEmpty());
	ApplyLabelStyle();
}

void UknightButtonBase::ApplyLabelStyle()
{
	if (!Txt_Label)
	{
		return;
	}
	if (GetLocked() && LockedTextStyle)
	{
		Txt_Label->SetStyle(LockedTextStyle);
	}
	else if (bFocusedVisual && FocusedTextStyle && GetIsEnabled())
	{
		Txt_Label->SetStyle(FocusedTextStyle);
	}
	else if (const TSubclassOf<UCommonTextStyle> StyleClass = GetCurrentTextStyleClass())
	{
		Txt_Label->SetStyle(StyleClass);
	}
}

void UknightButtonBase::StartAnimating()
{
	// Runs only while something is moving, so idle buttons cost nothing per frame. Core ticker keeps running while
	// the game is paused, which is when most menus are open.
	if (!MotionTicker.IsValid())
	{
		MotionTicker = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime) { return TickMotion(DeltaTime); }));
	}
}

void UknightButtonBase::StopAnimating()
{
	if (MotionTicker.IsValid())
	{
		FTSTicker::RemoveTicker(MotionTicker);
		MotionTicker.Reset();
	}
}

bool UknightButtonBase::TickMotion(float DeltaTime)
{
	DeltaTime = FMath::Min(DeltaTime, 0.05f);

	CurrentScale = FMath::FInterpTo(CurrentScale, TargetScale, DeltaTime, ScaleInterpSpeed);
	BracketAlpha = FMath::FInterpTo(BracketAlpha, TargetBracketAlpha, DeltaTime, BracketInterpSpeed);
	ShakeTimeLeft = FMath::Max(0.f, ShakeTimeLeft - DeltaTime);

	const bool bSettled = FMath::IsNearlyEqual(CurrentScale, TargetScale, 0.001f)
		&& FMath::IsNearlyEqual(BracketAlpha, TargetBracketAlpha, 0.01f)
		&& ShakeTimeLeft <= 0.f;
	if (bSettled)
	{
		CurrentScale = TargetScale;
		BracketAlpha = TargetBracketAlpha;
	}

	ApplyMotion();

	if (bSettled)
	{
		MotionTicker.Reset();
		return false;
	}
	return true;
}

void UknightButtonBase::ApplyMotion()
{
	float ShakeOffset = 0.f;
	if (ShakeTimeLeft > 0.f && ShakeDuration > 0.f)
	{
		const float Progress = 1.f - ShakeTimeLeft / ShakeDuration;
		// Two full swings that fade out.
		ShakeOffset = FMath::Sin(Progress * UE_TWO_PI * 2.f) * ShakeAmplitude * (1.f - Progress);
	}

	FWidgetTransform Transform;
	Transform.Scale = FVector2D(CurrentScale);
	Transform.Translation = FVector2D(ShakeOffset, 0.f);
	SetRenderTransform(Transform);

	const float BracketScaleY = FMath::Lerp(BracketClosedScale, 1.f, BracketAlpha);
	if (Bracket_L)
	{
		Bracket_L->SetRenderOpacity(BracketAlpha);
		Bracket_L->SetRenderScale(FVector2D(1.f, BracketScaleY));
	}
	if (Bracket_R)
	{
		// The right bracket is the left one mirrored.
		Bracket_R->SetRenderOpacity(BracketAlpha);
		Bracket_R->SetRenderScale(FVector2D(-1.f, BracketScaleY));
	}
}
