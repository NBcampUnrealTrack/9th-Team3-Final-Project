#pragma once

#include "CoreMinimal.h"
#include "Combat/knightCombatTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "knightCombatLibrary.generated.h"

/** 공용 전투 규칙. 모든 공격(플레이어, 몬스터, 투사체)이 같은 방식으로 타격을 처리한다. */
UCLASS()
class KNIGHT_API UknightCombatLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Attacker가 Hit.GetActor()에 가한 무기 타격 하나를 처리한다.
	 *   무적         -> Evaded, 아무것도 적용하지 않는다
	 *   패리 중      -> Parried: 공격자에게 Event.Parried, 패리 큐, 긴 히트 스톱, 피해 없음
	 *   그 외        -> 컨텍스트에 타격 결과를 담은 피해 이펙트(부위 배율 계산에 필요),
	 *                   타격 또는 약점 큐, 히트 스톱. 가드 중인 타깃에는 Event.Guarded
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Combat")
	static EknightHitOutcome ApplyMeleeHit(AActor* Attacker, const FHitResult& Hit, const FknightMeleeHitParams& Params);

	/**
	 * 스윕 타격을 액터당 하나로 줄인다. 순서는 액터가 처음 나온 순서를 따른다. 액터마다 부위로
	 * 판정되는(ICombatInterface) 첫 타격을 남기고, 없으면 첫 타격을 남긴다. 이 처리가 없으면 부위 바디와
	 * 겹쳐 있는 몬스터의 이동 캡슐이 먼저 걸리는 경우가 많아 타격이 부위 정보를 잃는다.
	 */
	static void SelectBestHitPerActor(TArrayView<const FHitResult> Hits, TArray<FHitResult>& OutHits);

	/** 모션 워프 타깃을 공격자가 락온한 타깃 쪽으로, MaxDistance 이내로 맞춘다. 락온이 없으면 false를 반환한다. */
	UFUNCTION(BlueprintCallable, Category="Knight|Combat")
	static bool SetWarpTargetToLockedTarget(AActor* Attacker, FName WarpTargetName, float MaxDistance);

private:
	static void ApplyHitStopPair(AActor* Attacker, AActor* Target, float Duration);
};
