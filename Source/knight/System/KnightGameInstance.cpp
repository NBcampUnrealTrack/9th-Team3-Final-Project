#include "System/KnightGameInstance.h"

#include "GameFlow/KnightGameFlowSubsystem.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Progression/knightSaveSubsystem.h"
#include "Raid/knightRaidSubsystem.h"

void UKnightGameInstance::Init()
{
	Super::Init();
	GetSubsystem<UknightSaveSubsystem>();
	GetSubsystem<UknightRaidSubsystem>();
	GetSubsystem<UknightProgressionSubsystem>();
	GetSubsystem<UKnightGameFlowSubsystem>();
}
