#include "Combat/knightGameplayCueNotify_Hit.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"

bool UknightGameplayCueNotify_Hit::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
	const APawn* const TargetPawn = Cast<APawn>(Target);
	if (!bUsePlayerHitEffects || !TargetPawn || !TargetPawn->IsPlayerControlled())
	{
		return Super::OnExecute_Implementation(Target, Parameters);
	}

	// 기본 Burst 큐와 같은 순서로, 이펙트만 플레이어용으로 바꿔서 낸다.
	UWorld* const World = Target ? Target->GetWorld() : GetWorld();
	FGameplayCueNotify_SpawnContext SpawnContext(World, Target, Parameters);
	SpawnContext.SetDefaultSpawnCondition(&DefaultSpawnCondition);
	SpawnContext.SetDefaultPlacementInfo(&DefaultPlacementInfo);
	if (DefaultSpawnCondition.ShouldSpawn(SpawnContext))
	{
		FGameplayCueNotify_SpawnResult SpawnResult;
		PlayerHitEffects.ExecuteEffects(SpawnContext, SpawnResult);
		OnBurst(Target, Parameters, SpawnResult);
	}
	return false;
}
