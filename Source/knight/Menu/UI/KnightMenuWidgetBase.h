#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFlow/KnightFlowTypes.h"
#include "KnightMenuWidgetBase.generated.h"

class UButton;
class UPanelWidget;
class UTextBlock;
class UWidgetSwitcher;
class UKnightGameFlowSubsystem;

/** Shared root-menu behavior. The two WBP roots provide layout and named widgets. */
UCLASS(Abstract)
class KNIGHT_API UKnightMenuWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Focuses a visible control, or the focusable root when none is available. */
	void FocusCurrentPage();

	/** Used by both Button_Back and Escape; returns false on Home or during travel. */
	UFUNCTION(BlueprintCallable, Category="Knight|Menu")
	bool RequestBack();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** These common controls belong outside Switcher_Pages in each root WBP. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UWidgetSwitcher> Switcher_Pages;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Back;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_Status;

	virtual EKnightFlowState GetExpectedFlowState() const { return EKnightFlowState::Uninitialized; }
	virtual bool HandleBackRequested() { return false; }
	virtual UWidget* GetPreferredFocusTarget() const;
	virtual void SetTravelControlsEnabled(bool bEnabled);

	bool ActivatePage(UPanelWidget* Page);
	void UpdateBackVisibility(bool bOnHome);
	void ValidateRequiredWidget(const UWidget* Widget, const TCHAR* WidgetName) const;
	void SetStatusText(const FText& Message);
	void ClearStatus();
	bool IsTravelPending() const;
	UKnightGameFlowSubsystem* GetFlow() const;
	void RefreshTravelControls();

private:
	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleFlowStateChanged(EKnightFlowState PreviousState, EKnightFlowState NewState);

	UFUNCTION()
	void HandleTravelRejected(EKnightFlowState RequestedState, FText Reason);

	UFUNCTION()
	void HandleTravelFailed(EKnightFlowState RequestedState, FText Reason);

	UFUNCTION()
	void HandleTravelPendingChanged(bool bIsPending);

	void SubscribeFlow();
	void UnsubscribeFlow();
	bool IsInsideSwitcher(const UWidget* Widget) const;

	TWeakObjectPtr<UKnightGameFlowSubsystem> BoundFlow;
};
