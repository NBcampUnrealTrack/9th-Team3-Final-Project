#include "Camera/knightPlayerCameraManager.h"

#include "Camera/CameraModifier_CameraShake.h"
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
