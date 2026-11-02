#include "Monster/knightMonsterNearMissSubsystem.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterDefinition.h"
#include "TimerManager.h"
#include "knight.h"

namespace knightMonsterNearMiss
{
	/** 판정이 가까이 지나간 뒤 이만큼 기다려 그 사이 맞지 않았을 때만 낸다. 판정은 매 틱 움직이므로 몇 틱이면 충분하다. */
	constexpr double PendingTime = 0.1;
}

bool UknightMonsterNearMissSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UknightMonsterNearMissSubsystem::ReportNearMiss(const AknightMonsterCharacterBase& Monster, AActor& Target, const FVector& Location, bool bCertain)
{
	const UknightMonsterDefinition* const Definition = Monster.GetMonsterDefinition();
	UWorld* const World = GetWorld();
	if (!Definition || !Definition->NearMissSound || !World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	if (const double* const Last = LastPlayTime.Find(&Target); Last && Now - *Last < Definition->NearMissInterval)
	{
		return;
	}
	if (bCertain)
	{
		Pending.Remove(const_cast<AActor*>(&Target));
		Play(Monster, Target, Location);
		return;
	}
	if (Pending.Contains(&Target))
	{
		return;
	}
	Pending.Add(&Target, FPending{ &Monster, Location, Now + knightMonsterNearMiss::PendingTime });
	if (!World->GetTimerManager().IsTimerActive(FlushTimer))
	{
		World->GetTimerManager().SetTimer(FlushTimer, this, &ThisClass::FlushPending, 0.02f, true);
	}
}

void UknightMonsterNearMissSubsystem::ReportContact(const AActor& Target)
{
	Pending.Remove(const_cast<AActor*>(&Target));
}

void UknightMonsterNearMissSubsystem::Play(const AknightMonsterCharacterBase& Monster, const AActor& Target, const FVector& Location)
{
	const UknightMonsterDefinition* const Definition = Monster.GetMonsterDefinition();
	UWorld* const World = GetWorld();
	if (!Definition || !Definition->NearMissSound || !World)
	{
		return;
	}
	LastPlayTime.Add(const_cast<AActor*>(&Target), World->GetTimeSeconds());
	UGameplayStatics::PlaySoundAtLocation(World, Definition->NearMissSound, Location, Definition->NearMissVolume, Definition->NearMissPitch);
	UE_LOG(LogknightAI, Log, TEXT("%s: near miss on %s."), *GetNameSafe(&Monster), *GetNameSafe(&Target));
}

void UknightMonsterNearMissSubsystem::FlushPending()
{
	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	for (auto It = Pending.CreateIterator(); It; ++It)
	{
		const AActor* const Target = It.Key().Get();
		const AknightMonsterCharacterBase* const Monster = It.Value().Monster.Get();
		if (!Target || !Monster)
		{
			It.RemoveCurrent();
			continue;
		}
		if (Now >= It.Value().PlayTime)
		{
			const FVector Location = It.Value().Location;
			It.RemoveCurrent();
			Play(*Monster, *Target, Location);
		}
	}
	if (Pending.IsEmpty())
	{
		World->GetTimerManager().ClearTimer(FlushTimer);
	}
}
