#include "KnightMenuWidgetBase.h"

#include "GameFlow/KnightGameFlowSubsystem.h"
#include "CommonButtonBase.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "knight.h"

#define LOCTEXT_NAMESPACE "KnightMenuWidget"

void UKnightMenuWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	ValidateRequiredWidget(Switcher_Pages, TEXT("Switcher_Pages"));
	ValidateRequiredWidget(Button_Back, TEXT("Button_Back"));
	ValidateRequiredWidget(Text_Status, TEXT("Text_Status"));
	if (Button_Back)
	{
		Button_Back->OnClicked().AddUObject(this, &ThisClass::HandleBackClicked);
	}
	if (IsInsideSwitcher(Button_Back) || IsInsideSwitcher(Text_Status))
	{
		UE_LOG(Logknight, Error, TEXT("%s: Button_Back and Text_Status must be outside Switcher_Pages."), *GetClass()->GetName());
	}
}

void UKnightMenuWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	SubscribeFlow();
	if (UKnightGameFlowSubsystem* const Flow = GetFlow())
	{
		if (Flow->GetCurrentFlowState() != GetExpectedFlowState())
		{
			UE_LOG(Logknight, Error, TEXT("%s constructed for flow state %d instead of %d."),
				*GetClass()->GetName(), static_cast<int32>(Flow->GetCurrentFlowState()), static_cast<int32>(GetExpectedFlowState()));
			SetStatusText(LOCTEXT("WrongFlowState", "메뉴 상태를 확인할 수 없습니다."));
		}
	}
	else
	{
		UE_LOG(Logknight, Error, TEXT("%s could not find KnightGameFlowSubsystem."), *GetClass()->GetName());
		SetStatusText(LOCTEXT("MissingFlow", "메뉴 기능을 사용할 수 없습니다."));
	}
	RefreshTravelControls();
}

void UKnightMenuWidgetBase::NativeDestruct()
{
	UnsubscribeFlow();
	Super::NativeDestruct();
}

FReply UKnightMenuWidgetBase::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (ShouldAllowEscapeToChild()) return FReply::Unhandled();
		RequestBack();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

bool UKnightMenuWidgetBase::RequestBack()
{
	return !IsTravelPending() && HandleBackRequested();
}

void UKnightMenuWidgetBase::HandleBackClicked()
{
	RequestBack();
}

void UKnightMenuWidgetBase::FocusCurrentPage()
{
	APlayerController* const OwningPlayer = GetOwningPlayer();
	if (!OwningPlayer || !IsInViewport())
	{
		return;
	}

	UWidget* FocusTarget = GetPreferredFocusTarget();
	if (const UCommonButtonBase* const PreferredButton = Cast<UCommonButtonBase>(FocusTarget))
	{
		if (!PreferredButton->GetIsFocusable())
		{
			FocusTarget = nullptr;
		}
	}
	if (!FocusTarget || !FocusTarget->IsVisible() || !FocusTarget->GetIsEnabled())
	{
		FocusTarget = CanFocusButton(Button_Back) ? static_cast<UWidget*>(Button_Back) : this;
	}
	FocusTarget->SetUserFocus(OwningPlayer);
}

UWidget* UKnightMenuWidgetBase::GetPreferredFocusTarget() const
{
	return Button_Back;
}

bool UKnightMenuWidgetBase::CanFocusButton(const UCommonButtonBase* Button)
{
	return Button && Button->GetIsFocusable() && Button->IsVisible() && Button->GetIsEnabled();
}

void UKnightMenuWidgetBase::SetTravelControlsEnabled(bool bEnabled)
{
	if (Button_Back)
	{
		Button_Back->SetIsEnabled(bEnabled && Button_Back->IsVisible());
	}
}

bool UKnightMenuWidgetBase::ActivatePage(UPanelWidget* Page)
{
	if (!Switcher_Pages || !Page || Page->GetParent() != Switcher_Pages || !Switcher_Pages->HasChild(Page))
	{
		UE_LOG(Logknight, Error, TEXT("%s has a missing page or a page outside Switcher_Pages."), *GetClass()->GetName());
		SetStatusText(LOCTEXT("InvalidPage", "화면 구성을 확인할 수 없습니다."));
		return false;
	}
	Switcher_Pages->SetActiveWidget(Page);
	return true;
}

void UKnightMenuWidgetBase::UpdateBackVisibility(bool bOnHome)
{
	if (Button_Back)
	{
		Button_Back->SetVisibility(bOnHome ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		Button_Back->SetIsEnabled(!bOnHome && !IsTravelPending());
	}
}

void UKnightMenuWidgetBase::ValidateRequiredWidget(const UWidget* Widget, const TCHAR* WidgetName) const
{
	if (!Widget)
	{
		UE_LOG(Logknight, Error, TEXT("%s is missing required BindWidget: %s."), *GetClass()->GetName(), WidgetName);
	}
}

void UKnightMenuWidgetBase::SetStatusText(const FText& Message)
{
	if (Text_Status)
	{
		Text_Status->SetText(Message);
	}
}

void UKnightMenuWidgetBase::ClearStatus()
{
	SetStatusText(FText::GetEmpty());
}

bool UKnightMenuWidgetBase::IsTravelPending() const
{
	const UKnightGameFlowSubsystem* const Flow = GetFlow();
	return Flow && Flow->IsTravelPending();
}

UKnightGameFlowSubsystem* UKnightMenuWidgetBase::GetFlow() const
{
	if (UKnightGameFlowSubsystem* const Flow = BoundFlow.Get())
	{
		return Flow;
	}
	UWorld* const World = GetWorld();
	UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UKnightGameFlowSubsystem>() : nullptr;
}

void UKnightMenuWidgetBase::RefreshTravelControls()
{
	SetTravelControlsEnabled(!IsTravelPending());
}

void UKnightMenuWidgetBase::SubscribeFlow()
{
	UKnightGameFlowSubsystem* const Flow = GetFlow();
	if (!Flow)
	{
		return;
	}
	if (BoundFlow.IsValid() && BoundFlow.Get() != Flow)
	{
		UnsubscribeFlow();
	}
	BoundFlow = Flow;
	Flow->OnFlowStateChanged.AddUniqueDynamic(this, &ThisClass::HandleFlowStateChanged);
	Flow->OnTravelRejected.AddUniqueDynamic(this, &ThisClass::HandleTravelRejected);
	Flow->OnTravelFailed.AddUniqueDynamic(this, &ThisClass::HandleTravelFailed);
	Flow->OnTravelPendingChanged.AddUniqueDynamic(this, &ThisClass::HandleTravelPendingChanged);
}

void UKnightMenuWidgetBase::UnsubscribeFlow()
{
	if (UKnightGameFlowSubsystem* const Flow = BoundFlow.Get())
	{
		Flow->OnFlowStateChanged.RemoveDynamic(this, &ThisClass::HandleFlowStateChanged);
		Flow->OnTravelRejected.RemoveDynamic(this, &ThisClass::HandleTravelRejected);
		Flow->OnTravelFailed.RemoveDynamic(this, &ThisClass::HandleTravelFailed);
		Flow->OnTravelPendingChanged.RemoveDynamic(this, &ThisClass::HandleTravelPendingChanged);
	}
	BoundFlow.Reset();
}

void UKnightMenuWidgetBase::HandleFlowStateChanged(EKnightFlowState PreviousState, EKnightFlowState NewState)
{
	RefreshTravelControls();
	if (NewState != GetExpectedFlowState())
	{
		SetTravelControlsEnabled(false);
	}
}

void UKnightMenuWidgetBase::HandleTravelRejected(EKnightFlowState RequestedState, FText Reason)
{
	UE_LOG(Logknight, Warning, TEXT("Menu UI travel request to %d rejected: %s"), static_cast<int32>(RequestedState), *Reason.ToString());
	RefreshTravelControls();
	SetStatusText(IsTravelPending() ? LOCTEXT("StillTraveling", "메뉴로 이동 중입니다.")
		: LOCTEXT("RequestRejected", "지금 메뉴를 열 수 없습니다."));
}

void UKnightMenuWidgetBase::HandleTravelFailed(EKnightFlowState RequestedState, FText Reason)
{
	UE_LOG(Logknight, Error, TEXT("Menu UI travel to %d failed: %s"), static_cast<int32>(RequestedState), *Reason.ToString());
	RefreshTravelControls();
	SetStatusText(LOCTEXT("TravelFailed", "메뉴를 열지 못했습니다. 다시 시도해 주세요."));
}

void UKnightMenuWidgetBase::HandleTravelPendingChanged(bool bIsPending)
{
	RefreshTravelControls();
	if (bIsPending)
	{
		SetStatusText(LOCTEXT("TravelPending", "메뉴로 이동 중입니다."));
	}
}

bool UKnightMenuWidgetBase::IsInsideSwitcher(const UWidget* Widget) const
{
	const UPanelWidget* Parent = Widget ? Widget->GetParent() : nullptr;
	while (Parent)
	{
		if (Parent == Switcher_Pages)
		{
			return true;
		}
		Parent = Parent->GetParent();
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
