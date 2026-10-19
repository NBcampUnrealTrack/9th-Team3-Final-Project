#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FieldNotificationId.h"
#include "knightHUDFrameStatus.generated.h"

class UImage;
class UProgressBar;
class UknightVM_PlayerStatus;

/**
 * Blueprint of the Knight at the bottom left of the HUD: one line drawing per part (head, torso, arms, legs) whose color
 * shows the part's health, and a silhouette that fills from the feet up as the Knight's poise is taken (full = groggy).
 *
 * Widgets are found by name: Img_PartHead, Img_PartTorso, Img_PartArms, Img_PartLegs and Bar_Groggy (a bottom-to-top
 * progress bar whose fill image is the silhouette, so the fill is clipped instead of stretched).
 */
UCLASS(Abstract, Blueprintable, meta=(DisableNativeTick))
class KNIGHTUI_API UknightHUDFrameStatus : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UImage> Img_PartHead;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UImage> Img_PartTorso;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UImage> Img_PartArms;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UImage> Img_PartLegs;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> Bar_Groggy;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Colors")
	FLinearColor NormalColor = FLinearColor(FColor(0x7F, 0xB8, 0xE0));

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Colors")
	FLinearColor DamagedColor = FLinearColor(FColor(0xFF, 0x9A, 0x2A));

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Colors")
	FLinearColor BrokenColor = FLinearColor(FColor(0xFF, 0x3B, 0x30));

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Colors")
	FLinearColor GroggyColor = FLinearColor(FColor(0xF2, 0xC2, 0x30, 0x8C));

	/** Part health fraction at or below which its outline turns DamagedColor (0 is BrokenColor). */
	UPROPERTY(EditAnywhere, Category="Knight|HUD", meta=(ClampMin=0, ClampMax=1))
	float DamagedThreshold = 0.3f;

private:
	void HandleViewModelChanged(UObject* InViewModel, UE::FieldNotification::FFieldId FieldId);
	void Refresh();
	FLinearColor GetPartColor(float HealthPercent) const;

	TWeakObjectPtr<UknightVM_PlayerStatus> ViewModel;
};
