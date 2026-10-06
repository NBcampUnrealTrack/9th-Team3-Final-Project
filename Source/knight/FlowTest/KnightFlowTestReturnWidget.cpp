#include "KnightFlowTestReturnWidget.h"

#include "GameFlow/KnightGameFlowSubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "InputCoreTypes.h"
#include "knightPlayerController.h"
#include "Raid/knightRaidGameMode.h"
#include "knight.h"

#define LOCTEXT_NAMESPACE "KnightFlowTestReturn"

void UKnightFlowTestReturnWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	if (!Button_ReturnToMainMenu || !Text_StageName || !Text_Status)
	{
		UE_LOG(Logknight, Error, TEXT("%s requires Button_ReturnToMainMenu (UButton), Text_StageName and Text_Status (UTextBlock)."), *GetClass()->GetName());
	}
	if (Button_ReturnToMainMenu)
	{
		Button_ReturnToMainMenu->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleReturnClicked);
	}
}

void UKnightFlowTestReturnWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BoundFlow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>() : nullptr;
	if (UKnightGameFlowSubsystem* const Flow = BoundFlow.Get())
	{
		Flow->OnTravelPendingChanged.AddUniqueDynamic(this, &ThisClass::HandlePendingChanged);
		Flow->OnTravelRejected.AddUniqueDynamic(this, &ThisClass::HandleTravelRejected);
		Flow->OnTravelFailed.AddUniqueDynamic(this, &ThisClass::HandleTravelFailed);
	}
	RefreshFromFlow();
}

void UKnightFlowTestReturnWidget::NativeDestruct()
{
	if (UKnightGameFlowSubsystem* const Flow = BoundFlow.Get())
	{
		Flow->OnTravelPendingChanged.RemoveDynamic(this, &ThisClass::HandlePendingChanged);
		Flow->OnTravelRejected.RemoveDynamic(this, &ThisClass::HandleTravelRejected);
		Flow->OnTravelFailed.RemoveDynamic(this, &ThisClass::HandleTravelFailed);
	}
	BoundFlow.Reset();
	Super::NativeDestruct();
}

FReply UKnightFlowTestReturnWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (AknightPlayerController* const RaidController = Cast<AknightPlayerController>(GetOwningPlayer()))
		{
			RaidController->CloseRaidReturnMenu();
			return FReply::Handled();
		}
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UKnightFlowTestReturnWidget::FocusReturnButton()
{
	if (!IsInViewport() || !GetOwningPlayer())
	{
		return;
	}
	UWidget* const Target = Button_ReturnToMainMenu && Button_ReturnToMainMenu->GetIsFocusable()
		&& Button_ReturnToMainMenu->IsVisible() && Button_ReturnToMainMenu->GetIsEnabled()
		? static_cast<UWidget*>(Button_ReturnToMainMenu.Get()) : this;
	Target->SetUserFocus(GetOwningPlayer());
}

void UKnightFlowTestReturnWidget::RefreshFromFlow()
{
	UKnightGameFlowSubsystem* const Flow = BoundFlow.Get();
	if (!Flow)
	{
		if (Button_ReturnToMainMenu) Button_ReturnToMainMenu->SetIsEnabled(false);
		if (Text_Status) Text_Status->SetText(LOCTEXT("FlowUnavailable", "메뉴 복귀 기능을 사용할 수 없습니다."));
		UE_LOG(Logknight, Error, TEXT("Flow test return widget cannot find KnightGameFlowSubsystem."));
		return;
	}
	const FName ActiveId = Flow->GetActiveStageId();
	if (Text_StageName)
	{
		FKnightStageDefinition Stage;
		Text_StageName->SetText(ActiveId.IsNone() ? LOCTEXT("DirectPlay", "직접 실행한 테스트 맵 (활성 스테이지 없음)")
			: Flow->FindStageDefinition(ActiveId, Stage) ? Stage.DisplayName : FText::FromName(ActiveId));
	}
	if (Button_ReturnToMainMenu)
	{
		Button_ReturnToMainMenu->SetIsEnabled(Flow->GetCurrentFlowState() == EKnightFlowState::Gameplay && !Flow->IsTravelPending());
	}
	if (Text_Status)
	{
		Text_Status->SetText(Flow->IsTravelPending() ? LOCTEXT("ReturnPending", "메인메뉴로 이동 중입니다.") : FText::GetEmpty());
	}
}

void UKnightFlowTestReturnWidget::HandleReturnClicked()
{
	UKnightGameFlowSubsystem* const Flow = BoundFlow.Get();
	if (!Flow || Flow->IsTravelPending() || Flow->GetCurrentFlowState() != EKnightFlowState::Gameplay)
	{
		return;
	}
	bool bAccepted = false;
	if (AknightRaidGameMode* const Raid = GetWorld() ? GetWorld()->GetAuthGameMode<AknightRaidGameMode>() : nullptr)
	{
		if (APlayerController* const Controller = GetOwningPlayer()) Controller->SetPause(false);
		Raid->RequestReturnToLobby();
		bAccepted = Flow->IsTravelPending();
	}
	else
	{
		bAccepted = Flow->RequestOpenMainMenu();
	}
	if (Button_ReturnToMainMenu) Button_ReturnToMainMenu->SetIsEnabled(Flow->GetCurrentFlowState() == EKnightFlowState::Gameplay && !Flow->IsTravelPending());
	if (bAccepted && Flow->IsTravelPending() && Text_Status)
	{
		Text_Status->SetText(LOCTEXT("ReturnPending", "메인메뉴로 이동 중입니다."));
	}
}

void UKnightFlowTestReturnWidget::HandlePendingChanged(bool bIsPending)
{
	if (Button_ReturnToMainMenu) Button_ReturnToMainMenu->SetIsEnabled(BoundFlow.IsValid()
		&& BoundFlow->GetCurrentFlowState() == EKnightFlowState::Gameplay && !bIsPending);
	if (bIsPending && Text_Status) Text_Status->SetText(LOCTEXT("ReturnPending", "메인메뉴로 이동 중입니다."));
}

void UKnightFlowTestReturnWidget::HandleTravelRejected(EKnightFlowState RequestedState, FText Reason)
{
	if (RequestedState != EKnightFlowState::MainMenu) return;
	UE_LOG(Logknight, Warning, TEXT("Flow test return rejected: %s"), *Reason.ToString());
	if (Button_ReturnToMainMenu) Button_ReturnToMainMenu->SetIsEnabled(BoundFlow.IsValid()
		&& BoundFlow->GetCurrentFlowState() == EKnightFlowState::Gameplay && !BoundFlow->IsTravelPending());
	if (Text_Status) Text_Status->SetText(BoundFlow.IsValid() && BoundFlow->IsTravelPending()
		? LOCTEXT("ReturnPending", "메인메뉴로 이동 중입니다.") : LOCTEXT("ReturnRejected", "지금 메인메뉴로 돌아갈 수 없습니다."));
}

void UKnightFlowTestReturnWidget::HandleTravelFailed(EKnightFlowState RequestedState, FText Reason)
{
	if (RequestedState != EKnightFlowState::MainMenu) return;
	UE_LOG(Logknight, Error, TEXT("Flow test return failed: %s"), *Reason.ToString());
	if (Button_ReturnToMainMenu) Button_ReturnToMainMenu->SetIsEnabled(BoundFlow.IsValid()
		&& BoundFlow->GetCurrentFlowState() == EKnightFlowState::Gameplay && !BoundFlow->IsTravelPending());
	if (Text_Status) Text_Status->SetText(LOCTEXT("ReturnFailed", "메인메뉴로 돌아가지 못했습니다. 다시 시도해 주세요."));
}

#undef LOCTEXT_NAMESPACE
