#include "System/knightAssetManager.h"

#include "System/knightTeams.h"
#include "Raid/knightRaidDefinition.h"
#include "Misc/ScopeLock.h"
#include "knight.h"

UknightAssetManager& UknightAssetManager::Get()
{
	check(GEngine);
	if (UknightAssetManager* const Singleton = Cast<UknightAssetManager>(GEngine->AssetManager))
	{
		return *Singleton;
	}
	UE_LOG(LogknightData, Fatal, TEXT("AssetManagerClassName in DefaultEngine.ini must be knightAssetManager."));
	// Fatal never returns; this keeps the compiler satisfied.
	return *NewObject<UknightAssetManager>();
}

TSharedPtr<FStreamableHandle> UknightAssetManager::LoadRaidDefinitionAsync(
	const TSoftObjectPtr<UknightRaidDefinition>& Definition, FStreamableDelegate Completion)
{
	if (Definition.IsNull()) return nullptr;
	return UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Definition.ToSoftObjectPath(), MoveTemp(Completion));
}

void UknightAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();
	knightTeams::RegisterAttitudeSolver();
}

UObject* UknightAssetManager::SynchronousLoadAsset(const FSoftObjectPath& AssetPath)
{
	if (!AssetPath.IsValid())
	{
		return nullptr;
	}
	if (UAssetManager::IsInitialized())
	{
		UE_LOG(LogknightData, Verbose, TEXT("Synchronous load: %s"), *AssetPath.ToString());
		return UAssetManager::GetStreamableManager().LoadSynchronous(AssetPath, false);
	}
	// Before the asset manager exists (very early startup), fall back to a plain load.
	return AssetPath.TryLoad();
}

void UknightAssetManager::AddLoadedAsset(const UObject* Asset)
{
	if (ensureAlways(Asset))
	{
		FScopeLock Lock(&LoadedAssetsCritical);
		LoadedAssets.Add(Asset);
	}
}
