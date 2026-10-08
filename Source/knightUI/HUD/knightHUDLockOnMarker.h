#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Containers/Ticker.h"
#include "FieldNotificationId.h"
#include "knightHUDLockOnMarker.generated.h"

class UCommonTextBlock;
class USoundBase;
class UknightTargetLockComponent;
class UknightVM_PlayerStatus;

/**
 * Lock-on marker: corner box on the locked target, a leader line branching up and out from the box, and a name plate
 * at the end of the line showing only the target's name.
 *
 * Fills the HUD; the pieces are positioned every frame from the target's projected screen position, so they must be
 * direct children of the root Canvas Panel: Box, Line_Diagonal, Line_Shelf, Plate (and Txt_TargetName anywhere).
 * The branch flips to the left when the target is on the right side of the screen. Ticks only while locked.
 */
UCLASS(Abstract, Blueprintable, meta=(DisableNativeTick))
class KNIGHTUI_API UknightHUDLockOnMarker : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Box;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Line_Diagonal;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Line_Shelf;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Plate;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_TargetName;

	UPROPERTY(EditAnywhere, Category="Knight|HUD")
	TObjectPtr<USoundBase> LockOnSound;

	/** Side of the corner box, in slate units at 1080p. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout", meta=(ClampMin=8))
	float BoxSize = 64.f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout", meta=(ClampMin=0))
	float DiagonalLength = 48.f;

	/** Angle of the diagonal above the horizontal. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout", meta=(ClampMin=0, ClampMax=90, Units="deg"))
	float DiagonalAngle = 60.f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout", meta=(ClampMin=0))
	float ShelfLength = 150.f;

	/** Gap between the line and the plate. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout")
	FVector2D PlateOffset = FVector2D(8.f, -4.f);

	/** The branch goes left when the target is right of this fraction of the screen width. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout", meta=(ClampMin=0, ClampMax=1))
	float FlipScreenFraction = 0.62f;

	/** Keeps the plate inside the screen edges (1080p safe zone is 4.5%, about 86 x 49). */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout")
	FVector2D SafeMargin = FVector2D(86.f, 49.f);

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Motion", meta=(ClampMin=0, Units="s"))
	float BoxInTime = 0.16f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Motion", meta=(ClampMin=0, Units="s"))
	float LineDrawTime = 0.14f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Motion", meta=(ClampMin=0, Units="s"))
	float PlateInTime = 0.16f;

	/** Box scale at the start of the lock, shrinking to 1. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Motion", meta=(ClampMin=1))
	float BoxStartScale = 1.6f;

private:
	void HandleViewModelChanged(UObject* InViewModel, UE::FieldNotification::FFieldId FieldId);
	void RefreshLockState();
	bool TickMarker(float DeltaTime);
	void StopTicking();
	bool UpdateLayout();
	void ApplyAnimation();
	UknightTargetLockComponent* FindTargetLock() const;

	TWeakObjectPtr<UknightVM_PlayerStatus> ViewModel;
	TWeakObjectPtr<AActor> ShownTarget;
	FTSTicker::FDelegateHandle Ticker;
	float AnimTime = 0.f;
	bool bFlipped = false;
};
