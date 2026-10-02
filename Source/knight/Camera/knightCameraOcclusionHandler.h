#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/Object.h"
#include "knightCameraOcclusionHandler.generated.h"

/**
 * Keeps the camera out of walls: sweeps from the pivot to the desired camera location and pulls the
 * camera in to the first blocking hit, easing back out once clear.
 *
 * Lock-on targets (ITargetableInterface actors, i.e. monsters) are ignored. Pulling the camera in front
 * of a giant monster's leg every time it walks past would be worse than seeing through it;
 * UknightCameraFadeHandler fades or hides the monster instead.
 */
UCLASS(EditInlineNew, DefaultToInstanced)
class KNIGHT_API UknightCameraOcclusionHandler : public UObject
{
	GENERATED_BODY()

public:
	/** Returns the camera location after occlusion. */
	FVector ResolveCameraLocation(const AActor* ViewOwner, const FVector& Pivot, const FVector& DesiredLocation, float DeltaTime);

protected:
	UPROPERTY(EditAnywhere, Category="Occlusion", meta=(ClampMin=1, Units="cm"))
	float ProbeRadius = 20.f;

	UPROPERTY(EditAnywhere, Category="Occlusion")
	TEnumAsByte<ECollisionChannel> ProbeChannel = ECC_Camera;

	/** Speed of easing back out after the obstruction clears. Pulling in is always instant. */
	UPROPERTY(EditAnywhere, Category="Occlusion", meta=(ClampMin=0))
	float RecoverySpeed = 6.f;

	UPROPERTY(EditAnywhere, Category="Occlusion")
	bool bIgnoreTargetableActors = true;

private:
	/** 0..1 fraction of the pivot-to-camera distance currently allowed. */
	float CurrentFraction = 1.f;
};
