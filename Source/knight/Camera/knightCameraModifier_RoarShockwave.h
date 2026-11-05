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
 * A roar can also make the whole screen wiggle: a slow, wavy warp that is strongest around the monster, rises
 * when the window opens and fades out after it closes (SetRoarStyle, set per notify). The same window can sink
 * the colors toward TintColor (enrage).
 *
 * Bursts are one-off shockwaves from a world point (a big landing, a part breaking): one fast ring with a short
 * radial blur toward that point. They do not need a roar window.
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

	/**
	 * Changes the look of the window Source just opened. StartRoar resets it to rings only (bPulses, no wiggle, no tint).
	 * Wiggle multiplies WiggleAmount; 0 turns the wiggle off. Tint (0..1) is how far the colors sink toward TintColor.
	 */
	void SetRoarStyle(const USceneComponent* Source, bool bPulses, float Wiggle, float Tint = 0.f, const FLinearColor& TintColor = FLinearColor::Red);

	/** Sends one shockwave from WorldLocation. Strength 1 = the Burst defaults; a stronger burst replaces a weaker one still running. */
	void AddBurst(const FVector& WorldLocation, float Strength);

	/** The modifier on Camera, added if it is not there yet. */
	static UknightCameraModifier_RoarShockwave* FindOrAdd(APlayerCameraManager& Camera);

	/** AddBurst on every local player's camera. */
	static void AddBurstForLocalPlayers(const UWorld* World, const FVector& WorldLocation, float Strength);

	/** Assets loaded on first use. The camera manager loads them ahead so the first roar does not hitch. */
	void GatherPreloadAssets(TArray<FSoftObjectPath>& OutPaths) const;

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

	/** How far the wiggle pushes the image at full strength, in screen heights. */
	UPROPERTY(EditDefaultsOnly, Category="Roar|Wiggle", meta=(ClampMin=0))
	float WiggleAmount = 0.015f;

	/** Waves per screen height. Lower is a slower, heavier swell. */
	UPROPERTY(EditDefaultsOnly, Category="Roar|Wiggle", meta=(ClampMin=0.1))
	float WiggleFrequency = 1.6f;

	/** How fast the waves move (radians per second). */
	UPROPERTY(EditDefaultsOnly, Category="Roar|Wiggle", meta=(ClampMin=0))
	float WiggleSpeed = 3.5f;

	/** Seconds the wiggle takes to rise when the window opens. */
	UPROPERTY(EditDefaultsOnly, Category="Roar|Wiggle", meta=(ClampMin=0.01))
	float WiggleRiseTime = 0.25f;

	/** Seconds the wiggle takes to fade after the window closes. */
	UPROPERTY(EditDefaultsOnly, Category="Roar|Wiggle", meta=(ClampMin=0.01))
	float WiggleFadeTime = 0.8f;

	/** Seconds the color sink takes to rise when the window opens. */
	UPROPERTY(EditDefaultsOnly, Category="Roar|Tint", meta=(ClampMin=0.01))
	float TintRiseTime = 0.3f;

	/** Seconds the color sink takes to fade after the window closes. */
	UPROPERTY(EditDefaultsOnly, Category="Roar|Tint", meta=(ClampMin=0.01))
	float TintFadeTime = 1.2f;

	/** Burst ring speed in screen heights per second. */
	UPROPERTY(EditDefaultsOnly, Category="Burst", meta=(ClampMin=0))
	float BurstSpeed = 2.4f;

	/** Seconds a burst lives. */
	UPROPERTY(EditDefaultsOnly, Category="Burst", meta=(ClampMin=0.05))
	float BurstLifetime = 0.45f;

	/** Burst ring half thickness in screen heights. */
	UPROPERTY(EditDefaultsOnly, Category="Burst", meta=(ClampMin=0.01))
	float BurstWidth = 0.08f;

	/** How far a burst ring pushes the image, in screen heights. */
	UPROPERTY(EditDefaultsOnly, Category="Burst", meta=(ClampMin=0))
	float BurstDistortion = 0.03f;

	/** Radial blur toward the burst point at its start (0 = off). */
	UPROPERTY(EditDefaultsOnly, Category="Burst", meta=(ClampMin=0))
	float BurstBlur = 1.f;

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
	float GetStrengthScale(const FMinimalViewInfo& POV, const FVector& WorldLocation) const;
	FVector2D ProjectToScreen(const FMinimalViewInfo& POV, const FVector& WorldLocation, float AspectRatio) const;
	float GetAspectRatio() const;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MaterialInstance;

	TWeakObjectPtr<USceneComponent> RoarSource;
	FName RoarSocket;
	FVector LastSourceLocation = FVector::ZeroVector;
	TArray<FPulse> Pulses;
	bool bRoaring = false;
	bool bPulsing = true;
	/** Wiggle strength of the current window (kept after it closes so the fade uses it). */
	float RoarWiggle = 0.f;
	/** 0..1, rises over WiggleRiseTime while roaring and falls over WiggleFadeTime after. */
	float WiggleLevel = 0.f;
	float WiggleTime = 0.f;
	/** Tint of the current window and its 0..1 level (same rise/fade idea as the wiggle). */
	float RoarTint = 0.f;
	FLinearColor RoarTintColor = FLinearColor::Red;
	float TintLevel = 0.f;
	/** The one running burst. Age < 0 means none. */
	FVector BurstLocation = FVector::ZeroVector;
	float BurstAge = -1.f;
	float BurstStrength = 0.f;
	float SinceLastPulse = 0.f;
	int32 PulsesSent = 0;
	/** 1 while roaring, then fades to 0 over FadeOutTime. */
	float Envelope = 0.f;
	float ShakeTime = 0.f;
	/** Distance and setting scale of the current frame. */
	float CurrentScale = 0.f;
	int32 NoiseSeed = 0;
};
