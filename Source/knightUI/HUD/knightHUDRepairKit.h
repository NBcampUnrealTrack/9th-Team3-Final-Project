#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FieldNotificationId.h"
#include "knightHUDRepairKit.generated.h"

class UCommonTextBlock;
class UProgressBar;
class UknightVM_PlayerStatus;

/**
 * Repair kits at the bottom right of the HUD: a wrench in a faint frame with the number of kits beside it. The wrench fills
 * from the bottom as perfect guards charge the next kit; the charge hides while the kits are full (it cannot grow then),
 * and the whole widget dims at 0 kits.
 *
 * Widgets are found by name: Bar_RepairGauge (bottom-to-top bar, fill image = the wrench) and Txt_RepairKitCount.
 */
UCLASS(Abstract, Blueprintable, meta=(DisableNativeTick))
class KNIGHTUI_API UknightHUDRepairKit : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> Bar_RepairGauge;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_RepairKitCount;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Colors")
	FLinearColor GaugeColor = FLinearColor(FColor(0x5F, 0xD0, 0xFF));

	/** Opacity of the whole widget with no kits left. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD", meta=(ClampMin=0, ClampMax=1))
	float EmptyOpacity = 0.35f;

private:
	void HandleViewModelChanged(UObject* InViewModel, UE::FieldNotification::FFieldId FieldId);
	void Refresh();

	TWeakObjectPtr<UknightVM_PlayerStatus> ViewModel;
};
