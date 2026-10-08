#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FieldNotificationId.h"
#include "knightHUDRaidTimer.generated.h"

class UCommonTextBlock;
class UknightVM_Raid;

/**
 * Top-right raid timer of the HUD. Reads the Raid view model; hidden when the raid has no time limit.
 * Turns to the warning color for the last WarningSeconds. Widgets are found by name: Txt_Time, Txt_Caption.
 */
UCLASS(Abstract, Blueprintable, meta=(DisableNativeTick))
class KNIGHTUI_API UknightHUDRaidTimer : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** "mm:ss" */
	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_Time;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_Caption;

	UPROPERTY(EditAnywhere, Category="Knight|HUD", meta=(ClampMin=0, Units="s"))
	float WarningSeconds = 60.f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Colors")
	FLinearColor NormalColor = FLinearColor(FColor(0xF4, 0xF8, 0xFC));

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Colors")
	FLinearColor WarningColor = FLinearColor(FColor(0xFF, 0x7A, 0x1A));

private:
	void HandleViewModelChanged(UObject* InViewModel, UE::FieldNotification::FFieldId FieldId);
	void Refresh();

	TWeakObjectPtr<UknightVM_Raid> ViewModel;
};
