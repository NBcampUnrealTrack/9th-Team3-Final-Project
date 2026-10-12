#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "knightPlayerCameraManager.generated.h"

class UCameraShakeBase;
class UknightCameraModifier_RoarShockwave;

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
	virtual void UpdateCamera(float DeltaTime) override;

	/** Applies the player's CameraShakeScale setting to every shake. */
	virtual UCameraShakeBase* StartCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass, const FAddCameraShakeParams& Params) override;
	using Super::StartCameraShake;

	/** Plays HitShake scaled by Intensity (0..1 typical). Used by hit, guard and parry feedback. */
	UFUNCTION(BlueprintCallable, Category="Knight|Camera")
	void PlayHitShake(float Intensity = 1.f);

	/** Plays HeavyImpactShake (monster slams, roars) scaled by Intensity. */
	UFUNCTION(BlueprintCallable, Category="Knight|Camera")
	void PlayHeavyImpactShake(float Intensity = 1.f);

	/** Starts the roar screen ripple from Source (a monster mesh) at Socket. See UknightCameraModifier_RoarShockwave. */
	void StartRoarShockwave(USceneComponent* Source, FName Socket);

	/** Stops sending new ripple rings for Source. */
	void StopRoarShockwave(const USceneComponent* Source);

	/** The player's CameraShakeScale setting. Screen effects that move or distort the view scale with it too. */
	float GetShakeScale() const;

protected:
	virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override;
	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera")
	TSubclassOf<UCameraShakeBase> HitShake;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera")
	TSubclassOf<UCameraShakeBase> HeavyImpactShake;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera")
	TSubclassOf<UknightCameraModifier_RoarShockwave> RoarShockwaveClass;
};
