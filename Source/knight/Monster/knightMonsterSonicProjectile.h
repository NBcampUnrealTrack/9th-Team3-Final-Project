#pragma once

#include "CoreMinimal.h"
#include "Combat/knightCombatTypes.h"
#include "GameFramework/Actor.h"
#include "knightMonsterSonicProjectile.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UStaticMeshComponent;

/**
 * 몬스터가 쏘는 직선 투사체(잔향 초음파). 정해진 방향으로 Speed만큼 날아가며, 매 틱 앞 구간을 구체로 훑어 처음 닿은 적대 대상을
 * 근접 타격과 같은 규칙(UknightCombatLibrary::ApplyMeleeHit: 가드·패링·회피)으로 한 번 때리고 사라진다. 벽·바닥에 닿거나
 * MaxDistance를 날면 사라진다. 대상을 통과하지 않는다.
 *
 * DeafenTime이 있으면 맞은(가드·패링·회피가 아닌) 플레이어의 소리가 줄었다가 그 시간 동안 돌아온다.
 *
 * 외형은 선택 사항이다: Effect(나이아가라)가 있으면 붙여 재생하고, 없으면 기본 구체 메시를 보인다.
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

	virtual void Tick(float DeltaSeconds) override;

private:
	void DeafenTarget(AActor& Target) const;

	UPROPERTY(VisibleAnywhere, Category="Sonic")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category="Sonic")
	TObjectPtr<UNiagaraComponent> EffectComponent;

	TWeakObjectPtr<AActor> Attacker;
	FknightMeleeHitParams HitParams;
	TSet<TWeakObjectPtr<AActor>> HitActors;
	FVector Velocity = FVector::ZeroVector;
	float Radius = 100.f;
	float RemainingDistance = 0.f;
	float DeafenTime = 0.f;
};
