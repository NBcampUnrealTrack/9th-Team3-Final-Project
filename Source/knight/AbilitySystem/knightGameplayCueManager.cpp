#include "AbilitySystem/knightGameplayCueManager.h"

#include "AbilitySystemGlobals.h"
#include "Engine/AssetManager.h"
#include "GameplayCueSet.h"
#include "knight.h"

UknightGameplayCueManager* UknightGameplayCueManager::Get()
{
	return Cast<UknightGameplayCueManager>(UAbilitySystemGlobals::Get().GetGameplayCueManager());
}

void UknightGameplayCueManager::PreloadCues(const FGameplayTagContainer& Tags)
{
	UGameplayCueSet* const CueSet = GetRuntimeCueSet();
	if (!CueSet || Tags.IsEmpty())
	{
		return;
	}
	TArray<FSoftObjectPath> Paths;
	for (const FGameplayCueNotifyData& Data : CueSet->GameplayCueData)
	{
		if (Data.GameplayCueNotifyObj.IsValid() && Data.GameplayCueTag.MatchesAny(Tags) && !Data.LoadedGameplayCueClass)
		{
			Paths.AddUnique(Data.GameplayCueNotifyObj);
		}
	}
	if (Paths.IsEmpty())
	{
		return;
	}
	UE_LOG(LogknightAbility, Verbose, TEXT("Preloading %d gameplay cues for %s."), Paths.Num(), *Tags.ToStringSimple());
	// Keeping the handle keeps the cues loaded until ReleasePreloadedCues.
	if (TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths, FStreamableDelegate(), FStreamableManager::AsyncLoadHighPriority))
	{
		PreloadHandles.Add(Handle);
	}
}

void UknightGameplayCueManager::ReleasePreloadedCues()
{
	for (const TSharedPtr<FStreamableHandle>& Handle : PreloadHandles)
	{
		if (Handle.IsValid())
		{
			Handle->ReleaseHandle();
		}
	}
	PreloadHandles.Reset();
}
