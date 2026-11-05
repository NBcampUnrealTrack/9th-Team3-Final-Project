#include "Raid/knightRaidDefinition.h"

#include "knightItemDefinition.h"
#include "Monster/knightMonsterDefinition.h"

FknightRaidFinishSettings::FknightRaidFinishSettings()
{
	// 앞 아래에서 올려다본 얼굴 → 옆에서 넓게 → 뒤 위에서 내려다본 전체.
	FknightRaidFinishShot Low;
	Low.Yaw = 30.f;
	Low.Pitch = -4.f;
	Low.Distance = 1.1f;
	Low.FieldOfView = 42.f;
	Low.DriftYaw = 12.f;
	Low.FocusHeight = 0.25f;
	FknightRaidFinishShot Side;
	Side.Yaw = 100.f;
	Side.Pitch = 12.f;
	Side.Distance = 1.6f;
	Side.FieldOfView = 55.f;
	Side.DriftYaw = -14.f;
	FknightRaidFinishShot High;
	High.Yaw = 205.f;
	High.Pitch = 32.f;
	High.Distance = 1.9f;
	High.FieldOfView = 50.f;
	High.DriftYaw = 16.f;
	High.DriftIn = 0.12f;
	Shots = { Low, Side, High };

	SlowPostProcess.bOverride_ColorSaturation = true;
	SlowPostProcess.ColorSaturation = FVector4(0.55f, 0.55f, 0.55f, 1.f);
	SlowPostProcess.bOverride_ColorContrast = true;
	SlowPostProcess.ColorContrast = FVector4(1.12f, 1.12f, 1.12f, 1.f);
	SlowPostProcess.bOverride_VignetteIntensity = true;
	SlowPostProcess.VignetteIntensity = 0.75f;
	SlowPostProcess.bOverride_SceneFringeIntensity = true;
	SlowPostProcess.SceneFringeIntensity = 2.5f;
	SlowPostProcess.bOverride_FilmGrainIntensity = true;
	SlowPostProcess.FilmGrainIntensity = 0.2f;
}

void UknightRaidDefinition::RollRewards(const FGameplayTagContainer& BrokenParts, TArray<FknightItemStack>& OutRewards) const
{
	OutRewards.Reset();
	for (const FknightRaidReward& Reward : Rewards)
	{
		if (!Reward.Item)
		{
			continue;
		}
		if (Reward.RequiredBrokenPart.IsValid() && !BrokenParts.HasTagExact(Reward.RequiredBrokenPart))
		{
			continue;
		}
		if (FMath::FRand() > Reward.Chance)
		{
			continue;
		}
		const int32 Count = FMath::RandRange(Reward.MinCount, FMath::Max(Reward.MinCount, Reward.MaxCount));
		if (FknightItemStack* const Existing = OutRewards.FindByPredicate([&Reward](const FknightItemStack& Stack) { return Stack.Item == Reward.Item; }))
		{
			Existing->Count += Count;
		}
		else
		{
			OutRewards.Emplace(Reward.Item, Count);
		}
	}
}
