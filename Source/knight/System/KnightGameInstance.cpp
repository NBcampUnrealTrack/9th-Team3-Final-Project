#include "System/KnightGameInstance.h"

#include "GameFlow/KnightGameFlowSubsystem.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Progression/knightSaveSubsystem.h"
#include "Raid/knightRaidSubsystem.h"
#include "Settings/knightGameUserSettings.h"
#include "knight.h"
#include "CoreGlobals.h"

void UKnightGameInstance::Init()
{
	Super::Init();
	if (!GIsEditor)
	{
		// Development packages keep file logs, but should not cover the game HUD with
		// Print String, on-screen debug messages, or engine warning overlays.
		GAreScreenMessagesEnabled = false;

		if (UknightGameUserSettings* const Graphics = UknightGameUserSettings::Get())
		{
			const bool bHadPendingGraphics = Graphics->HasPendingGraphics();
			const bool bApplied = Graphics->ApplyPendingGraphicsAtStartup();
			UE_LOG(Logknight, Log, TEXT("Game settings startup: pending=%d applied=%d"), bHadPendingGraphics, bApplied);
		}
	}
	GetSubsystem<UknightSaveSubsystem>();
	GetSubsystem<UknightRaidSubsystem>();
	GetSubsystem<UknightProgressionSubsystem>();
	GetSubsystem<UKnightGameFlowSubsystem>();
}
