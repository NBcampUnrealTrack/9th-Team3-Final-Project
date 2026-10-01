#include "knightAudioSubsystem.h"

#include "AudioModulationStatics.h"
#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "SoundControlBus.h"
#include "SoundControlBusMix.h"
#include "Sound/SoundBase.h"
#include "UObject/UObjectGlobals.h"
#include "knightAudioModule.h"
#include "knightAudioSettings.h"

void UknightAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
}

void UknightAudioSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	Super::Deinitialize();
}

void UknightAudioSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (LoadedWorld && LoadedWorld->GetGameInstance() == GetGameInstance())
	{
		ApplyAllVolumes(LoadedWorld);
	}
}

void UknightAudioSubsystem::ApplyAllVolumes(UWorld* World)
{
	if (!World)
	{
		return;
	}
	const UknightAudioSettings* const Settings = GetDefault<UknightAudioSettings>();
	if (USoundControlBusMix* const Mix = Cast<USoundControlBusMix>(Settings->UserVolumeMix.TryLoad()))
	{
		UAudioModulationStatics::ActivateBusMix(World, Mix);
	}
	for (const TPair<FName, float>& Entry : Volumes)
	{
		ApplyVolume(World, Entry.Key, Entry.Value);
	}
}

void UknightAudioSubsystem::SetCategoryVolume(FName Category, float Volume)
{
	const float Clamped = FMath::Clamp(Volume, 0.f, 1.f);
	Volumes.Add(Category, Clamped);
	ApplyVolume(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr, Category, Clamped);
}

float UknightAudioSubsystem::GetCategoryVolume(FName Category) const
{
	const float* const Volume = Volumes.Find(Category);
	return Volume ? *Volume : 1.f;
}

void UknightAudioSubsystem::ApplyVolume(UWorld* World, FName Category, float Volume) const
{
	if (!World)
	{
		return;
	}
	const FSoftObjectPath* const BusPath = GetDefault<UknightAudioSettings>()->VolumeBuses.Find(Category);
	USoundControlBus* const Bus = BusPath ? Cast<USoundControlBus>(BusPath->TryLoad()) : nullptr;
	if (!Bus)
	{
		UE_LOG(LogknightAudio, Warning, TEXT("No control bus for volume category %s (Project Settings > Knight Audio)."), *Category.ToString());
		return;
	}
	UAudioModulationStatics::SetGlobalBusMixValue(World, Bus, Volume, 0.1f);
}

float UknightAudioSubsystem::ResolveFadeTime(float FadeTime) const
{
	return FadeTime >= 0.f ? FadeTime : GetDefault<UknightAudioSettings>()->DefaultMusicFadeTime;
}

void UknightAudioSubsystem::PlayMusic(USoundBase* Music, float FadeTime)
{
	UWorld* const World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World || !Music)
	{
		return;
	}
	const float Fade = ResolveFadeTime(FadeTime);
	if (UAudioComponent* const Current = MusicComponent.Get())
	{
		if (Current->GetSound() == Music && Current->IsPlaying())
		{
			return;
		}
		Current->FadeOut(Fade, 0.f);
	}
	UAudioComponent* const NewMusic = UGameplayStatics::CreateSound2D(World, Music, 1.f, 1.f, 0.f, nullptr, false, true);
	if (!NewMusic)
	{
		return;
	}
	NewMusic->SetIntParameter(GetDefault<UknightAudioSettings>()->MusicStateParameter, MusicState);
	NewMusic->FadeIn(Fade);
	MusicComponent = NewMusic;
}

void UknightAudioSubsystem::StopMusic(float FadeTime)
{
	if (UAudioComponent* const Current = MusicComponent.Get())
	{
		Current->FadeOut(ResolveFadeTime(FadeTime), 0.f);
	}
	MusicComponent.Reset();
}

void UknightAudioSubsystem::SetMusicState(int32 State)
{
	MusicState = State;
	if (UAudioComponent* const Current = MusicComponent.Get())
	{
		Current->SetIntParameter(GetDefault<UknightAudioSettings>()->MusicStateParameter, State);
	}
}
