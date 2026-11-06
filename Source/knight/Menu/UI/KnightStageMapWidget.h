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

	UPROPERTY(BlueprintReadWrite, Category="Knight|Map")
	FText Availability;

	UPROPERTY(BlueprintReadWrite, Category="Knight|Map")
	FText ClearHistory;
};

/** Quest pin placed relative to a painted map region, independently from the raid StageId. */
USTRUCT(BlueprintType)
struct KNIGHT_API FKnightMapQuestMarker
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="Knight|Map")
	FName QuestId;

	UPROPERTY(BlueprintReadWrite, Category="Knight|Map")
	FName RegionId;

	UPROPERTY(BlueprintReadWrite, Category="Knight|Map")
	FText Label;

	/** Absolute longitude/latitude, derived from the editorial offset and region centre. */
	UPROPERTY(BlueprintReadWrite, Category="Knight|Map")
	FVector2D Location = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category="Knight|Map")
	bool bLocked = false;

	UPROPERTY(BlueprintReadWrite, Category="Knight|Map")
	bool bCompleted = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKnightMapStageClicked, FName, StageId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKnightMapQuestClicked, FName, QuestId);

/**
 * Stage-select map: a flat (equirectangular) dot map larger than the widget, panned by dragging and stopped at its edges.
 * Each stage is a soft-edged region around its location; clicking anywhere in a region selects the stage.
 *
 * The material paints the dots, grid, regions and a continuous silhouette sweep. This class paints region names,
 * progression marks and a leader line from the selected mark to LeaderTarget (the info panel).
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

	UFUNCTION(BlueprintCallable, Category="Knight|Map")
	void SetQuestMarkers(const TArray<FKnightMapQuestMarker>& InMarkers);

	UFUNCTION(BlueprintCallable, Category="Knight|Map")
	void SetSelectedQuest(FName QuestId);
	bool CycleQuestMarker(int32 Direction);

	/** NAME_None clears the selection. Does not broadcast OnStageClicked. */
	UFUNCTION(BlueprintCallable, Category="Knight|Map")
	void SetSelectedStage(FName StageId);
	void RestoreSelection(FName StageId);
	void ResetView();
	void RestoreView(FVector2D InCenter, float Latitude);
	FVector2D GetViewCenter() const { return Center; }
	float GetVisibleLatitude() const { return VisibleLatitude; }

	UFUNCTION(BlueprintPure, Category="Knight|Map")
	FName GetSelectedStage() const { return SelectedStageId; }

	/** Slides the map so the region sits as close to the middle as the map edges allow. */
	UFUNCTION(BlueprintCallable, Category="Knight|Map")
	void FocusStage(FName StageId);

	/** The selected mark draws a leader line to the nearest side of this widget. */
	void SetLeaderTarget(UWidget* InTarget) { LeaderTarget = InTarget; }
	/** Presentation-only reveal; never changes map coordinates, selection or hit geometry. */
	void SetTransitionReveal(float Progress, float Intensity, float Width, float GlowWidth, bool bActive, bool bRetreating);

	UPROPERTY(BlueprintAssignable, Category="Knight|Map")
	FKnightMapStageClicked OnStageClicked;

	UPROPERTY(BlueprintAssignable, Category="Knight|Map")
	FKnightMapQuestClicked OnQuestClicked;

protected:
	friend class UknightOperationMapCheckCommandlet;
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	/** UI-domain material: ViewRect, MapPixels, Region0..7, RegionEffect0..7 and SelectionTuning. */
	UPROPERTY(EditAnywhere, Category="Knight|Map")
	TObjectPtr<UMaterialInterface> MapMaterial;

	/** Degrees of latitude the widget shows top to bottom; sets the zoom. */
	UPROPERTY(EditAnywhere, Category="Knight|Map", meta=(ClampMin=20, ClampMax=150, Units="deg"))
	float VisibleLatitude = 110.f;

	/** The map stops this far from the equator; the poles are cut off. */
	UPROPERTY(EditAnywhere, Category="Knight|Map", meta=(ClampMin=30, ClampMax=90, Units="deg"))
	float LatitudeLimit = 75.f;

	UPROPERTY(EditAnywhere, Category="Knight|Map|Selection", meta=(ClampMin="0.25", ClampMax="0.4"))
	float SweepDuration = .34f; // Legacy serialized one-shot tuning, retained for compatibility.
	UPROPERTY(EditAnywhere, Category="Knight|Map|Selection", meta=(ClampMin="2.5", ClampMax="3.5"))
	float LoopDuration = 3.f;
	UPROPERTY(EditAnywhere, Category="Knight|Map|Selection", meta=(ClampMin="0.05", ClampMax="0.3"))
	float SelectionFadeDuration = .12f;
	UPROPERTY(EditAnywhere, Category="Knight|Map|Selection", meta=(ClampMin="0.1", ClampMax="1"))
	float OutlineBrightness = .58f;
	UPROPERTY(EditAnywhere, Category="Knight|Map|Selection", meta=(ClampMin="1", ClampMax="20"))
	float SelectionGlowWidth = 7.f;
	UPROPERTY(EditAnywhere, Category="Knight|Map|Selection", meta=(ClampMin="0", ClampMax="0.4"))
	float InteriorOpacity = .13f;

	/** Legacy serialized sonar tuning, retained for asset compatibility; no longer used. */
	UPROPERTY(EditAnywhere, Category="Knight|Map", meta=(ClampMin=0.1))
	float PingInterval = 1.1f;

	UPROPERTY(EditAnywhere, Category="Knight|Map", meta=(ClampMin=0.1))
	float PingLifetime = 1.4f;

	/** Legacy serialized sonar radius; no longer used. */
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
	int32 FindQuestMarkerAt(const FVector2D& LocalPosition, const FVector2D& LocalSize) const;
	void SetHovered(int32 Index);
	void SetHoveredQuest(int32 Index);
	void ApplyToMaterial(const FVector2D& LocalSize);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MapMID;

	FSlateBrush MapBrush;
	TArray<FKnightMapMarker> Markers;
	TArray<FKnightMapQuestMarker> QuestMarkers;
	TWeakObjectPtr<UWidget> LeaderTarget;
	FName SelectedStageId;
	FName SelectedQuestId;
	int32 HoveredIndex = INDEX_NONE;
	int32 HoveredQuestIndex = INDEX_NONE;

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
	/** Session clock and selection origin. Hidden widgets do not advance their loop. */
	float SelectTime = 0.f;
	TMap<FName, float> SelectionWeights;
	TMap<FName, TArray<float>> ContourLengths;
	float GetContourAngle(FName RegionId, float Phase) const;
	float InitialVisibleLatitude = 110.f;
	float TransitionReveal = 1.f, TransitionScan = 0.f, TransitionWidth = 1.5f, TransitionGlow = 16.f;
	bool bTransitionReveal = false, bTransitionRetreating = false;
};
