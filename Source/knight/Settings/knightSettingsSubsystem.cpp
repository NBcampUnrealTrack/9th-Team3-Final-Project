#include "Settings/knightSettingsSubsystem.h"

#include "Engine/LocalPlayer.h"
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
		}
	}
	return SharedSettings;
}

void UknightSettingsSubsystem::SaveSharedSettings()
{
	if (SharedSettings)
	{
		SharedSettings->AsyncSaveGameToSlotForLocalPlayer();
	}
}
