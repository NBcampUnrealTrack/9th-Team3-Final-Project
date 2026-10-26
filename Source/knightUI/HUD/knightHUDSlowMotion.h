#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Containers/Ticker.h"
#include "knightHUDSlowMotion.generated.h"

class UImage;
class UknightVM_PlayerStatus;

/**
 * Screen effects for the slow-motion moments.
 *   Groggy presentation (the Knight breaks a target's poise): the HUD dims while it lasts
 *   (prediction slow, freeze, slow release). A short white flash marks the hit. No letterboxing.
 *   Knockdown (the Knight is knocked down): the screen edges flash white and fade out.
 * Sits under the rest of the HUD in WBP_HUDLayout and dims the HUD widgets drawn after it. Ticks only while an effect is showing.
 *
 * Widgets are found by name: Img_KnockdownEdge and Img_Flash (stretched over the screen).
 * Retired Img_Edge and Img_LetterboxTop/Bottom bindings only hide legacy WBP copies.
 */
UCLASS(Abstract, Blueprintable, meta=(DisableNativeTick))
class KNIGHTUI_API UknightHUDSlowMotion : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UImage> Img_Edge;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UImage> Img_KnockdownEdge;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UImage> Img_Flash;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UImage> Img_LetterboxTop;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UImage> Img_LetterboxBottom;

	/** Time for the HUD dimming to come in once the groggy presentation starts. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Groggy", meta=(ClampMin=0, Units="s"))
	float GroggyEnterTime = 0.25f;

	/** Time for the HUD dimming to leave after the presentation ends. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Groggy", meta=(ClampMin=0, Units="s"))
	float GroggyExitTime = 0.3f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Groggy", meta=(ClampMin=0, ClampMax=1))
	float GroggyEdgeOpacity = 0.7f; // Legacy asset compatibility only; Img_Edge stays collapsed.

	/** How far the other HUD widgets fade during groggy presentation (0.5 = half opacity). */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Groggy", meta=(ClampMin=0, ClampMax=1))
	float GroggyHUDDim = 0.5f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Groggy", meta=(ClampMin=0, ClampMax=1))
	float HitFlashOpacity = 0.55f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Groggy", meta=(ClampMin=0, Units="s"))
	float HitFlashTime = 0.1f;

	/** Matches the knockdown slow-motion (GA_Knight_HitReact KnockdownImpact, 0.25 real seconds). */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Knockdown", meta=(ClampMin=0, Units="s"))
	float KnockdownHoldTime = 0.25f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Knockdown", meta=(ClampMin=0, Units="s"))
	float KnockdownFadeTime = 0.3f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Knockdown", meta=(ClampMin=0, ClampMax=1))
	float KnockdownEdgeOpacity = 0.9f;

private:
	void HandleGroggyPresentation(bool bImpact);
	void HandleKnockdown();
	void StartTicker();
	bool Tick(float DeltaTime);
	void SetHUDOpacity(float Opacity);

	TWeakObjectPtr<UknightVM_PlayerStatus> ViewModel;
	FDelegateHandle GroggyHandle;
	FDelegateHandle KnockdownHandle;
	FTSTicker::FDelegateHandle Ticker;
	/** 0..1 HUD dimming envelope; it never changes the camera or viewport. */
	float GroggyWeight = 0.f;
	float Flash = 0.f;
	/** Real seconds since the knockdown started; negative when none is showing. */
	float KnockdownTime = -1.f;
	bool bHUDDimmed = false;
};
