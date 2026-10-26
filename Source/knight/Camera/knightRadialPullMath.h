#pragma once

#include "CoreMinimal.h"

namespace KnightRadialPull
{
inline float Smooth(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }

// Actual seconds, independent of world/actor time dilation. Overlong attack/release
// proportions are fitted into Duration, never added on top of it.
inline float Envelope(double Age, float Duration, float Attack, float Release, float StartWeight = 0.f)
{
	if (Age < 0.0 || Duration <= 0.f || Age >= Duration) return 0.f;
	Attack = FMath::Max(Attack, 0.f);
	Release = FMath::Max(Release, 0.001f);
	const float Fit = FMath::Min(1.f, Duration / (Attack + Release));
	Attack *= Fit; Release *= Fit;
	if (Attack > 0.f && Age < Attack)
		return FMath::Lerp(FMath::Clamp(StartWeight, 0.f, 1.f), 1.f, Smooth(Age / Attack));
	return 1.f - Smooth((Age - (Duration - Release)) / Release);
}

inline FVector2D ProjectFocus(const FVector& WorldPosition, const FMatrix& ViewProjection)
{
	const FVector4 Clip = ViewProjection.TransformFVector4(FVector4(WorldPosition, 1.0));
	if (!FMath::IsFinite(Clip.W) || Clip.W <= SMALL_NUMBER) return FVector2D(.5, .5);
	const FVector2D UV(.5 + .5 * Clip.X / Clip.W, .5 - .5 * Clip.Y / Clip.W);
	return !UV.ContainsNaN() && UV.X >= 0 && UV.X <= 1 && UV.Y >= 0 && UV.Y <= 1
		? UV : FVector2D(.5, .5);
}
}
