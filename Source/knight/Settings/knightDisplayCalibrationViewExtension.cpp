#include "Settings/knightDisplayCalibrationViewExtension.h"

#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "SceneView.h"
#include "Settings/knightDisplayCalibration.h"
#include "Settings/knightSettingsSubsystem.h"

FknightDisplayCalibrationViewExtension::FknightDisplayCalibrationViewExtension(
	const FAutoRegister& AutoRegister, UknightSettingsSubsystem* InSettings)
	: FSceneViewExtensionBase(AutoRegister), Settings(InSettings)
{
}

bool FknightDisplayCalibrationViewExtension::IsActiveThisFrame_Internal(
	const FSceneViewExtensionContext& Context) const
{
	const ULocalPlayer* Player = Settings.IsValid() ? Settings->GetLocalPlayer() : nullptr;
	return Player && Player->ViewportClient && Player->ViewportClient->Viewport
		&& Context.Viewport == Player->ViewportClient->Viewport;
}

void FknightDisplayCalibrationViewExtension::SetupView(FSceneViewFamily& ViewFamily, FSceneView& View)
{
	UknightSettingsSubsystem* Owner = Settings.Get();
	const ULocalPlayer* Player = Owner ? Owner->GetLocalPlayer() : nullptr;
	if (!Player || !Player->ViewportClient || !Player->ViewportClient->Viewport
		|| ViewFamily.RenderTarget != Player->ViewportClient->Viewport
		|| View.PlayerIndex != Player->GetControllerId())
	{
		return;
	}
	float FieldOfViewOffset, Brightness, Gamma;
	Owner->GetEffectiveDisplaySettings(FieldOfViewOffset, Brightness, Gamma);
	// UE builds FinalPostProcessSettings anew before SetupView. No accumulation, camera
	// tick dependence, global CVars, exposure override, or SDR/HDR output-mode change.
	KnightDisplayCalibration::Compose(View.FinalPostProcessSettings, Brightness, Gamma);
}
