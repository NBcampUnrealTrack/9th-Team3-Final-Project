#include "knightInventoryLibrary.h"

#include "Engine/AssetManager.h"
#include "knightInventoryModule.h"
#include "knightItemDefinition.h"

const UknightItemDefinition* UknightInventoryLibrary::LoadItemDefinition(FPrimaryAssetId ItemId)
{
	if (!ItemId.IsValid() || !UAssetManager::IsInitialized())
	{
		return nullptr;
	}
	UAssetManager& AssetManager = UAssetManager::Get();
	if (UObject* const Loaded = AssetManager.GetPrimaryAssetObject(ItemId))
	{
		return Cast<UknightItemDefinition>(Loaded);
	}
	const FSoftObjectPath Path = AssetManager.GetPrimaryAssetPath(ItemId);
	if (!Path.IsValid())
	{
		UE_LOG(LogknightInventory, Warning, TEXT("Item %s is not registered with the asset manager. Is the Item type scanned?"), *ItemId.ToString());
		return nullptr;
	}
	return Cast<UknightItemDefinition>(UAssetManager::GetStreamableManager().LoadSynchronous(Path, false));
}
