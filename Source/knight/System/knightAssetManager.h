#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "knightAssetManager.generated.h"

/**
 * Project asset manager. Set as AssetManagerClassName in DefaultEngine.ini.
 *
 * Rule: gameplay classes and data assets hold soft references to heavy content (meshes, montages,
 * Niagara, other data assets) and resolve them here or through async streaming. Primary asset types
 * are registered in DefaultGame.ini under [/Script/Engine.AssetManagerSettings] as each type is added.
 */
UCLASS(Config=Game)
class KNIGHT_API UknightAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	static UknightAssetManager& Get();

	/** Synchronously loads a soft object. Logs the load so hitches are visible, and can pin the result. */
	template<typename AssetType>
	static AssetType* GetAsset(const TSoftObjectPtr<AssetType>& AssetPointer, bool bKeepInMemory = true);

	/** Synchronously loads a soft class. Logs the load so hitches are visible, and can pin the result. */
	template<typename AssetType>
	static TSubclassOf<AssetType> GetSubclass(const TSoftClassPtr<AssetType>& AssetPointer, bool bKeepInMemory = true);

protected:
	virtual void StartInitialLoading() override;

	static UObject* SynchronousLoadAsset(const FSoftObjectPath& AssetPath);
	void AddLoadedAsset(const UObject* Asset);

private:
	/** Assets pinned by GetAsset/GetSubclass with bKeepInMemory. */
	UPROPERTY()
	TSet<TObjectPtr<const UObject>> LoadedAssets;

	FCriticalSection LoadedAssetsCritical;
};

template<typename AssetType>
AssetType* UknightAssetManager::GetAsset(const TSoftObjectPtr<AssetType>& AssetPointer, bool bKeepInMemory)
{
	AssetType* LoadedAsset = nullptr;
	const FSoftObjectPath& AssetPath = AssetPointer.ToSoftObjectPath();
	if (AssetPath.IsValid())
	{
		LoadedAsset = AssetPointer.Get();
		if (!LoadedAsset)
		{
			LoadedAsset = Cast<AssetType>(SynchronousLoadAsset(AssetPath));
			ensureAlwaysMsgf(LoadedAsset, TEXT("Failed to load asset [%s]"), *AssetPointer.ToString());
		}
		if (LoadedAsset && bKeepInMemory)
		{
			Get().AddLoadedAsset(Cast<UObject>(LoadedAsset));
		}
	}
	return LoadedAsset;
}

template<typename AssetType>
TSubclassOf<AssetType> UknightAssetManager::GetSubclass(const TSoftClassPtr<AssetType>& AssetPointer, bool bKeepInMemory)
{
	TSubclassOf<AssetType> LoadedSubclass;
	const FSoftObjectPath& AssetPath = AssetPointer.ToSoftObjectPath();
	if (AssetPath.IsValid())
	{
		LoadedSubclass = AssetPointer.Get();
		if (!LoadedSubclass)
		{
			LoadedSubclass = Cast<UClass>(SynchronousLoadAsset(AssetPath));
			ensureAlwaysMsgf(LoadedSubclass, TEXT("Failed to load asset class [%s]"), *AssetPointer.ToString());
		}
		if (LoadedSubclass && bKeepInMemory)
		{
			Get().AddLoadedAsset(Cast<UObject>(LoadedSubclass));
		}
	}
	return LoadedSubclass;
}
