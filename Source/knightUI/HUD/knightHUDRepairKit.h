#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FieldNotificationId.h"
#include "knightHUDRepairKit.generated.h"

class UCommonTextBlock;
class UProgressBar;
class UknightVM_PlayerStatus;

/**
 * Repair kits next to the mech frame (the Knight's groggy gauge): a large wrench in a faint frame with the number of kits on
 * its bottom-right corner. At 0 kits the whole widget dims.
 * (2026-10-28) The wrench fills from the bottom with the energy (0..100): perfect guards add 10, the convert key (E) spends 50 on a kit.
 * Before, the wrench stayed full while any kit was left and only showed the charge toward the next automatic kit at 0 kits.
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
