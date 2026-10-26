#pragma once

#include "CoreMinimal.h"
#include "Engine/Scene.h"

namespace KnightDisplayCalibration
{
inline float Sanitize(float Value)
{
	return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.8f, 1.2f) : 1.f;
}

// Called once on each freshly blended game view, never on camera or asset settings.
// The LUT multiplies RGB by W, so the master channel preserves authored colour ratios.
inline void Compose(FPostProcessSettings& Settings, float Brightness, float Gamma)
{
	Settings.ColorGain.W *= Sanitize(Brightness);
	Settings.ColorGamma.W *= Sanitize(Gamma);
}
}
