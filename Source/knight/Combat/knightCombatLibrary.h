#pragma once

#include "CoreMinimal.h"
#include "CollisionShape.h"
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
	 *                   (Params.bCanBeParried가 false이거나 공격이 가드 범위(정면) 밖에서 들어오면 패리 대신 아래 경로로 처리.
	 *                    범위 밖이면 피해 계산에서 가드 감소도 없다 — 2026-10-08)
	 *   그 외        -> 컨텍스트에 타격 결과를 담은 피해 이펙트(부위 배율 계산에 필요),
	 *                   타격 또는 약점 큐, 히트 스톱. 가드 중인 타깃에는 Event.Guarded
	 *                   (2026-10-12: 가드 범위 안에서 막았을 때만. 범위 밖에서 들어오면 가드 중이어도 일반 피격.
	 *                    방향은 Hit.TraceStart(공격이 들어온 지점)에서 재고, 없으면 공격자 위치)
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Combat")
	static EknightHitOutcome ApplyMeleeHit(AActor* Attacker, const FHitResult& Hit, const FknightMeleeHitParams& Params);
	/** C++ attack path carries its active ability so a pre-hit prediction only resolves for that same attack. */
	static EknightHitOutcome ApplyMeleeHit(AActor* Attacker, const FHitResult& Hit, const FknightMeleeHitParams& Params,
		const UGameplayAbility* SourceAbility);

	/**
	 * Shape(중심 Center) 안의 적대 ICombatInterface 액터마다 ApplyMeleeHit을 한 번씩 적용한다(몬스터 범위·몸통·투사체 판정).
	 * HitActors에 있는 액터와 Filter가 false인 액터는 건너뛰고, 새로 맞은 액터는 HitActors에 넣는다. 히트 결과는 Center에서
	 * 가장 가까운 표면 지점으로 만들며, 법선은 FixedNormal이 0이 아니면 그것, 아니면 표면에서 Center 쪽이다.
	 * OnHit은 적용 직후 결과와 함께 불린다. 새로 맞은 수를 반환한다.
	 */
	static int32 ApplyAreaMeleeHits(AActor* Attacker, const FVector& Center, const FCollisionShape& Shape, const FknightMeleeHitParams& Params,
		TSet<TWeakObjectPtr<AActor>>& HitActors, TFunctionRef<bool(const AActor&)> Filter = [](const AActor&) { return true; },
		TFunctionRef<void(AActor&, EknightHitOutcome)> OnHit = [](AActor&, EknightHitOutcome) {}, const FVector& FixedNormal = FVector::ZeroVector);

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
