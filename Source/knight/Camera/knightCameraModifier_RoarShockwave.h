#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "knightCameraModifier_RoarShockwave.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;

/**
 * Screen ripple for monster roars: rings of distortion spread from the roaring monster on screen, with
 * chromatic aberration, line jitter and a light camera shake.
 *
 * While a roar window is open (Knight Roar Shockwave notify state) a new ring is sent every PulseInterval, each
 * one PulseDecay times weaker than the last, so a longer roar simply sends more rings. A low jitter and shake
 * stay on between rings and fade out after the window closes.
 *
 * Everything scales with distance to the monster and with the player's CameraShakeScale setting
 * (0 turns the whole effect off).
 */
UCLASS()
class KNIGHT_API UknightCameraModifier_RoarShockwave : public UCameraModifier
{
	GENERATED_BODY()

public:
	UknightCameraModifier_RoarShockwave();

	/** Opens a roar window. Source (usually the monster's mesh) and Socket give the point the rings start from. */
	void StartRoar(USceneComponent* Source, FName Socket);

	/** Closes the window opened by Source. Rings already sent finish on their own. */
	void StopRoar(const USceneComponent* Source);

	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
	virtual void ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings) override;

protected:
	/** Post process material reading the parameters below (M_PP_RoarShockwave). */
	UPROPERTY(EditDefaultsOnly, Category="Roar")
	TSoftObjectPtr<UMaterialInterface> Material;

	/** Seconds between rings while the roar window is open. */
	UPROPERTY(EditDefaultsOnly, Category="Roar|Pulse", meta=(ClampMin=0.05))
	float PulseInterval = 0.4f;

	/** Strength of each ring relative to the previous one. */
	UPROPERTY(EditDefaultsOnly, Category="Roar|Pulse", meta=(ClampMin=0, ClampMax=1))
	float PulseDecay = 0.8f;

	/** Seconds a ring lives. */
	UPROPERTY(EditDefaultsOnly, Category="Roar|Pulse", meta=(ClampMin=0.1))
	float PulseLifetime = 0.9f;

	/** Ring speed in screen heights per second. */
	UPROPERTY(EditDefaultsOnly, Category="Roar|Look", meta=(ClampMin=0))
	float RingSpeed = 1.1f;

	/** Ring half thickness in screen heights. */
	UPROPERTY(EditDefaultsOnly, Category="Roar|Look", meta=(ClampMin=0.01))
	float RingWidth = 0.09f;

	/** How far a ring pushes the image, in screen heights. */
	UPROPERTY(EditDefaultsOnly, Category="Roar|Look", meta=(ClampMin=0))
	float Distortion = 0.018f;

	UPROPERTY(EditDefaultsOnly, Category="Roar|Look", meta=(ClampMin=0))
	float ChromaticAberration = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category="Roar|Look", meta=(ClampMin=0))
	float Jitter = 0.5f;

	/** Camera shake strength. 1 = about 0.6 degrees. */
	UPROPERTY(EditDefaultsOnly, Category="Roar|Look", meta=(ClampMin=0))
	float Shake = 0.5f;

	/** Full strength up to this distance (m). */
	UPROPERTY(EditDefaultsOnly, Category="Roar|Distance", meta=(ClampMin=0))
	float FullStrengthDistance = 10.f;

	/** Strength reaches MinDistanceScale at this distance (m) and stays there beyond it. */
	UPROPERTY(EditDefaultsOnly, Category="Roar|Distance", meta=(ClampMin=0))
	float MinStrengthDistance = 60.f;

	UPROPERTY(EditDefaultsOnly, Category="Roar|Distance", meta=(ClampMin=0, ClampMax=1))
	float MinDistanceScale = 0.25f;

	/** Seconds the in-between jitter and shake take to fade after the roar window closes. */
	UPROPERTY(EditDefaultsOnly, Category="Roar|Pulse", meta=(ClampMin=0.01))
	float FadeOutTime = 0.3f;

private:
	/** The material holds six ring slots. */
	static constexpr int32 MaxPulses = 6;

	struct FPulse
	{
		float Age = 0.f;
		float Strength = 1.f;
	};

	void SendPulse();
	bool IsVisible() const;
	float GetStrengthScale(const FMinimalViewInfo& POV) const;
	FVector2D ProjectToScreen(const FMinimalViewInfo& POV, const FVector& WorldLocation, float AspectRatio) const;
	float GetAspectRatio() const;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MaterialInstance;

	TWeakObjectPtr<USceneComponent> RoarSource;
	FName RoarSocket;
	FVector LastSourceLocation = FVector::ZeroVector;
	TArray<FPulse> Pulses;
	bool bRoaring = false;
	float SinceLastPulse = 0.f;
	int32 PulsesSent = 0;
	/** 1 while roaring, then fades to 0 over FadeOutTime. */
	float Envelope = 0.f;
	float ShakeTime = 0.f;
	/** Distance and setting scale of the current frame. */
	float CurrentScale = 0.f;
	int32 NoiseSeed = 0;
};
