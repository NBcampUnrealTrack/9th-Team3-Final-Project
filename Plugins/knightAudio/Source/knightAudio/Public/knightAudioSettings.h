#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "knightAudioSettings.generated.h"

class USoundControlBus;
class USoundControlBusMix;

/** Project Settings > Game > Knight Audio. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Knight Audio"))
class KNIGHTAUDIO_API UknightAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UknightAudioSettings();

	/** Mix holding the user volume buses. Activated in every world. */
	UPROPERTY(EditAnywhere, Config, Category="Volume", meta=(AllowedClasses="/Script/AudioModulation.SoundControlBusMix"))
	FSoftObjectPath UserVolumeMix;

	/** Runtime volume overrides for the Knight sound-class hierarchy. */
	UPROPERTY(EditAnywhere, Config, Category="Volume", meta=(AllowedClasses="/Script/Engine.SoundMix"))
	FSoftObjectPath UserSoundMix;

	UPROPERTY(EditAnywhere, Config, Category="Volume", meta=(AllowedClasses="/Script/Engine.SoundClass"))
	TMap<FName, FSoftObjectPath> SoundClasses;

	/** Volume category (Master, Music, SFX, Voice, UI) to the control bus that sounds of that category are routed through. */
	UPROPERTY(EditAnywhere, Config, Category="Volume", meta=(AllowedClasses="/Script/AudioModulation.SoundControlBus"))
	TMap<FName, FSoftObjectPath> VolumeBuses;

	/** Int parameter on music MetaSounds that selects the layer or section (see UknightAudioSubsystem::SetMusicState). */
	UPROPERTY(EditAnywhere, Config, Category="Music")
	FName MusicStateParameter = TEXT("MusicState");

	UPROPERTY(EditAnywhere, Config, Category="Music", meta=(ClampMin=0, Units="s"))
	float DefaultMusicFadeTime = 2.f;

	/** Pause between a voice line and the queued one after it. */
	UPROPERTY(EditAnywhere, Config, Category="Voice", meta=(ClampMin=0, Units="s"))
	float VoiceQueueGap = 0.3f;

	/** A queued voice line that has waited longer than this is dropped. */
	UPROPERTY(EditAnywhere, Config, Category="Voice", meta=(ClampMin=0, Units="s"))
	float VoiceMaxWait = 3.f;
};
