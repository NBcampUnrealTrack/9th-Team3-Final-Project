#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Screens/knightConfirmModal.h"
#include "knightAsyncAction_ShowConfirm.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FknightConfirmAsyncPin);

/** Blueprint node "Show Confirm Modal": one exec pin per answer. Cancelled also fires when the modal cannot be shown. */
UCLASS()
class KNIGHTUI_API UknightAsyncAction_ShowConfirm : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Empty button texts keep the modal's defaults (확인 / 취소). */
	UFUNCTION(BlueprintCallable, Category="Knight|UI", meta=(BlueprintInternalUseOnly="true", DisplayName="Show Confirm Modal"))
	static UknightAsyncAction_ShowConfirm* ShowConfirmModal(APlayerController* OwningPlayer, FText Title, FText Message, FText ConfirmText, FText CancelText);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FknightConfirmAsyncPin Confirmed;

	UPROPERTY(BlueprintAssignable)
	FknightConfirmAsyncPin Cancelled;

private:
	void HandleResult(EknightConfirmResult Result);

	TWeakObjectPtr<APlayerController> OwningPlayer;
	FknightConfirmRequest Request;
};
