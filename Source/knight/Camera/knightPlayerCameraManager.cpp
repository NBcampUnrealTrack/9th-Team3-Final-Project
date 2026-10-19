#include "Camera/knightPlayerCameraManager.h"

#include "Camera/CameraModifier_CameraShake.h"
#include "Camera/CameraShakeBase.h"
#include "Engine/Scene.h"
#include "HAL/PlatformTime.h"
#include "Camera/knightCameraModifier_RoarShockwave.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Settings/knightSettingsSubsystem.h"
#include "Settings/knightSharedSettings.h"

AknightPlayerCameraManager::AknightPlayerCameraManager()
{
	ViewPitchMin = -70.f;
	ViewPitchMax = 60.f;
	RoarShockwaveClass = UknightCameraModifier_RoarShockwave::StaticClass();
}

void AknightPlayerCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	Super::UpdateViewTarget(OutVT, DeltaTime);
	const ULocalPlayer* const LocalPlayer = PCOwner ? PCOwner->GetLocalPlayer() : nullptr;
	UknightSettingsSubsystem* const Settings = LocalPlayer ? LocalPlayer->GetSubsystem<UknightSettingsSubsystem>() : nullptr;
	if (Settings)
	{
		float Offset, Brightness, Gamma;
		Settings->GetEffectiveDisplaySettings(Offset, Brightness, Gamma);
		if (!FMath::IsNearlyZero(Offset))
		{
			// Apply after the active camera mode or menu camera has produced its designed FOV.
			OutVT.POV.FOV = FMath::Clamp(OutVT.POV.FOV + Offset, 60.f, 110.f);
		}
	}

	if (bKnockdownZoomActive)
	{
		if (!KnockdownViewTarget.IsValid() || OutVT.Target != KnockdownViewTarget.Get())
		{
			CancelKnockdownZoom();
			return;
		}
		const double Now = FPlatformTime::Seconds();
		if (bKnockdownZoomReturning && Now - KnockdownZoomReturnStartTime >= KnockdownZoomReturn)
		{
			CancelKnockdownZoom();
			return;
		}
		// OutVT contains this frame's camera mode and user FOV; never subtract from last frame's zoomed FOV.
		const float BaseFOV = OutVT.POV.FOV;
		OutVT.POV.FOV = FMath::Clamp(BaseFOV - KnockdownZoomDegrees * GetKnockdownZoomWeight(Now), FMath::Min(BaseFOV, 50.f), 115.f);
	}
}

float AknightPlayerCameraManager::GetKnockdownZoomWeight(double Now) const
{
	if (bKnockdownZoomReturning)
	{
		const float Alpha = KnockdownZoomReturn > 0.f
			? FMath::Clamp(static_cast<float>((Now - KnockdownZoomReturnStartTime) / KnockdownZoomReturn), 0.f, 1.f) : 1.f;
		// Explosive snap-out on release with a subtle shockwave wide-FOV rebound (-0.12) before settling.
		const float FastOut = 1.f - FMath::Pow(1.f - Alpha, 2.6f);
		const float Rebound = FMath::Sin(Alpha * PI) * (1.f - Alpha) * 0.14f;
		return KnockdownZoomReturnStartWeight * ((1.f - FastOut) - Rebound);
	}
	const float Alpha = KnockdownZoomEnter > 0.f
		? FMath::Clamp(static_cast<float>((Now - KnockdownZoomStartTime) / KnockdownZoomEnter), 0.f, 1.f) : 1.f;
	return Alpha * Alpha * (3.f - 2.f * Alpha);
}

void AknightPlayerCameraManager::StartKnockdownZoom(AActor* InViewTarget, float Degrees, float EnterSeconds, double StartRealTime)
{
	CancelKnockdownZoom();
	if (!InViewTarget || Degrees <= 0.f)
	{
		return;
	}
	KnockdownViewTarget = InViewTarget;
	KnockdownZoomDegrees = Degrees;
	KnockdownZoomEnter = FMath::Max(EnterSeconds, 0.f);
	KnockdownZoomStartTime = StartRealTime;
	bKnockdownZoomActive = true;
}

void AknightPlayerCameraManager::ReturnKnockdownZoom(float ReturnSeconds)
{
	if (!bKnockdownZoomActive || bKnockdownZoomReturning)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	KnockdownZoomReturnStartWeight = GetKnockdownZoomWeight(Now);
	KnockdownZoomReturnStartTime = Now;
	KnockdownZoomReturn = FMath::Max(ReturnSeconds, 0.f);
	bKnockdownZoomReturning = true;
}

void AknightPlayerCameraManager::CancelKnockdownZoom()
{
	bKnockdownZoomActive = false;
	bKnockdownZoomReturning = false;
	KnockdownViewTarget.Reset();
}

void AknightPlayerCameraManager::UpdateCamera(float DeltaTime)
{
	Super::UpdateCamera(DeltaTime);

	if (bKnockdownZoomActive)
	{
		const float ShakeScale = FMath::Clamp(GetShakeScale(), 0.f, 2.f);
		if (ShakeScale > KINDA_SMALL_NUMBER)
		{
			const double Now = FPlatformTime::Seconds();
			FPostProcessSettings ImpactPP;
			if (!bKnockdownZoomReturning)
			{
				const float Weight = FMath::Clamp(GetKnockdownZoomWeight(Now), 0.f, 1.f) * ShakeScale;
				ImpactPP.bOverride_VignetteIntensity = true;
				ImpactPP.VignetteIntensity = FMath::Lerp(0.4f, 0.72f, Weight);
				ImpactPP.bOverride_SceneFringeIntensity = true;
				ImpactPP.SceneFringeIntensity = 2.4f * Weight;
				AddCachedPPBlend(ImpactPP, FMath::Clamp(Weight, 0.f, 1.f), VTBlendOrder_Override);
			}
			else
			{
				const float ReturnAlpha = KnockdownZoomReturn > 0.f
					? FMath::Clamp(static_cast<float>((Now - KnockdownZoomReturnStartTime) / KnockdownZoomReturn), 0.f, 1.f) : 1.f;
				const float Pulse = FMath::Pow(1.f - ReturnAlpha, 1.4f) * KnockdownZoomReturnStartWeight * ShakeScale;
				if (Pulse > KINDA_SMALL_NUMBER)
				{
					ImpactPP.bOverride_SceneFringeIntensity = true;
					ImpactPP.SceneFringeIntensity = 5.0f * Pulse;
					ImpactPP.bOverride_VignetteIntensity = true;
					ImpactPP.VignetteIntensity = FMath::Lerp(0.4f, 0.68f, Pulse);
					AddCachedPPBlend(ImpactPP, FMath::Clamp(Pulse, 0.f, 1.f), VTBlendOrder_Override);
				}
			}
		}
	}

	const ULocalPlayer* const LocalPlayer = PCOwner ? PCOwner->GetLocalPlayer() : nullptr;
	UknightSettingsSubsystem* const Settings = LocalPlayer ? LocalPlayer->GetSubsystem<UknightSettingsSubsystem>() : nullptr;
	if (!Settings) return;
	float Offset, Brightness, Gamma;
	Settings->GetEffectiveDisplaySettings(Offset, Brightness, Gamma);
	if (FMath::IsNearlyEqual(Brightness, 1.f) && FMath::IsNearlyEqual(Gamma, 1.f)) return;

	// Keep scene and camera post process intact. This final camera blend only overrides the user's calibration.
	FPostProcessSettings Calibration;
	if (!FMath::IsNearlyEqual(Brightness, 1.f))
	{
		Calibration.bOverride_ColorGain = true;
		Calibration.ColorGain = FVector4(Brightness, Brightness, Brightness, 1.f);
	}
	if (!FMath::IsNearlyEqual(Gamma, 1.f))
	{
		Calibration.bOverride_ColorGamma = true;
		Calibration.ColorGamma = FVector4(Gamma, Gamma, Gamma, 1.f);
	}
	AddCachedPPBlend(Calibration, 1.f, VTBlendOrder_Override);
}

float AknightPlayerCameraManager::GetShakeScale() const
{
	const ULocalPlayer* const LocalPlayer = PCOwner ? PCOwner->GetLocalPlayer() : nullptr;
	UknightSettingsSubsystem* const Settings = LocalPlayer ? LocalPlayer->GetSubsystem<UknightSettingsSubsystem>() : nullptr;
	const UknightSharedSettings* const Shared = Settings ? Settings->GetSharedSettings() : nullptr;
	return Shared ? Shared->CameraShakeScale : 1.f;
}

UCameraShakeBase* AknightPlayerCameraManager::StartCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass, const FAddCameraShakeParams& Params)
{
	// Every shake (gameplay cues, abilities, the helpers below) passes through here.
	FAddCameraShakeParams Scaled = Params;
	Scaled.Scale *= GetShakeScale();
	if (Scaled.Scale <= 0.f)
	{
		return nullptr;
	}
	return Super::StartCameraShake(ShakeClass, Scaled);
}

void AknightPlayerCameraManager::PlayHitShake(float Intensity)
{
	if (HitShake && Intensity > 0.f)
	{
		StartCameraShake(HitShake, Intensity);
	}
}

void AknightPlayerCameraManager::PlayHeavyImpactShake(float Intensity)
{
	if (HeavyImpactShake && Intensity > 0.f)
	{
		StartCameraShake(HeavyImpactShake, Intensity);
	}
}

void AknightPlayerCameraManager::StartRoarShockwave(USceneComponent* Source, FName Socket)
{
	if (!RoarShockwaveClass)
	{
		return;
	}
	UknightCameraModifier_RoarShockwave* Modifier = Cast<UknightCameraModifier_RoarShockwave>(FindCameraModifierByClass(RoarShockwaveClass));
	if (!Modifier)
	{
		Modifier = Cast<UknightCameraModifier_RoarShockwave>(AddNewCameraModifier(RoarShockwaveClass));
	}
	if (Modifier)
	{
		Modifier->StartRoar(Source, Socket);
	}
}

void AknightPlayerCameraManager::StopRoarShockwave(const USceneComponent* Source)
{
	if (UknightCameraModifier_RoarShockwave* const Modifier = RoarShockwaveClass ? Cast<UknightCameraModifier_RoarShockwave>(FindCameraModifierByClass(RoarShockwaveClass)) : nullptr)
	{
		Modifier->StopRoar(Source);
	}
}
