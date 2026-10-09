#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "knightAudioSubsystem.generated.h"

class UAudioComponent;
class USoundBase;
class USoundClass;
class USoundMix;
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
 * A non-looping track can be repeated with a pause between plays (LoopGap).
 *
 * Voice: one voice line at a time. A line requested while another plays waits in a queue and follows it after
 * VoiceQueueGap; lines that waited longer than VoiceMaxWait are dropped, since they no longer fit the moment.
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

	/**
	 * Starts Music (replacing the current track with a crossfade). FadeTime below zero uses the default.
	 * LoopGap above zero replays a non-looping track that many seconds after it ends.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Audio")
	void PlayMusic(USoundBase* Music, float FadeTime = -1.f, float LoopGap = 0.f);

	/** Plays a voice line now, or queues it behind the one playing. Urgent lines go to the front of the queue. */
	UFUNCTION(BlueprintCallable, Category="Knight|Audio")
	void PlayVoice(USoundBase* Voice, bool bUrgent = false);

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
	void ApplyVolume(UWorld* World, FName Category, float Volume);
	USoundMix* GetUserSoundMix();
	USoundClass* GetSoundClass(FName Category);
	float ResolveFadeTime(float FadeTime) const;
	void HandleMusicFinished(UAudioComponent* Component);
	void ReplayMusic();
	bool IsVoicePlaying() const;
	void StartVoice(USoundBase* Voice);
	void HandleVoiceFinished(UAudioComponent* Component);
	void PlayNextVoice();
	void ClearVoices();

	TMap<FName, float> Volumes;
	// The audio device keeps raw sound-class pointers in its async mix overrides.
	// Keep dynamically loaded assets alive for as long as this subsystem can use them.
	UPROPERTY(Transient)
	TObjectPtr<USoundMix> LoadedUserSoundMix;
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<USoundClass>> LoadedSoundClasses;
	TWeakObjectPtr<UWorld> SoundMixWorld;
	TWeakObjectPtr<UAudioComponent> MusicComponent;
	int32 MusicState = 0;
	float MusicLoopGap = 0.f;
	FTSTicker::FDelegateHandle MusicReplayTicker;

	/** Waiting voice lines, oldest first, with the time each was requested. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> VoiceQueue;
	TArray<double> VoiceQueueTimes;
	TWeakObjectPtr<UAudioComponent> VoiceComponent;
	FTSTicker::FDelegateHandle VoiceGapTicker;
	FDelegateHandle PostLoadMapHandle;
};
