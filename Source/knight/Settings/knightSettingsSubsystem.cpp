#include "Settings/knightSettingsSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
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

void UknightSettingsSubsystem::PreviewDisplaySettings(float FieldOfViewOffset, float Brightness, float Gamma)
{
	bDisplayPreviewActive = true;
	PreviewFieldOfViewOffset = FMath::Clamp(FieldOfViewOffset, -20.f, 30.f);
	PreviewBrightness = FMath::Clamp(Brightness, 0.8f, 1.2f);
	PreviewGamma = FMath::Clamp(Gamma, 0.8f, 1.2f);
	RefreshActiveCamera();
}

void UknightSettingsSubsystem::EndDisplayPreview()
{
	if (bDisplayPreviewActive)
	{
		bDisplayPreviewActive = false;
		RefreshActiveCamera();
	}
}

void UknightSettingsSubsystem::GetEffectiveDisplaySettings(float& FieldOfViewOffset, float& Brightness, float& Gamma)
{
	const UknightSharedSettings* const Settings = GetSharedSettings();
	FieldOfViewOffset = bDisplayPreviewActive ? PreviewFieldOfViewOffset : Settings ? Settings->FieldOfViewOffset : 0.f;
	Brightness = bDisplayPreviewActive ? PreviewBrightness : Settings ? Settings->Brightness : 1.f;
	Gamma = bDisplayPreviewActive ? PreviewGamma : Settings ? Settings->Gamma : 1.f;
}

void UknightSettingsSubsystem::RefreshActiveCamera()
{
	const ULocalPlayer* const Player = GetLocalPlayer();
	APlayerController* const Controller = Player ? Player->GetPlayerController(Player->GetWorld()) : nullptr;
	if (Controller && Controller->PlayerCameraManager)
	{
		// Paused menu worlds do not tick their camera; refresh the cached view and PP blends now.
		Controller->PlayerCameraManager->UpdateCamera(0.f);
	}
}
