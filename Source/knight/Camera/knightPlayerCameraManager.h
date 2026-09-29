#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "knightPlayerCameraManager.generated.h"

class UCameraShakeBase;

/**
 * Global camera concerns only: pitch limits, combat shakes, and cinematic view targets (Sequencer camera
 * cuts go through the manager's view target as usual). Per-frame framing lives in UknightCameraComponent.
 */
UCLASS()
class KNIGHT_API AknightPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	AknightPlayerCameraManager();

	/** Plays HitShake scaled by Intensity (0..1 typical). Used by hit, guard and parry feedback. */
	UFUNCTION(BlueprintCallable, Category="Knight|Camera")
	void PlayHitShake(float Intensity = 1.f);

	/** Plays HeavyImpactShake (monster slams, roars) scaled by Intensity. */
	UFUNCTION(BlueprintCallable, Category="Knight|Camera")
	void PlayHeavyImpactShake(float Intensity = 1.f);

protected:
	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera")
	TSubclassOf<UCameraShakeBase> HitShake;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera")
	TSubclassOf<UCameraShakeBase> HeavyImpactShake;
};
