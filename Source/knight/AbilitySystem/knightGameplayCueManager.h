#pragma once

#include "CoreMinimal.h"
#include "GameplayCueManager.h"
#include "knightGameplayCueManager.generated.h"

struct FStreamableHandle;

/**
 * Loads gameplay cue notifies on demand instead of all at startup, so cue content (Niagara, sounds)
 * for every monster is not resident all the time. A cue that is not loaded yet starts loading asynchronously
 * the first time it fires, and that first firing can be skipped. PreloadCues loads a set up front (the raid
 * preloads its RaidDefinition::PreloadGameplayCues) so hits and breaks show their effect from the start.
 *
 * Set as the Global Gameplay Cue Manager Class in Project Settings > Gameplay Abilities Settings.
 */
UCLASS()
class KNIGHT_API UknightGameplayCueManager : public UGameplayCueManager
{
	GENERATED_BODY()

public:
	static UknightGameplayCueManager* Get();

	/** Asynchronously loads every cue notify whose tag matches one of Tags (parents include children). */
	void PreloadCues(const FGameplayTagContainer& Tags);

	/** Releases cues kept by PreloadCues (called when a raid ends). */
	void ReleasePreloadedCues();

protected:
	virtual bool ShouldAsyncLoadRuntimeObjectLibraries() const override { return false; }
	virtual bool ShouldSyncLoadMissingGameplayCues() const override { return false; }
	virtual bool ShouldAsyncLoadMissingGameplayCues() const override { return true; }

private:
	TArray<TSharedPtr<FStreamableHandle>> PreloadHandles;
};
