#pragma once

#include "CoreMinimal.h"
#include "Combat/knightCombatTypes.h"
#include "Monster/knightMonsterGuardPushComponent.h"
#include "GameFramework/Actor.h"
#include "knightMonsterSpikeProjectile.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * 한 번의 가시 비에서 대상마다 마지막으로 맞은 시각. 같은 발사에서 나온 가시들이 나눠 쓴다.
 * RehitInterval이 0보다 작으면 대상마다 한 번만 맞고, 0 이상이면 마지막으로 맞은 뒤 그 시간이 지나야 다시 맞는다.
 */
struct FknightSpikeVolleyHits
{
	TMap<TWeakObjectPtr<AActor>, double> LastHitTime;
	float RehitInterval = -1.f;
};

/** 가시 외형. 원뿔 메시 두 개를 밑면끼리 붙여 가운데가 두껍고 양끝이 뾰족한 가시를 만든다. */
struct FknightSpikeLook
{
	/** 끝이 +Z를 향하는 원뿔(엔진 기본 원뿔). */
	TObjectPtr<UStaticMesh> Mesh;
	TObjectPtr<UMaterialInterface> Material;
	/** 원뿔 하나의 스케일. 가시 전체 길이는 원뿔 높이 x Z x 2다. */
	FVector Scale = FVector::OneVector;
	/** 떨어지는 가시에 붙여 꽂혀 있는 동안까지 재생하는 효과(뜨거운 가시의 불길). 위가 +Z인 채로 붙는다. */
	TObjectPtr<UNiagaraSystem> Effect;
};

/** 떨어지는 가시의 착탄 설정. */
struct FknightSpikeImpact
{
	FVector Landing = FVector::ZeroVector;
	float HitRadius = 150.f;
	/** 착탄점에서 위로 이 높이까지(낙하 경로)에 있는 대상이 맞는다. */
	float PathHeight = 400.f;
	TArray<TObjectPtr<UNiagaraSystem>> Effects;
	/** 떨어지기 시작할 때 가시에 붙여 재생하는 소리(바람 가르는 소리). 비우면 소리 없음. */
	TObjectPtr<USoundBase> FallSound;
	/** 착탄점에서 재생하는 소리. 비우면 소리 없음. */
	TObjectPtr<USoundBase> ImpactSound;
	/** 착탄 뒤 땅에 꽂혀 있는 시간. */
	float StuckTime = 1.5f;
};

/**
 * 몬스터가 쏘는 가시 하나. 두 가지로 쓴다(충돌 없음).
 * - LaunchUp: 위로 곧게 솟아 Duration 뒤에 사라진다(연출만).
 * - LaunchDown: 착탄점 위에서 수직으로 꽂힌다. 대상에 걸리지 않고 통과해 항상 땅에 박히며, 착탄 순간 낙하 경로
 *   (반지름 HitRadius, 높이 PathHeight 기둥) 안의 적대 대상을 근접 타격과 같은 규칙(UknightCombatLibrary::ApplyMeleeHit)으로 때린다.
 * 디버그: knight.Monster.DrawAttacks 1 이면 착탄 판정 캡슐을 그린다.
 */
UCLASS()
class KNIGHT_API AknightMonsterSpikeProjectile : public AActor
{
	GENERATED_BODY()

public:
	AknightMonsterSpikeProjectile();

	void SetLook(const FknightSpikeLook& Look);
	void LaunchUp(float Speed, float Duration);
	/** Launch 전에 부른다. 가드로 막혔을 때의 밀림 단계(쏜 공격의 GuardReaction). */
	void SetGuardReaction(EknightMonsterGuardReaction Reaction) { GuardReaction = Reaction; }
	void LaunchDown(AActor* InAttacker, const FknightMeleeHitParams& InHitParams, const FknightSpikeImpact& InImpact, float FallTime,
		const TSharedPtr<FknightSpikeVolleyHits>& InVolleyHits);

	virtual void Tick(float DeltaSeconds) override;

private:
	void Land();

	UPROPERTY(VisibleAnywhere, Category="Spike")
	TObjectPtr<USceneComponent> Root;

	/** 가시 앞쪽 절반(+X로 뾰족). */
	UPROPERTY(VisibleAnywhere, Category="Spike")
	TObjectPtr<UStaticMeshComponent> FrontHalf;

	/** 가시 뒤쪽 절반(-X로 뾰족). */
	UPROPERTY(VisibleAnywhere, Category="Spike")
	TObjectPtr<UStaticMeshComponent> BackHalf;

	TWeakObjectPtr<AActor> Attacker;
	FknightMeleeHitParams HitParams;
	EknightMonsterGuardReaction GuardReaction = EknightMonsterGuardReaction::Light;
	FknightSpikeImpact Impact;
	TObjectPtr<UNiagaraSystem> SpikeEffect;
	TSharedPtr<FknightSpikeVolleyHits> VolleyHits;
	FVector Velocity = FVector::ZeroVector;
	float Remaining = 0.f;
	bool bFalling = false;
	bool bLanded = false;
};
