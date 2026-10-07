#pragma once

#include "CoreMinimal.h"
#include "Combat/knightCombatTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
class UAnimMontage;
class UGameplayAbility;

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

	/**
	 * 공격 시작 시 모션 워프 타깃을 정한다. 락온 타깃이 있으면 그 지점, 없고 bUseSoftTarget이면 공격 방향
	 * (이동 입력, 없으면 정면) SoftTargetAngle 안, SoftTargetRange 안의 가장 가까운 적.
	 * 워프 위치는 타깃 표면(부위 지점이면 지점)에서 StopDistance 앞이며, 이동량은 MaxDistance로 제한한다.
	 * 이미 그보다 가까우면 앞으로 끌지 않고 방향만 돌린다. 타깃이 없으면 워프 타깃을 지우고 false.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Combat")
	static bool SetAttackWarpTarget(AActor* Attacker, FName WarpTargetName, float MaxDistance, float StopDistance,
		bool bUseSoftTarget, float SoftTargetRange, float SoftTargetAngle);

	/** SetAttackWarpTarget과 같은 규칙으로 워프 위치·방향·이동량만 계산한다 (워프 타깃은 바꾸지 않는다). */
	static bool ComputeAttackWarp(AActor* Attacker, float MaxDistance, float StopDistance, bool bUseSoftTarget,
		float SoftTargetRange, float SoftTargetAngle, FVector& OutLocation, FRotator& OutRotation, float& OutTravel);

	/**
	 * 대검 공격 애니메이션은 거의 제자리(루트 모션 수 cm)라 Skew Warp가 거리를 만들지 못한다. 그래서 몽타주의
	 * Motion Warping 구간이 끝날 때 Location에 도착하도록, 구간 끝 MaxLungeTime 동안 루트 모션 힘으로 이동시킨다.
	 * 회전은 몽타주의 워프 구간이 맡는다. Travel이 MinTravel보다 짧으면 아무것도 하지 않는다.
	 */
	static void StartAttackLunge(UGameplayAbility* Ability, const UAnimMontage* Montage, float PlayRate, const FVector& Location,
		float Travel, float MaxLungeTime = 0.35f, float MinTravel = 20.f);

	/** 몽타주의 첫 Motion Warping 구간이 끝나는 시간(초). 없으면 -1. */
	static float GetWarpWindowEnd(const UAnimMontage* Montage);

private:
	static void ApplyHitStopPair(AActor* Attacker, AActor* Target, float Duration);
};
