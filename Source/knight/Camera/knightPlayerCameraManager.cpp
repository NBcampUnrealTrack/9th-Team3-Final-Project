#include "Camera/knightPlayerCameraManager.h"

#include "Camera/CameraShakeBase.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Settings/knightSettingsSubsystem.h"
#include "Settings/knightSharedSettings.h"

AknightPlayerCameraManager::AknightPlayerCameraManager()
{
	ViewPitchMin = -70.f;
	ViewPitchMax = 60.f;
}

float AknightPlayerCameraManager::GetShakeScale() const
{
	const ULocalPlayer* const LocalPlayer = PCOwner ? PCOwner->GetLocalPlayer() : nullptr;
	UknightSettingsSubsystem* const Settings = LocalPlayer ? LocalPlayer->GetSubsystem<UknightSettingsSubsystem>() : nullptr;
	const UknightSharedSettings* const Shared = Settings ? Settings->GetSharedSettings() : nullptr;
	return Shared ? Shared->CameraShakeScale : 1.f;
}

void AknightPlayerCameraManager::PlayHitShake(float Intensity)
{
	const float Scale = Intensity * GetShakeScale();
	if (HitShake && Scale > 0.f)
	{
		StartCameraShake(HitShake, Scale);
	}
}

void AknightPlayerCameraManager::PlayHeavyImpactShake(float Intensity)
{
	const float Scale = Intensity * GetShakeScale();
	if (HeavyImpactShake && Scale > 0.f)
	{
		StartCameraShake(HeavyImpactShake, Scale);
	}
}
