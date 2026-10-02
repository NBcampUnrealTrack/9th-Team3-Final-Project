#include "Settings/knightSettingsSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "knightAudioSubsystem.h"
#include "Settings/knightSharedSettings.h"

const FString UknightSettingsSubsystem::SlotName = TEXT("KnightSharedSettings");

UknightSharedSettings* UknightSettingsSubsystem::GetSharedSettings()
{
	if (!SharedSettings)
	{
		if (const ULocalPlayer* const LocalPlayer = GetLocalPlayer())
		{
			SharedSettings = Cast<UknightSharedSettings>(
				ULocalPlayerSaveGame::LoadOrCreateSaveGameForLocalPlayer(UknightSharedSettings::StaticClass(), LocalPlayer, SlotName));
			ApplyAudioSettings();
		}
	}
	return SharedSettings;
}

void UknightSettingsSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);
	if (NewPlayerController)
	{
		GetSharedSettings();
	}
}

void UknightSettingsSubsystem::SaveSharedSettings()
{
	if (SharedSettings)
	{
		ApplyAudioSettings();
		SharedSettings->AsyncSaveGameToSlotForLocalPlayer();
	}
}

void UknightSettingsSubsystem::ApplyAudioSettings()
{
	const ULocalPlayer* const LocalPlayer = GetLocalPlayer();
	UknightAudioSubsystem* const Audio = LocalPlayer && LocalPlayer->GetGameInstance()
		? LocalPlayer->GetGameInstance()->GetSubsystem<UknightAudioSubsystem>() : nullptr;
	if (!Audio || !SharedSettings)
	{
		return;
	}
	Audio->SetCategoryVolume(TEXT("Master"), SharedSettings->MasterVolume);
	Audio->SetCategoryVolume(TEXT("Music"), SharedSettings->MusicVolume);
	Audio->SetCategoryVolume(TEXT("SFX"), SharedSettings->SFXVolume);
	Audio->SetCategoryVolume(TEXT("Voice"), SharedSettings->VoiceVolume);
}
