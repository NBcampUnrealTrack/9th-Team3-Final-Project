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
 * (2026-10-29) 렌치가 차오르는 대신 사각형 배경이 아래부터 GaugeColor(남색)로 차오른다(M_UI_HUDWrenchGauge: 배경 = 투명 사각형 + 흰 렌치,
 * 채움 = 렌치 자리가 뚫린 사각형). 키트 개수와 상관없이 늘 게이지만큼 차고, 다 차면 키트 +1과 함께 비워진다.
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
