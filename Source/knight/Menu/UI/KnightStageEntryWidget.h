#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "KnightStageEntryWidget.generated.h"

class UButton;
class UTextBlock;
class UKnightGameFlowSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKnightStageEntrySelected, FName, StageId);

/** C++ parent for one generated stage-list entry. */
UCLASS()
class KNIGHT_API UKnightStageEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetStage(FName InStageId, const FText& InDisplayName);
	void SetSelectionEnabled(bool bEnabled);
	UButton* GetSelectionButton() const { return Button_SelectStage; }

	UPROPERTY(BlueprintAssignable, Category="Knight|Stage")
	FKnightStageEntrySelected OnStageSelected;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_SelectStage;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_StageName;

private:
	UFUNCTION()
	void HandleSelectClicked();

	UFUNCTION()
	void HandleTravelPendingChanged(bool bIsPending);

	void RefreshDisplay();

	FName StageId;
	FText DisplayName;
	TWeakObjectPtr<UKnightGameFlowSubsystem> BoundFlow;
};
