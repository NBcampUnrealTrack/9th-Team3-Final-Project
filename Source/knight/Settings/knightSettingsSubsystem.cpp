#include "Settings/knightSettingsSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "knightAudioSubsystem.h"
#include "Settings/knightSharedSettings.h"

const FString UknightSettingsSubsystem::SlotName = TEXT("KnightSharedSettings");

UknightSharedSettings* UknightSettingsSubsystem::GetSharedSettings()
{
	RegisterMappableContexts();
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

void UknightSettingsSubsystem::RegisterMappableContexts()
{
	if (bMappableContextsRegistered || !GetLocalPlayer()) return;
	UEnhancedInputLocalPlayerSubsystem* const Input = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	UEnhancedInputUserSettings* const Keys = Input ? Input->GetUserSettings() : nullptr;
	if (!Keys) return;
	const TCHAR* const ContextPaths[] = {
		TEXT("/Game/Input/IMC_Default.IMC_Default"),
		TEXT("/Game/Knight/Input/IMC_Knight_Combat.IMC_Knight_Combat")
	};
	for (const TCHAR* Path : ContextPaths)
	{
		const UInputMappingContext* const Context = LoadObject<UInputMappingContext>(nullptr, Path);
		if (!Context) return;
		Keys->RegisterInputMappingContext(Context);
	}
	bMappableContextsRegistered = true;
}

void UknightSettingsSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);
	if (NewPlayerController)
	{
		GetSharedSettings();
	}
}

bool UknightSettingsSubsystem::SaveSharedSettings()
{
	if (!GetSharedSettings() || !SharedSettings->SaveGameToSlotForLocalPlayer()) return false;
	ApplyAudioSettings();
	return true;
}

bool UknightSettingsSubsystem::AreSubtitlesEnabled()
{
	const UknightSharedSettings* const Settings = GetSharedSettings();
	return Settings && Settings->bSubtitlesEnabled;
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
