#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/Object.h"
#include "knightCameraFadeHandler.generated.h"

class APlayerController;
class UMaterialParameterCollection;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class EknightCameraFadeMode : uint8
{
	/**
	 * Writes an opacity (FadedOpacity..1) to each occluding primitive's custom primitive data under
	 * FadeParameterName. The material turns it into a dithered opacity mask. Needs material support;
	 * shadows and collision are unaffected.
	 */
	DitherFade,
	/**
	 * Removes occluding primitives from this player's view only (APlayerController::HiddenPrimitiveComponents).
	 * Works with any material, but pops instead of fading.
	 */
	HideFromView
};

/**
 * Fades or hides what stands between the camera and the Knight: monsters (ITargetableInterface actors)
 * and anything tagged FadeTag. The occlusion handler keeps the camera out of walls; this one handles the
 * things the camera is allowed to pass through or behind.
 *
 * Optionally writes CameraLocation / PivotLocation / OcclusionAmount to a material parameter collection,
 * so materials can cut a soft hole along the camera-to-Knight line instead of fading a whole giant monster.
 */
UCLASS(EditInlineNew, DefaultToInstanced)
class KNIGHT_API UknightCameraFadeHandler : public UObject
{
	GENERATED_BODY()

public:
	void UpdateFade(const AActor* ViewOwner, const FVector& PivotLocation, const FVector& CameraLocation, float DeltaTime);

	/** Restores every faded or hidden primitive (camera removed, view target changed). */
	void RestoreAll();

protected:
	UPROPERTY(EditAnywhere, Category="Fade")
	EknightCameraFadeMode FadeMode = EknightCameraFadeMode::DitherFade;

	/** Fade monsters and other lock-on targets. */
	UPROPERTY(EditAnywhere, Category="Fade")
	bool bFadeTargetableActors = true;

	/** Actors or components with this tag also fade (large props, trees). */
	UPROPERTY(EditAnywhere, Category="Fade")
	FName FadeTag = TEXT("CameraFade");

	UPROPERTY(EditAnywhere, Category="Fade")
	TArray<TEnumAsByte<EObjectTypeQuery>> FadeObjectTypes;

	/** Radius of the sweep from the camera to the Knight. */
	UPROPERTY(EditAnywhere, Category="Fade", meta=(ClampMin=1, Units="cm"))
	float ProbeRadius = 40.f;

	/** Anything this close to the camera fades too, which covers the camera ending up inside a mesh. */
	UPROPERTY(EditAnywhere, Category="Fade", meta=(ClampMin=0, Units="cm"))
	float CameraProximityRadius = 150.f;

	/** Opacity of a fully faded primitive (DitherFade). */
	UPROPERTY(EditAnywhere, Category="Fade|Dither", meta=(ClampMin=0, ClampMax=1))
	float FadedOpacity = 0.25f;

	/** Custom primitive data parameter name the materials read. */
	UPROPERTY(EditAnywhere, Category="Fade|Dither")
	FName FadeParameterName = TEXT("CameraFadeOpacity");

	UPROPERTY(EditAnywhere, Category="Fade|Dither", meta=(ClampMin=0))
	float FadeOutSpeed = 10.f;

	UPROPERTY(EditAnywhere, Category="Fade|Dither", meta=(ClampMin=0))
	float FadeInSpeed = 4.f;

	/** Optional. Receives vectors CameraLocation, PivotLocation and scalar OcclusionAmount (0..1). */
	UPROPERTY(EditAnywhere, Category="Fade|Material Collection")
	TObjectPtr<UMaterialParameterCollection> OcclusionParameterCollection;

public:
	UknightCameraFadeHandler();

private:
	void GatherOccluders(const AActor* ViewOwner, const FVector& PivotLocation, const FVector& CameraLocation,
		TSet<TWeakObjectPtr<UPrimitiveComponent>>& OutOccluders) const;
	bool ShouldFade(const AActor* Actor, const UPrimitiveComponent* Component, const AActor* ViewOwner) const;
	void ApplyOpacity(UPrimitiveComponent* Component, float Opacity) const;
	void SetHiddenFromView(UPrimitiveComponent* Component, bool bHidden);
	void UpdateParameterCollection(const AActor* ViewOwner, const FVector& PivotLocation, const FVector& CameraLocation, float DeltaTime);

	/** Current opacity per primitive this handler has touched. */
	TMap<TWeakObjectPtr<UPrimitiveComponent>, float> FadedComponents;
	TWeakObjectPtr<APlayerController> ViewController;
	float OcclusionAmount = 0.f;
	bool bAnyOccluder = false;
};
