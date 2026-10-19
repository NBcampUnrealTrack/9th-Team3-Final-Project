#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Sound/SlateSound.h"
#include "KnightStageGlobeWidget.generated.h"

class UCommonTextStyle;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/** One stage region on the globe. */
USTRUCT(BlueprintType)
struct KNIGHT_API FKnightGlobeMarker
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="Knight|Globe")
	FName StageId;

	UPROPERTY(BlueprintReadWrite, Category="Knight|Globe")
	FText Label;

	/** Degrees: X = longitude, Y = latitude. */
	UPROPERTY(BlueprintReadWrite, Category="Knight|Globe")
	FVector2D Location = FVector2D::ZeroVector;

	/** Angular radius of the painted region around Location, in degrees. */
	UPROPERTY(BlueprintReadWrite, Category="Knight|Globe")
	float RegionRadius = 14.f;

	/** Drawn dimmed; still selectable so the panel can explain why it is locked. */
	UPROPERTY(BlueprintReadWrite, Category="Knight|Globe")
	bool bLocked = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKnightGlobeStageClicked, FName, StageId);

/**
 * Stage-select globe: a sphere drawn by a UI material, turned by dragging. Each stage is a region of land around its
 * location, painted by the material in light blue with relief shading; clicking anywhere in a region selects the stage.
 *
 * Everything is painted here (no child widgets): the material quad, the region names, and a leader line from the
 * selected region to LeaderTarget (the info panel). The widget math and the material must agree on the rotation:
 * the material undoes Pitch (around X) and then Yaw (around Y) to find the surface point under each pixel.
 *
 * Regions reach the material as vector parameters Region0 .. Region7 = (longitude, latitude, radius in radians, state),
 * state 0 = unused, 1 normal, 2 hovered, 3 selected, +3 when locked. Stages past the eighth have no painted region.
 */
UCLASS()
class KNIGHT_API UKnightStageGlobeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UKnightStageGlobeWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category="Knight|Globe")
	void SetMarkers(const TArray<FKnightGlobeMarker>& InMarkers);

	/** NAME_None clears the selection. Does not broadcast OnStageClicked. */
	UFUNCTION(BlueprintCallable, Category="Knight|Globe")
	void SetSelectedStage(FName StageId);

	UFUNCTION(BlueprintPure, Category="Knight|Globe")
	FName GetSelectedStage() const { return SelectedStageId; }

	/** Turns the globe so the region faces the camera. */
	UFUNCTION(BlueprintCallable, Category="Knight|Globe")
	void FocusStage(FName StageId);

	/** Position in this widget's local space; false when the stage is unknown. bFront is false behind the globe. */
	bool GetMarkerLocalPosition(FName StageId, FVector2D& OutPosition, bool& bOutFront) const;

	/** The selected marker draws a leader line to the nearest side of this widget. */
	void SetLeaderTarget(UWidget* InTarget) { LeaderTarget = InTarget; }

	UPROPERTY(BlueprintAssignable, Category="Knight|Globe")
	FKnightGlobeStageClicked OnStageClicked;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	/** UI-domain material with scalar parameters Yaw, Pitch (radians), Radius (sphere radius / half the quad) and Region0..7. */
	UPROPERTY(EditAnywhere, Category="Knight|Globe")
	TObjectPtr<UMaterialInterface> GlobeMaterial;

	/** Must match the Radius the marker math uses; sent to the material so the two never disagree. */
	UPROPERTY(EditAnywhere, Category="Knight|Globe", meta=(ClampMin=0.5, ClampMax=1))
	float SphereRadius = 0.86f;

	UPROPERTY(EditAnywhere, Category="Knight|Globe", meta=(ClampMin=0))
	float DragDegreesPerPixel = 0.25f;

	UPROPERTY(EditAnywhere, Category="Knight|Globe", meta=(ClampMin=0, ClampMax=89))
	float MaxPitchDegrees = 60.f;

	/** Slow spin until the player first touches the globe or a region is selected. */
	UPROPERTY(EditAnywhere, Category="Knight|Globe", meta=(ClampMin=0))
	float IdleSpinDegreesPerSecond = 4.f;

	UPROPERTY(EditAnywhere, Category="Knight|Globe")
	TSubclassOf<UCommonTextStyle> LabelTextStyle;

	UPROPERTY(EditAnywhere, Category="Knight|Globe")
	FSlateSound HoverSound;

	UPROPERTY(EditAnywhere, Category="Knight|Globe")
	FSlateSound SelectSound;

private:
	struct FProjectedMarker
	{
		FVector2D Position;
		/** View-space depth: 1 = facing the camera, below 0 = behind the globe. */
		float Depth = 0.f;
	};

	FProjectedMarker Project(const FVector2D& LocationDegrees, const FVector2D& LocalSize) const;
	float GetPixelRadius(const FVector2D& LocalSize) const;
	int32 FindMarkerAt(const FVector2D& LocalPosition, const FVector2D& LocalSize) const;
	/** Unit vector of the globe surface under a local position, in the material's axes; false off the sphere. */
	bool LocalToSurface(const FVector2D& LocalPosition, const FVector2D& LocalSize, FVector& OutDirection) const;
	void SetHovered(int32 Index);
	void ApplyRotationToMaterial();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GlobeMID;

	FSlateBrush GlobeBrush;
	TArray<FKnightGlobeMarker> Markers;
	TWeakObjectPtr<UWidget> LeaderTarget;
	FName SelectedStageId;
	int32 HoveredIndex = INDEX_NONE;

	/** Radians. */
	float Yaw = 0.f;
	float Pitch = 0.f;
	float YawVelocity = 0.f;
	float PitchVelocity = 0.f;
	bool bDragging = false;
	bool bDragMoved = false;
	bool bUserTouched = false;
	FVector2D DragLast = FVector2D::ZeroVector;
	FVector2D DragStart = FVector2D::ZeroVector;
	double DragLastTime = 0.0;

	bool bFocusing = false;
	float FocusYaw = 0.f;
	float FocusPitch = 0.f;

	float Time = 0.f;
};
