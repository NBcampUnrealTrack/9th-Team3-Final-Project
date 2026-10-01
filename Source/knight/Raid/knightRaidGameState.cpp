#include "Raid/knightRaidGameState.h"

#include "Engine/World.h"
#include "Raid/knightRaidDefinition.h"
#include "knight.h"

bool AknightRaidGameState::ShouldShowLoadingScreen(FString& OutReason) const
{
	if (RaidPhase == EknightRaidPhase::None)
	{
		OutReason = TEXT("Raid is starting");
		return true;
	}
	return false;
}

float AknightRaidGameState::GetElapsedRaidTime() const
{
	if (RaidClockStartTime < 0.0 || !GetWorld())
	{
		return 0.f;
	}
	const double End = RaidClockStopTime >= 0.0 ? RaidClockStopTime : GetWorld()->GetTimeSeconds();
	return static_cast<float>(End - RaidClockStartTime);
}

float AknightRaidGameState::GetRemainingRaidTime() const
{
	if (!RaidDefinition || RaidDefinition->TimeLimit <= 0.f)
	{
		return -1.f;
	}
	return FMath::Max(0.f, RaidDefinition->TimeLimit - GetElapsedRaidTime());
}

void AknightRaidGameState::SetRaidPhase(EknightRaidPhase NewPhase)
{
	if (RaidPhase == NewPhase)
	{
		return;
	}
	const EknightRaidPhase OldPhase = RaidPhase;
	RaidPhase = NewPhase;
	UE_LOG(Logknight, Log, TEXT("Raid phase %s -> %s"), *UEnum::GetValueAsString(OldPhase), *UEnum::GetValueAsString(NewPhase));
	OnRaidPhaseChanged.Broadcast(OldPhase, NewPhase);
}

void AknightRaidGameState::StartRaidClock()
{
	RaidClockStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	RaidClockStopTime = -1.0;
}

void AknightRaidGameState::StopRaidClock()
{
	if (RaidClockStartTime >= 0.0 && RaidClockStopTime < 0.0 && GetWorld())
	{
		RaidClockStopTime = GetWorld()->GetTimeSeconds();
	}
}
