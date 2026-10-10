#include "knightAudioSubsystem.h"

#include "AudioModulationStatics.h"
#include "Components/AudioComponent.h"
#include "AudioDeviceHandle.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "SoundControlBus.h"
#include "SoundControlBusMix.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UObject/UObjectGlobals.h"
#include "knightAudioModule.h"
#include "knightAudioSettings.h"

UknightAudioSettings::UknightAudioSettings()
{
	SoundClasses.Add(TEXT("Master"), FSoftObjectPath(TEXT("/Game/Knight/Audio/Settings/SC_Knight_Master.SC_Knight_Master")));
	SoundClasses.Add(TEXT("Music"), FSoftObjectPath(TEXT("/Game/Knight/Audio/Settings/SC_Knight_BGM.SC_Knight_BGM")));
	SoundClasses.Add(TEXT("SFX"), FSoftObjectPath(TEXT("/Game/Knight/Audio/Settings/SC_Knight_SFX.SC_Knight_SFX")));
	SoundClasses.Add(TEXT("Voice"), FSoftObjectPath(TEXT("/Game/Knight/Audio/Settings/SC_Knight_Voice.SC_Knight_Voice")));
}

void UknightAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
	WorldInitializedHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &ThisClass::HandleWorldInitialized);
}

void UknightAudioSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitializedHandle);
	if (UWorld* const World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		if (USoundMix* const Mix = LoadedUserSoundMix.Get(); Mix && World->AllowAudioPlayback()
			&& World->GetAudioDevice() && World->GetAudioDevice().GetDeviceID() == PushedSoundMixDeviceId)
		{
			UGameplayStatics::PopSoundMixModifier(World, Mix);
		}
	}
	FTSTicker::RemoveTicker(MusicReplayTicker);
	ClearVoices();
	Super::Deinitialize();
}

void UknightAudioSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (LoadedWorld && LoadedWorld->GetGameInstance() == GetGameInstance())
	{
		ApplyAllVolumes(LoadedWorld);
		// Lines queued in the previous map belong to it.
		ClearVoices();
	}
}

void UknightAudioSubsystem::HandleWorldInitialized(const FActorsInitializedParams& Params)
{
	if (!Params.World || Params.World->GetGameInstance() != GetGameInstance()) return;
	const TWeakObjectPtr<UWorld> WeakWorld(Params.World);
	Params.World->OnWorldBeginPlay.AddWeakLambda(this, [this, WeakWorld]
	{
		if (UWorld* const World = WeakWorld.Get()) ApplyAllVolumes(World);
	});
}

bool UknightAudioSubsystem::EnsureUserSoundMix(UWorld* World, USoundMix* SoundMix)
{
	if (!World || !SoundMix || !World->AllowAudioPlayback() || !GEngine || !GEngine->UseSound()) return false;
	const FAudioDeviceHandle Device = World->GetAudioDevice();
	if (!Device) return false;
	if (PushedSoundMixDeviceId != Device.GetDeviceID())
	{
		UGameplayStatics::PushSoundMixModifier(World, SoundMix);
		PushedSoundMixDeviceId = Device.GetDeviceID();
	}
	return true;
}

USoundMix* UknightAudioSubsystem::GetUserSoundMix()
{
	if (!LoadedUserSoundMix)
	{
		LoadedUserSoundMix = Cast<USoundMix>(GetDefault<UknightAudioSettings>()->UserSoundMix.TryLoad());
	}
	return LoadedUserSoundMix;
}

USoundClass* UknightAudioSubsystem::GetSoundClass(FName Category)
{
	if (const TObjectPtr<USoundClass>* Loaded = LoadedSoundClasses.Find(Category))
	{
		return Loaded->Get();
	}
	const FSoftObjectPath* ClassPath = GetDefault<UknightAudioSettings>()->SoundClasses.Find(Category);
	USoundClass* SoundClass = ClassPath ? Cast<USoundClass>(ClassPath->TryLoad()) : nullptr;
	if (SoundClass)
	{
		LoadedSoundClasses.Add(Category, SoundClass);
	}
	return SoundClass;
}

void UknightAudioSubsystem::ApplyAllVolumes(UWorld* World)
{
	if (!World)
	{
		return;
	}
	const UknightAudioSettings* const Settings = GetDefault<UknightAudioSettings>();
	if (USoundMix* const SoundMix = GetUserSoundMix())
	{
		EnsureUserSoundMix(World, SoundMix);
	}
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

void UknightAudioSubsystem::ApplyVolume(UWorld* World, FName Category, float Volume)
{
	if (!World)
	{
		return;
	}
	const UknightAudioSettings* const Settings = GetDefault<UknightAudioSettings>();
	USoundMix* const SoundMix = GetUserSoundMix();
	USoundClass* const SoundClass = GetSoundClass(Category);
	if (SoundMix && SoundClass)
	{
		if (EnsureUserSoundMix(World, SoundMix))
		{
			UGameplayStatics::SetSoundMixClassOverride(World, SoundMix, SoundClass, Volume, 1.f, 0.1f,
				Category == TEXT("Master"));
		}
		return;
	}
	const FSoftObjectPath* const BusPath = Settings->VolumeBuses.Find(Category);
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

void UknightAudioSubsystem::PlayMusic(USoundBase* Music, float FadeTime, float LoopGap)
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
		Current->OnAudioFinishedNative.RemoveAll(this);
		Current->FadeOut(Fade, 0.f);
	}
	FTSTicker::RemoveTicker(MusicReplayTicker);
	MusicReplayTicker.Reset();

	// A track replayed after a gap must outlive its first play, so it is not auto-destroyed.
	const bool bReplay = LoopGap > 0.f;
	UAudioComponent* const NewMusic = UGameplayStatics::CreateSound2D(World, Music, 1.f, 1.f, 0.f, nullptr, false, !bReplay);
	if (!NewMusic)
	{
		return;
	}
	NewMusic->SoundClassOverride = GetSoundClass(TEXT("Music"));
	NewMusic->SetIntParameter(GetDefault<UknightAudioSettings>()->MusicStateParameter, MusicState);
	if (bReplay)
	{
		NewMusic->OnAudioFinishedNative.AddUObject(this, &ThisClass::HandleMusicFinished);
	}
	NewMusic->FadeIn(Fade);
	MusicComponent = NewMusic;
	MusicLoopGap = LoopGap;
}

void UknightAudioSubsystem::HandleMusicFinished(UAudioComponent* Component)
{
	if (Component != MusicComponent.Get() || MusicReplayTicker.IsValid())
	{
		return;
	}
	MusicReplayTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
	{
		MusicReplayTicker.Reset();
		ReplayMusic();
		return false;
	}), MusicLoopGap);
}

void UknightAudioSubsystem::ReplayMusic()
{
	if (UAudioComponent* const Current = MusicComponent.Get())
	{
		Current->SetIntParameter(GetDefault<UknightAudioSettings>()->MusicStateParameter, MusicState);
		Current->Play();
	}
}

void UknightAudioSubsystem::StopMusic(float FadeTime)
{
	FTSTicker::RemoveTicker(MusicReplayTicker);
	MusicReplayTicker.Reset();
	if (UAudioComponent* const Current = MusicComponent.Get())
	{
		Current->OnAudioFinishedNative.RemoveAll(this);
		Current->FadeOut(ResolveFadeTime(FadeTime), 0.f);
	}
	MusicComponent.Reset();
}

void UknightAudioSubsystem::PlayVoice(USoundBase* Voice, bool bUrgent)
{
	if (!Voice)
	{
		return;
	}
	UAudioComponent* const Current = VoiceComponent.Get();
	if ((Current && Current->IsPlaying() && Current->GetSound() == Voice) || VoiceQueue.Contains(Voice))
	{
		return;
	}
	if (!IsVoicePlaying() && !VoiceGapTicker.IsValid())
	{
		StartVoice(Voice);
		return;
	}
	const int32 Index = bUrgent ? 0 : VoiceQueue.Num();
	VoiceQueue.Insert(Voice, Index);
	VoiceQueueTimes.Insert(FPlatformTime::Seconds(), Index);
}

bool UknightAudioSubsystem::IsVoicePlaying() const
{
	const UAudioComponent* const Current = VoiceComponent.Get();
	return Current && Current->IsPlaying();
}

void UknightAudioSubsystem::StartVoice(USoundBase* Voice)
{
	UWorld* const World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	UAudioComponent* const Component = World ? UGameplayStatics::CreateSound2D(World, Voice, 1.f, 1.f, 0.f, nullptr, false, true) : nullptr;
	if (!Component)
	{
		return;
	}
	Component->SoundClassOverride = GetSoundClass(TEXT("Voice"));
	Component->OnAudioFinishedNative.AddUObject(this, &ThisClass::HandleVoiceFinished);
	Component->Play();
	VoiceComponent = Component;
}

void UknightAudioSubsystem::HandleVoiceFinished(UAudioComponent* Component)
{
	if (Component != VoiceComponent.Get())
	{
		return;
	}
	VoiceComponent.Reset();
	if (VoiceQueue.IsEmpty() || VoiceGapTicker.IsValid())
	{
		return;
	}
	VoiceGapTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
	{
		VoiceGapTicker.Reset();
		PlayNextVoice();
		return false;
	}), GetDefault<UknightAudioSettings>()->VoiceQueueGap);
}

void UknightAudioSubsystem::PlayNextVoice()
{
	const double Now = FPlatformTime::Seconds();
	const float MaxWait = GetDefault<UknightAudioSettings>()->VoiceMaxWait;
	while (!VoiceQueue.IsEmpty())
	{
		USoundBase* const Next = VoiceQueue[0];
		const double QueuedAt = VoiceQueueTimes[0];
		VoiceQueue.RemoveAt(0);
		VoiceQueueTimes.RemoveAt(0);
		if (Next && Now - QueuedAt <= MaxWait)
		{
			StartVoice(Next);
			return;
		}
	}
}

void UknightAudioSubsystem::ClearVoices()
{
	FTSTicker::RemoveTicker(VoiceGapTicker);
	VoiceGapTicker.Reset();
	VoiceQueue.Reset();
	VoiceQueueTimes.Reset();
	VoiceComponent.Reset();
}

void UknightAudioSubsystem::SetMusicState(int32 State)
{
	MusicState = State;
	if (UAudioComponent* const Current = MusicComponent.Get())
	{
		Current->SetIntParameter(GetDefault<UknightAudioSettings>()->MusicStateParameter, State);
	}
}
