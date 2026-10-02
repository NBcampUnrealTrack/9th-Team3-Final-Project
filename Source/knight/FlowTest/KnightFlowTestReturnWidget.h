#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "KnightFlowTestReturnWidget.generated.h"

class UButton;
class UTextBlock;
class UKnightGameFlowSubsystem;
enum class EKnightFlowState : uint8;

/** Small independent return UI; it does not require the menu's WidgetSwitcher. */
UCLASS()
class KNIGHT_API UKnightFlowTestReturnWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void FocusReturnButton();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_ReturnToMainMenu;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_StageName;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_Status;

private:
	UFUNCTION()
	void HandleReturnClicked();

	UFUNCTION()
	void HandlePendingChanged(bool bIsPending);

	UFUNCTION()
	void HandleTravelRejected(EKnightFlowState RequestedState, FText Reason);

	UFUNCTION()
	void HandleTravelFailed(EKnightFlowState RequestedState, FText Reason);

	void RefreshFromFlow();
	TWeakObjectPtr<UKnightGameFlowSubsystem> BoundFlow;
};
