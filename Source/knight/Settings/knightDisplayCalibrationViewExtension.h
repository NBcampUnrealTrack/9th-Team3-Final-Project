#pragma once

#include "SceneViewExtension.h"

class UknightSettingsSubsystem;

/** Local-player scene calibration after volume/camera blending; also runs on paused views. */
class FknightDisplayCalibrationViewExtension final : public FSceneViewExtensionBase
{
public:
	FknightDisplayCalibrationViewExtension(const FAutoRegister& AutoRegister, UknightSettingsSubsystem* InSettings);
	virtual void SetupView(FSceneViewFamily& ViewFamily, FSceneView& View) override;

protected:
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

private:
	TWeakObjectPtr<UknightSettingsSubsystem> Settings;
};
