#include "Screens/knightAsyncAction_ShowConfirm.h"

#include "GameFramework/PlayerController.h"

UknightAsyncAction_ShowConfirm* UknightAsyncAction_ShowConfirm::ShowConfirmModal(APlayerController* OwningPlayer, FText Title, FText Message, FText ConfirmText, FText CancelText)
{
	UknightAsyncAction_ShowConfirm* const Action = NewObject<UknightAsyncAction_ShowConfirm>();
	Action->OwningPlayer = OwningPlayer;
	Action->Request.Title = Title;
	Action->Request.Message = Message;
	Action->Request.ConfirmText = ConfirmText;
	Action->Request.CancelText = CancelText;
	// Keeps the action alive until the player answers.
	Action->RegisterWithGameInstance(OwningPlayer);
	return Action;
}

void UknightAsyncAction_ShowConfirm::Activate()
{
	if (!UknightConfirmModal::Show(OwningPlayer.Get(), Request, FknightConfirmResultDelegate::CreateUObject(this, &ThisClass::HandleResult)))
	{
		HandleResult(EknightConfirmResult::Cancelled);
	}
}

void UknightAsyncAction_ShowConfirm::HandleResult(EknightConfirmResult Result)
{
	if (Result == EknightConfirmResult::Confirmed)
	{
		Confirmed.Broadcast();
	}
	else
	{
		Cancelled.Broadcast();
	}
	SetReadyToDestroy();
}
