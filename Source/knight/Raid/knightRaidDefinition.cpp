#include "Raid/knightRaidDefinition.h"

#include "knightItemDefinition.h"
#include "Monster/knightMonsterDefinition.h"

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
