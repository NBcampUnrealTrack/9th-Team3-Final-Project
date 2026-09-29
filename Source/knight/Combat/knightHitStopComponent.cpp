#include "Combat/knightHitStopComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

UknightHitStopComponent::UknightHitStopComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UknightHitStopComponent::ApplyHitStop(float Duration)
{
	AActor* const Owner = GetOwner();
	UWorld* const World = GetWorld();
	if (!Owner || !World || Duration <= 0.f)
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	if (bActive)
	{
		// Extend only; a short hit never cuts a longer freeze short.
		if (TimerManager.GetTimerRemaining(HitStopTimer) >= Duration)
		{
			return;
		}
	}
	else
	{
		RestoreTimeDilation = Owner->CustomTimeDilation;
		Owner->CustomTimeDilation = HitStopTimeDilation;
		bActive = true;
	}
	// World timers run on world time, which the owner's CustomTimeDilation does not affect.
	TimerManager.SetTimer(HitStopTimer, this, &ThisClass::StopHitStop, Duration, false);
}

void UknightHitStopComponent::StopHitStop()
{
	if (!bActive)
	{
		return;
	}
	bActive = false;
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitStopTimer);
	}
	if (AActor* const Owner = GetOwner())
	{
		Owner->CustomTimeDilation = RestoreTimeDilation;
	}
}

void UknightHitStopComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopHitStop();
	Super::EndPlay(EndPlayReason);
}
