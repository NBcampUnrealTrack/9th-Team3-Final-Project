#pragma once

#include "CoreMinimal.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SlateWrapperTypes.h"
#include "Slate/WidgetTransform.h"
#include "KnightMaintenanceTransition.generated.h"

class UKnightMaintenanceWidget;
class UCanvasPanel;
class URetainerBox;
class UImage;
class UMaterialInterface;
class UMaterialInstanceDynamic;

USTRUCT(BlueprintType)
struct FKnightMaintenanceEntranceSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, meta=(ClampMin="0.1", ClampMax="1.0")) float Duration = .38f;
	UPROPERTY(EditAnywhere, meta=(ClampMin="1.0", ClampMax="5.0")) float EaseOutExponent = 3.f;
	UPROPERTY(EditAnywhere, meta=(ClampMin="0.05", ClampMax="0.8")) float FadeInFraction = .28f;
	UPROPERTY(EditAnywhere, meta=(ClampMin="4.0")) float RevealSoftness = 28.f;
	UPROPERTY(EditAnywhere, meta=(ClampMin="0.5")) float EdgeWidth = 2.f;
	UPROPERTY(EditAnywhere, meta=(ClampMin="4.0")) float GlowWidth = 24.f;
	UPROPERTY(EditAnywhere, meta=(ClampMin="0.0")) float GlowIntensity = .8f;
	UPROPERTY(EditAnywhere, meta=(ClampMin="0.0")) float TrailStrength = .45f;
	UPROPERTY(EditAnywhere, meta=(ClampMin="0.04", ClampMax="0.4")) float ExitDuration = .12f;
};

USTRUCT()
struct FKnightMovingPanelLayer
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UWidget> Widget;
	UPROPERTY() TObjectPtr<UCanvasPanel> Parent;
	UPROPERTY() TObjectPtr<URetainerBox> Retainer;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Material;
	FAnchorData Layout;
	int32 Z = 0;
	bool bAutoSize = false;
	bool bRight = false;
	FVector2D Origin = FVector2D::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;
	float ParentScale = 1.f;
};

USTRUCT()
struct FKnightFadingCenterLayer
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UWidget> Widget;
	float Opacity = 1.f;
};

/** Temporary rendering treatment. Original canvas layouts/child transforms remain authoritative. */
UCLASS()
class KNIGHT_API UKnightMaintenanceTransition : public UObject
{
	GENERATED_BODY()
public:
	void Begin(UKnightMaintenanceWidget* InOwner, UWidget* InFooter, FSimpleDelegate Finished);
	void BeginExit(UKnightMaintenanceWidget* InOwner, UWidget* InFooter, FSimpleDelegate Finished);
	void Tick(float DeltaTime);
	void Cancel();
	bool IsActive() const { return Phase != EPhase::Idle; }
	bool IsMoving() const { return Phase == EPhase::Entering; }
	bool IsExiting() const { return Phase == EPhase::Exiting; }
	float GetProgress() const { return Progress; }
	FVector2D GetPanelOffset() const { return PanelOffset; }
	int32 GetRetainerCount() const
	{
		int32 Count = 0;
		for (const FKnightMovingPanelLayer& Layer : Layers) if (Layer.Retainer) ++Count;
		return Count;
	}
private:
	enum class EPhase : uint8 { Idle, Preparing, Entering, Exiting };
	bool PrepareLayers();
	void AddLayer(UWidget* Widget, bool bRight);
	void AddCenter(UWidget* Widget);
	void UpdateEntrance(float T);
	void Restore();
	void SaveOwner(UKnightMaintenanceWidget* InOwner, UWidget* InFooter);
	UPROPERTY() TObjectPtr<UKnightMaintenanceWidget> Owner;
	UPROPERTY() TObjectPtr<UWidget> Footer;
	UPROPERTY() TArray<FKnightMovingPanelLayer> Layers;
	UPROPERTY() TArray<FKnightFadingCenterLayer> Centers;
	UPROPERTY() TObjectPtr<UImage> LeftScan;
	UPROPERTY() TObjectPtr<UImage> RightScan;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> LeftScanMaterial;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> RightScanMaterial;
	FSimpleDelegate OnFinished;
	FKnightMaintenanceEntranceSettings Settings;
	EPhase Phase = EPhase::Idle;
	FBox2D LeftBounds, RightBounds;
	FVector2D RootSize = FVector2D::ZeroVector;
	FVector2D PanelOffset = FVector2D::ZeroVector;
	FWidgetTransform FooterTransform;
	float OriginalOpacity = 1.f, FooterOpacity = 1.f, ExitStartOpacity = 1.f, ExitFooterOpacity = 1.f;
	ESlateVisibility OriginalVisibility = ESlateVisibility::Visible, FooterVisibility = ESlateVisibility::Visible;
	float Elapsed = 0.f, Progress = 0.f;
	int32 PrepareFrames = 0;
};
