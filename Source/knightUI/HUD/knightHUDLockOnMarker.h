#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/EngineBaseTypes.h"
#include "FieldNotificationId.h"
#include "Monster/knightMonsterPartComponent.h"
#include "knightHUDLockOnMarker.generated.h"

class UCommonTextBlock;
class USoundBase;
class UknightTargetLockComponent;
class UknightVM_PlayerStatus;

/**
 * Lock-on marker: corner box on the locked target, a leader line branching up and out from the box, and a name plate
 * at the end of the line showing only the target's name.
 *
 * When the lock is on a monster part, a part frame around the box shows the part's condition by color (red, dark red
 * blinking when critical, gray when broken, neutral when unbreakable) and flashes when the condition changes.
 *
 * Below the box the distance to the target is shown in meters.
 *
 * Fills the HUD; the pieces are positioned every frame from the target's projected screen position, so they must be
 * direct children of the root Canvas Panel: Box, PartFrame, PartFlash, Line_Diagonal, Line_Shelf, Plate,
 * Txt_Distance (and Txt_TargetName / Txt_PartName anywhere). The branch flips to the left when the target is on the right side of the
 * screen. Ticks only while locked, right after the world tick (cameras updated, before Slate paints) so the marker
 * lands on the target in the same frame instead of one frame behind.
 */
UCLASS(Abstract, Blueprintable, meta=(DisableNativeTick))
class KNIGHTUI_API UknightHUDLockOnMarker : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 이번 프레임 이름판 자리(슬레이트 단위). 이름판 아래 모서리(뒤집히지 않았으면 왼쪽, 뒤집혔으면 오른쪽)와 방향을 준다.
	 * 다른 HUD가 이름판 바로 아래에 붙어 그릴 때 쓴다. 잠겨 있지 않거나 대상이 화면 뒤면 false.
	 */
	bool GetPlateAnchor(const AActor*& OutTarget, FVector2D& OutPlateCorner, bool& bOutFlipped) const;

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

	/** Tinted by the locked part's condition. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> PartFrame;

	/** White copy of the frame, shown briefly when the condition changes. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> PartFlash;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_PartName;

	/** Under the box: "[ 42M ]" */
	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_Distance;

	UPROPERTY(EditAnywhere, Category="Knight|HUD")
	TObjectPtr<USoundBase> LockOnSound;

	/** Played when the locked part turns critical or breaks. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD")
	TObjectPtr<USoundBase> PartConditionSound;

	/** 화면 부위 틀을 보인다. 부위 상태는 몬스터 몸 위의 부위 셸이 보여 주므로 기본은 끈다(부위 이름은 그대로). */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Part")
	bool bShowPartFrame = false;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Part")
	FLinearColor NormalColor = FLinearColor(FColor(0xFF, 0x3B, 0x30));

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Part")
	FLinearColor CriticalColor = FLinearColor(FColor(0x9E, 0x1B, 0x1B));

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Part")
	FLinearColor BrokenColor = FLinearColor(FColor(0x7A, 0x7F, 0x87));

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Part")
	FLinearColor UnbreakableColor = FLinearColor(FColor(0xF4, 0xF8, 0xFC));

	/** One full fade out and back in while the part is critical. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Part", meta=(ClampMin=0.1, Units="s"))
	float CriticalBlinkPeriod = 1.4f;

	/** Lowest frame opacity in the critical blink. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Part", meta=(ClampMin=0, ClampMax=1))
	float CriticalBlinkMinOpacity = 0.25f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Part", meta=(ClampMin=0, Units="s"))
	float FlashTime = 0.35f;

	/** Flash scale at the moment of the change, shrinking to 1. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Part", meta=(ClampMin=1))
	float FlashStartScale = 1.5f;

	/** Distance from the box centre down to the top of the distance readout (clears the 104 part frame). Below the box, because the leader line leaves upward. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD|Layout", meta=(ClampMin=0))
	float DistanceOffset = 58.f;

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
	void RefreshPart();
	void HandleLockedPartConditionChanged(EknightPartCondition NewCondition);
	void TickMarker(UWorld* World, ELevelTick TickType, float DeltaSeconds);
	void StopTicking();
	bool UpdateLayout();
	void ApplyAnimation();
	UknightTargetLockComponent* FindTargetLock() const;

	TWeakObjectPtr<UknightVM_PlayerStatus> ViewModel;
	TWeakObjectPtr<AActor> ShownTarget;
	FName ShownSocket = NAME_None;
	FVector LastWorldLocation = FVector::ZeroVector;
	FVector PartTransitionOffset = FVector::ZeroVector;
	bool bHasLastWorldLocation = false;
	FDelegateHandle Ticker;
	FDelegateHandle PartConditionHandle;
	float AnimTime = 0.f;
	/** Time since the last condition flash; starts past the end so nothing flashes on lock. */
	float FlashElapsed = TNumericLimits<float>::Max();
	int32 ShownDistance = -1;
	bool bFlipped = false;
	bool bPlateAnchorValid = false;
	FVector2D PlateAnchor = FVector2D::ZeroVector;
};
