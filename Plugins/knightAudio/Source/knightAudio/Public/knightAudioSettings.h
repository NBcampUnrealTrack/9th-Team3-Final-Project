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
	/** Mix holding the user volume buses. Activated in every world. */
	UPROPERTY(EditAnywhere, Config, Category="Volume", meta=(AllowedClasses="/Script/AudioModulation.SoundControlBusMix"))
	FSoftObjectPath UserVolumeMix;

	/** Volume category (Master, Music, SFX, Voice, UI) to the control bus that sounds of that category are routed through. */
	UPROPERTY(EditAnywhere, Config, Category="Volume", meta=(AllowedClasses="/Script/AudioModulation.SoundControlBus"))
	TMap<FName, FSoftObjectPath> VolumeBuses;

	/** Int parameter on music MetaSounds that selects the layer or section (see UknightAudioSubsystem::SetMusicState). */
	UPROPERTY(EditAnywhere, Config, Category="Music")
	FName MusicStateParameter = TEXT("MusicState");

	UPROPERTY(EditAnywhere, Config, Category="Music", meta=(ClampMin=0, Units="s"))
	float DefaultMusicFadeTime = 2.f;
};
