#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Containers/Ticker.h"
#include "FieldNotificationId.h"
#include "knightHUDGroggyNoise.generated.h"

class UImage;
class UknightVM_PlayerStatus;

/**
 * Screen noise over the whole HUD while the Knight is groggy: a tiled grain image jumping to a new spot every frame and a
 * few horizontal scan bars at random heights, like a cockpit feed losing signal. Ticks only while the Knight is groggy.
 *
 * Widgets are found by name: Img_Noise (larger than the screen, tiled) and Img_Scan0 .. Img_Scan5 (any that exist).
 */
UCLASS(Abstract, Blueprintable, meta=(DisableNativeTick))
class KNIGHTUI_API UknightHUDGroggyNoise : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UImage> Img_Noise;

	/** Strongest opacity, right as the Knight goes groggy; it settles to HoldOpacity after BurstTime. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD", meta=(ClampMin=0, ClampMax=1))
	float BurstOpacity = 0.6f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD", meta=(ClampMin=0, ClampMax=1))
	float HoldOpacity = 0.3f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD", meta=(ClampMin=0, Units="s"))
	float BurstTime = 0.4f;

	/** Seconds between jumps of the grain; frames in between keep the same spot. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD", meta=(ClampMin=0, Units="s"))
	float JumpInterval = 0.04f;

private:
	void HandleViewModelChanged(UObject* InViewModel, UE::FieldNotification::FFieldId FieldId);
	void SetActive(bool bActive);
	bool Tick(float DeltaTime);

	TArray<TObjectPtr<UImage>> ScanBars;
	TWeakObjectPtr<UknightVM_PlayerStatus> ViewModel;
	FTSTicker::FDelegateHandle Ticker;
	float ActiveTime = 0.f;
	float NextJump = 0.f;
};
