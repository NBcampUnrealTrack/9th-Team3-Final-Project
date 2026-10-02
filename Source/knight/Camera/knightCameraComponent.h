#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "GameplayTagContainer.h"
#include "knightCameraComponent.generated.h"

class UknightCameraMode;
class UknightCameraFadeHandler;
class UknightCameraModeStack;
class UknightCameraOcclusionHandler;

/** Use CameraMode while the owner's ASC has RequiredTag. Rules are checked in order; the first match wins. */
USTRUCT(BlueprintType)
struct FknightCameraModeRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Camera", meta=(Categories="State"))
	FGameplayTag RequiredTag;

	UPROPERTY(EditAnywhere, Category="Camera")
	TSubclassOf<UknightCameraMode> CameraMode;
};

/**
 * Camera driven by a blended stack of camera modes.
 *
 * Each frame the mode is chosen as: an active temporary override (parry punch-in, finisher), else the first
 * rule whose tag the owner has (State.Flying -> Flight, State.LockedOn -> LockOn, State.Boosting -> Boost),
 * else DefaultCameraMode. Mode changes blend over the new mode's BlendTime.
 *
 * With no DefaultCameraMode set, it behaves like a plain camera component (the template spring arm setup),
 * so Blueprints keep working until they are configured.
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightCameraComponent : public UCameraComponent
{
	GENERATED_BODY()

public:
	UknightCameraComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView) override;

	/** Forces a mode for Duration seconds regardless of rules (parry, finishers, scripted beats). */
	UFUNCTION(BlueprintCallable, Category="Knight|Camera")
	void SetTemporaryCameraMode(TSubclassOf<UknightCameraMode> CameraMode, float Duration);

	UFUNCTION(BlueprintCallable, Category="Knight|Camera")
	void ClearTemporaryCameraMode();

	/** Mode currently on top of the stack, for debugging. */
	UknightCameraMode* GetActiveCameraMode() const;

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera")
	TSubclassOf<UknightCameraMode> DefaultCameraMode;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera", meta=(TitleProperty="RequiredTag"))
	TArray<FknightCameraModeRule> CameraModeRules;

	UPROPERTY(EditDefaultsOnly, Instanced, Category="Knight|Camera")
	TObjectPtr<UknightCameraOcclusionHandler> OcclusionHandler;

	/** Fades or hides monsters and tagged props between the camera and the Knight. */
	UPROPERTY(EditDefaultsOnly, Instanced, Category="Knight|Camera")
	TObjectPtr<UknightCameraFadeHandler> FadeHandler;

private:
	TSubclassOf<UknightCameraMode> DetermineCameraMode() const;

	UPROPERTY(Transient)
	TObjectPtr<UknightCameraModeStack> ModeStack;

	UPROPERTY(Transient)
	TSubclassOf<UknightCameraMode> TemporaryMode;

	double TemporaryModeEndTime = 0.0;
};
