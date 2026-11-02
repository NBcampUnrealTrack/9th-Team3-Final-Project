#pragma once

#include "CoreMinimal.h"
#include "Combat/knightCombatTypes.h"
#include "Monster/knightMonsterGuardPushComponent.h"
#include "GameFramework/Actor.h"
#include "knightMonsterSonicProjectile.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UStaticMeshComponent;

/** 음파가 끝나는 모습. 비어 있는 칸은 아무것도 남기지 않고 사라진다. 로컬 +X가 음파가 날아온 방향이다. */
USTRUCT(BlueprintType)
struct FknightSonicImpactEffects
{
	GENERATED_BODY()

	/** 대상이 맞았을 때(파편이 튀고 돌풍으로 흩어짐). */
	UPROPERTY(EditDefaultsOnly, Category="Sonic")
	TObjectPtr<UNiagaraSystem> Hit;

	/** 가드에 막혔을 때(좌우로 갈라져 흘러감). */
	UPROPERTY(EditDefaultsOnly, Category="Sonic")
	TObjectPtr<UNiagaraSystem> Guarded;

	/** 패링됐을 때(깨끗한 링 하나). */
	UPROPERTY(EditDefaultsOnly, Category="Sonic")
	TObjectPtr<UNiagaraSystem> Parried;

	/** 벽·바닥에 닿거나 사거리 끝에서 흩어질 때. */
	UPROPERTY(EditDefaultsOnly, Category="Sonic")
	TObjectPtr<UNiagaraSystem> World;

	/** 물에 닿으면 World 대신 수면에서 이 물보라를 터뜨리고 물결을 낸다. 비어 있으면 Knight Water Ripple 설정의 SplashHeavySystem을 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category="Sonic")
	TObjectPtr<UNiagaraSystem> WaterSplash;

	UPROPERTY(EditDefaultsOnly, Category="Sonic", meta=(ClampMin=0))
	float WaterSplashScale = 1.f;

	/** 수면을 누르는 반지름과 깊이(cm). 0이면 물결을 내지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Sonic", meta=(ClampMin=0, Units="cm"))
	float WaterRippleRadius = 400.f;

	UPROPERTY(EditDefaultsOnly, Category="Sonic", meta=(ClampMin=0, Units="cm"))
	float WaterRippleDepth = 30.f;

	/** 음파 가장자리가 플레이어 몸 중심에서 이 거리 안을 맞히지 않고 지나가면 그 플레이어 화면 가장자리가 짧게 갈라진다. 0이면 끈다. */
	UPROPERTY(EditDefaultsOnly, Category="Sonic", meta=(ClampMin=0, Units="cm"))
	float NearMissDistance = 400.f;
};

/**
 * 몬스터가 쏘는 직선 투사체(잔향 초음파). 정해진 방향으로 Speed만큼 날아가며, 매 틱 앞 구간을 구체로 훑어 처음 닿은 적대 대상을
 * 근접 타격과 같은 규칙(UknightCombatLibrary::ApplyMeleeHit: 가드·패링·회피)으로 한 번 때리고 사라진다. 벽·바닥에 닿거나
 * MaxDistance를 날면 사라진다. 대상을 통과하지 않는다.
 *
 * 끝날 때는 결과(맞음·가드·패링·벽·물)에 맞는 ImpactEffects를 그 자리에 남긴다. 회피로 통과한 대상에는 남기지 않는다.
 *
 * DeafenTime이 있으면 맞은(가드·패링·회피가 아닌) 플레이어의 소리가 줄었다가 그 시간 동안 돌아온다.
 *
 * 외형은 선택 사항이다: Effect(나이아가라)가 있으면 스케일 1인 루트에 붙여 재생하고(로컬 +X가 날아가는 방향), 없으면 판정
 * 크기의 기본 구체 메시를 보인다.
 * 디버그: knight.Monster.DrawAttacks 1 이면 판정 구체를 그린다.
 */
UCLASS()
class KNIGHT_API AknightMonsterSonicProjectile : public AActor
{
	GENERATED_BODY()

public:
	AknightMonsterSonicProjectile();

	void Launch(AActor* InAttacker, const FknightMeleeHitParams& InHitParams, const FVector& Direction, float InSpeed, float InRadius,
		float InMaxDistance, UNiagaraSystem* Effect);

	/** Launch 전에 부른다. 0이면 끈다. */
	void SetDeafenTime(float Seconds) { DeafenTime = Seconds; }

	/** Launch 전에 부른다. 가드로 막혔을 때의 밀림 단계(쏜 공격의 GuardReaction). */
	void SetGuardReaction(EknightMonsterGuardReaction Reaction) { GuardReaction = Reaction; }

	/** Launch 전에 부른다. */
	void SetImpactEffects(const FknightSonicImpactEffects& Effects) { ImpactEffects = Effects; }

	virtual void Tick(float DeltaSeconds) override;

private:
	void DeafenTarget(AActor& Target) const;
	/** Direction은 이펙트 방향(이펙트는 이 방향에 수직으로 퍼진다). */
	void SpawnImpact(EknightHitOutcome Outcome, const FVector& Location, const FVector& Direction) const;
	/** Location이 수면에 닿았으면 물보라와 물결을 내고 true. */
	bool SpawnWaterImpact(const FVector& Location) const;
	/** 이번 틱에 지나간 구간(Start→End)이 맞히지 않은 플레이어 곁을 스쳤으면 그 화면에 알린다. bEnding이면 이번 틱에 사라진다. */
	void CheckNearMiss(const FVector& Start, const FVector& End, const AActor* HitTarget, bool bEnding);

	UPROPERTY(VisibleAnywhere, Category="Sonic")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category="Sonic")
	TObjectPtr<UNiagaraComponent> EffectComponent;

	TWeakObjectPtr<AActor> Attacker;
	FknightMeleeHitParams HitParams;
	EknightMonsterGuardReaction GuardReaction = EknightMonsterGuardReaction::Light;
	TSet<TWeakObjectPtr<AActor>> HitActors;
	FVector Velocity = FVector::ZeroVector;
	float Radius = 100.f;
	float RemainingDistance = 0.f;
	float DeafenTime = 0.f;

	UPROPERTY()
	FknightSonicImpactEffects ImpactEffects;

	TSet<TWeakObjectPtr<const AActor>> NearMissed;
};
