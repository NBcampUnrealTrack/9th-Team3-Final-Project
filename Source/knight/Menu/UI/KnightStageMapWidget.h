#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Sound/SlateSound.h"
#include "KnightStageMapWidget.generated.h"

class UCommonTextStyle;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/** One stage region on the map. */
USTRUCT(BlueprintType)
struct KNIGHT_API FKnightMapMarker
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="Knight|Map")
	FName StageId;

	UPROPERTY(BlueprintReadWrite, Category="Knight|Map")
	FText Label;

	/** Degrees: X = longitude, Y = latitude. */
	UPROPERTY(BlueprintReadWrite, Category="Knight|Map")
	FVector2D Location = FVector2D::ZeroVector;

	/** Radius of the painted region around Location, in degrees of latitude. */
	UPROPERTY(BlueprintReadWrite, Category="Knight|Map")
	float RegionRadius = 14.f;

	/** Drawn grey; still selectable so the panel can explain why it is locked. */
	UPROPERTY(BlueprintReadWrite, Category="Knight|Map")
	bool bLocked = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKnightMapStageClicked, FName, StageId);

/**
 * Stage-select map: a flat (equirectangular) dot map larger than the widget, panned by dragging and stopped at its edges.
 * Each stage is a soft-edged region around its location; clicking anywhere in a region selects the stage.
 *
 * The material paints the dots, grid and regions. This class paints the region names and marks, the sonar rings that
 * keep spreading from the selected stage, and a leader line from the selected mark to LeaderTarget (the info panel).
 *
 * Regions reach the material as vector parameters Region0 .. Region7 = (u, v, radius in map pixels, state),
 * state 0 = unused, 1 normal, 2 hovered, 3 selected, +3 when locked. Stages past the eighth have no painted region.
 * The edge wobble below must match the material so clicks land where the region is drawn.
 */
UCLASS()
class KNIGHT_API UKnightStageMapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UKnightStageMapWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category="Knight|Map")
	void SetMarkers(const TArray<FKnightMapMarker>& InMarkers);

	/** NAME_None clears the selection. Does not broadcast OnStageClicked. */
	UFUNCTION(BlueprintCallable, Category="Knight|Map")
	void SetSelectedStage(FName StageId);

	UFUNCTION(BlueprintPure, Category="Knight|Map")
	FName GetSelectedStage() const { return SelectedStageId; }

	/** Slides the map so the region sits as close to the middle as the map edges allow. */
	UFUNCTION(BlueprintCallable, Category="Knight|Map")
	void FocusStage(FName StageId);

	/** The selected mark draws a leader line to the nearest side of this widget. */
	void SetLeaderTarget(UWidget* InTarget) { LeaderTarget = InTarget; }

	UPROPERTY(BlueprintAssignable, Category="Knight|Map")
	FKnightMapStageClicked OnStageClicked;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	/** UI-domain material with vector parameters ViewRect (u, v, width, height of the visible part), MapPixels, Region0..7 and scalar Blink. */
	UPROPERTY(EditAnywhere, Category="Knight|Map")
	TObjectPtr<UMaterialInterface> MapMaterial;

	/** Degrees of latitude the widget shows top to bottom; sets the zoom. */
	UPROPERTY(EditAnywhere, Category="Knight|Map", meta=(ClampMin=20, ClampMax=150, Units="deg"))
	float VisibleLatitude = 110.f;

	/** The map stops this far from the equator; the poles are cut off. */
	UPROPERTY(EditAnywhere, Category="Knight|Map", meta=(ClampMin=30, ClampMax=90, Units="deg"))
	float LatitudeLimit = 75.f;

	/** Sonar ring around the selected stage: a new ring every PingInterval seconds, each living PingLifetime. */
	UPROPERTY(EditAnywhere, Category="Knight|Map", meta=(ClampMin=0.1))
	float PingInterval = 1.1f;

	UPROPERTY(EditAnywhere, Category="Knight|Map", meta=(ClampMin=0.1))
	float PingLifetime = 1.4f;

	/** How far a ring spreads before it fades out. Slate units. */
	UPROPERTY(EditAnywhere, Category="Knight|Map", meta=(ClampMin=0))
	float PingRadius = 220.f;

	UPROPERTY(EditAnywhere, Category="Knight|Map")
	TSubclassOf<UCommonTextStyle> LabelTextStyle;

	UPROPERTY(EditAnywhere, Category="Knight|Map")
	FSlateSound HoverSound;

	UPROPERTY(EditAnywhere, Category="Knight|Map")
	FSlateSound SelectSound;

private:
	/** The visible part of the map for a widget of this size. */
	struct FMapView
	{
		FVector2D Center = FVector2D::ZeroVector;
		FVector2D LocalSize = FVector2D::ZeroVector;
		float PixelsPerDegree = 1.f;
	};

	FMapView MakeView(const FVector2D& LocalSize) const;
	FVector2D ClampCenter(const FVector2D& InCenter, const FMapView& View) const;
	FVector2D ToLocal(const FVector2D& LocationDegrees, const FMapView& View) const;
	int32 FindMarkerAt(const FVector2D& LocalPosition, const FVector2D& LocalSize) const;
	void SetHovered(int32 Index);
	void ApplyToMaterial(const FVector2D& LocalSize);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MapMID;

	FSlateBrush MapBrush;
	TArray<FKnightMapMarker> Markers;
	TWeakObjectPtr<UWidget> LeaderTarget;
	FName SelectedStageId;
	int32 HoveredIndex = INDEX_NONE;

	/** Map centre in degrees (longitude, latitude), and its glide speed in degrees per second. */
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D Velocity = FVector2D::ZeroVector;
	bool bDragging = false;
	bool bDragMoved = false;
	bool bUserTouched = false;
	FVector2D DragLast = FVector2D::ZeroVector;
	FVector2D DragStart = FVector2D::ZeroVector;
	double DragLastTime = 0.0;

	bool bFocusing = false;
	FVector2D FocusCenter = FVector2D::ZeroVector;

	float Time = 0.f;
	/** Time when the current stage was selected; rings and the blink count from here. */
	float SelectTime = 0.f;
};
