#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "knightAudioSubsystem.generated.h"

class UAudioComponent;
class USoundBase;
class UWorld;

/**
 * Game-wide audio services.
 *
 * Volume: each category (Master, Music, SFX, Voice, UI) maps to an Audio Modulation control bus in
 * UknightAudioSettings. Values are remembered here and re-applied whenever a map loads, because bus mixes
 * live per world.
 *
 * Music: one looping music sound per world. SetMusicState writes an int parameter (MusicStateParameter)
 * so a MetaSound can switch layers or sections without restarting (explore, combat, enraged, victory).
 */
UCLASS()
class KNIGHTAUDIO_API UknightAudioSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 0..1. Unknown categories are logged and ignored. */
	UFUNCTION(BlueprintCallable, Category="Knight|Audio")
	void SetCategoryVolume(FName Category, float Volume);

	UFUNCTION(BlueprintPure, Category="Knight|Audio")
	float GetCategoryVolume(FName Category) const;

	/** Starts Music (replacing the current track with a crossfade). FadeTime below zero uses the default. */
	UFUNCTION(BlueprintCallable, Category="Knight|Audio")
	void PlayMusic(USoundBase* Music, float FadeTime = -1.f);

	UFUNCTION(BlueprintCallable, Category="Knight|Audio")
	void StopMusic(float FadeTime = -1.f);

	/** Sets the music state parameter. Ignored while no music plays. */
	UFUNCTION(BlueprintCallable, Category="Knight|Audio")
	void SetMusicState(int32 State);

	UFUNCTION(BlueprintPure, Category="Knight|Audio")
	int32 GetMusicState() const { return MusicState; }

private:
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void ApplyAllVolumes(UWorld* World);
	void ApplyVolume(UWorld* World, FName Category, float Volume) const;
	float ResolveFadeTime(float FadeTime) const;

	TMap<FName, float> Volumes;
	TWeakObjectPtr<UAudioComponent> MusicComponent;
	int32 MusicState = 0;
	FDelegateHandle PostLoadMapHandle;
};
