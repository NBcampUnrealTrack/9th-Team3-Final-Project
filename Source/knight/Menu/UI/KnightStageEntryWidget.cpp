#include "KnightStageEntryWidget.h"

#include "GameFlow/KnightGameFlowSubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "knight.h"

void UKnightStageEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!Button_SelectStage || !Text_StageName)
	{
		UE_LOG(Logknight, Error, TEXT("%s requires Button_SelectStage (UButton) and Text_StageName (UTextBlock)."), *GetClass()->GetName());
	}
	if (Button_SelectStage)
	{
		Button_SelectStage->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSelectClicked);
	}
	RefreshDisplay();
}

void UKnightStageEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BoundFlow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>() : nullptr;
	if (UKnightGameFlowSubsystem* const Flow = BoundFlow.Get())
	{
		Flow->OnTravelPendingChanged.AddUniqueDynamic(this, &ThisClass::HandleTravelPendingChanged);
	}
	RefreshDisplay();
	SetSelectionEnabled(BoundFlow.IsValid() && !BoundFlow->IsTravelPending());
}

void UKnightStageEntryWidget::NativeDestruct()
{
	if (UKnightGameFlowSubsystem* const Flow = BoundFlow.Get())
	{
		Flow->OnTravelPendingChanged.RemoveDynamic(this, &ThisClass::HandleTravelPendingChanged);
	}
	BoundFlow.Reset();
	Super::NativeDestruct();
}

void UKnightStageEntryWidget::SetStage(FName InStageId, const FText& InDisplayName)
{
	StageId = InStageId;
	DisplayName = InDisplayName;
	RefreshDisplay();
	SetSelectionEnabled(BoundFlow.IsValid() && !BoundFlow->IsTravelPending());
}

void UKnightStageEntryWidget::SetSelectionEnabled(bool bEnabled)
{
	if (Button_SelectStage)
	{
		Button_SelectStage->SetIsEnabled(bEnabled && !StageId.IsNone());
	}
}

void UKnightStageEntryWidget::RefreshDisplay()
{
	if (Text_StageName)
	{
		Text_StageName->SetText(DisplayName);
	}
}

void UKnightStageEntryWidget::HandleSelectClicked()
{
	if (!StageId.IsNone() && BoundFlow.IsValid() && !BoundFlow->IsTravelPending())
	{
		OnStageSelected.Broadcast(StageId);
	}
}

void UKnightStageEntryWidget::HandleTravelPendingChanged(bool bIsPending)
{
	SetSelectionEnabled(!bIsPending);
}
