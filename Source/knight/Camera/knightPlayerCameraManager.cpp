#include "Camera/knightPlayerCameraManager.h"

#include "Camera/CameraShakeBase.h"

AknightPlayerCameraManager::AknightPlayerCameraManager()
{
	ViewPitchMin = -70.f;
	ViewPitchMax = 60.f;
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
