#include "Camera/knightPlayerCameraManager.h"

#include "Camera/CameraModifier_CameraShake.h"
#include "Camera/CameraShakeBase.h"
#include "Engine/Scene.h"
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
	if (!Settings) return;
	float Offset, Brightness, Gamma;
	Settings->GetEffectiveDisplaySettings(Offset, Brightness, Gamma);
	if (!FMath::IsNearlyZero(Offset))
	{
		// Apply after the active camera mode or menu camera has produced its designed FOV.
		OutVT.POV.FOV = FMath::Clamp(OutVT.POV.FOV + Offset, 60.f, 110.f);
	}
}

void AknightPlayerCameraManager::UpdateCamera(float DeltaTime)
{
	Super::UpdateCamera(DeltaTime);
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
