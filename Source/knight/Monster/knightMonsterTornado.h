#pragma once

#include "CoreMinimal.h"
#include "Combat/knightCombatTypes.h"
#include "GameFramework/Actor.h"
#include "knightMonsterTornado.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UStaticMeshComponent;

/** 회오리 하나의 크기·움직임. 몬스터 어빌리티가 정해 AknightMonsterTornado::Launch에 넘긴다. */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightMonsterTornadoParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado", meta=(ClampMin=1, Units="cm"))
	float Radius = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado", meta=(ClampMin=1, Units="cm"))
	float Height = 1200.f;

	/** 생긴 자리에서 다 커질 때까지. 커지는 동안은 움직이지 않고, 판정도 크기만큼 작다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado", meta=(ClampMin=0, Units="s"))
	float GrowTime = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado", meta=(ClampMin=0, Units="s"))
	float ChaseTime = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado", meta=(ClampMin=0, Units="cm/s"))
	float Speed = 800.f;

	/** 타깃 쪽으로 진행 방향을 꺾는 속도. 작을수록 옆으로 피하기 쉽다. 0이면 처음 방향으로 일직선으로 지나간다(잔향). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado", meta=(ClampMin=0, Units="deg"))
	float TurnRate = 90.f;

	/** 쫓기가 끝난 뒤 줄어들며 사라지는 시간. 판정은 없다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado", meta=(ClampMin=0, Units="s"))
	float FadeTime = 0.5f;

	/** 선택 사항. 없으면 거꾸로 선 기본 원뿔이 보인다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado")
	TObjectPtr<UNiagaraSystem> Effect;
};

/**
 * 몬스터가 바닥에 만드는 회오리(잔향 공중 4). 생긴 자리에서 GrowTime 동안 커진 뒤 ChaseTime 동안 Speed로 바닥을 따라 타깃을 쫓고,
 * FadeTime 동안 줄어들며 사라진다. 원기둥(Radius·Height) 안에 들어온 적대 대상을 근접 타격과 같은 규칙(UknightCombatLibrary::ApplyMeleeHit)으로
 * 때린다. 맞은 대상은 이 회오리에 다시 맞지 않는다. 회피(무적)로 피한 대상은 무적이 끝난 뒤 아직 안에 있으면 맞는다.
 * 막을 수 없게 하려면 가드 범위가 0인 피해 실행(UknightDamageExecution::GuardArcDegrees)을 쓴다.
 * 공격한 몬스터가 죽으면 바로 사라지기 시작한다.
 * 디버그: knight.Monster.DrawAttacks 1 이면 판정 원기둥을 그린다.
 */
UCLASS()
class KNIGHT_API AknightMonsterTornado : public AActor
{
	GENERATED_BODY()

public:
	AknightMonsterTornado();

	/** Direction은 쫓기 시작할 때의 진행 방향(수평). */
	void Launch(AActor* InAttacker, AActor* InTarget, const FknightMeleeHitParams& InHitParams, const FknightMonsterTornadoParams& InParams,
		const FVector& Direction);

	virtual void Tick(float DeltaSeconds) override;

private:
	void ApplyHits(float Scale);
	void UpdateVisual(float Scale, float DeltaSeconds);
	/** 발밑 바닥 높이로 붙인다. 바닥을 못 찾으면 그대로 둔다. */
	void SnapToFloor(FVector& Location) const;

	UPROPERTY(VisibleAnywhere, Category="Tornado")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category="Tornado")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category="Tornado")
	TObjectPtr<UNiagaraComponent> EffectComponent;

	TWeakObjectPtr<AActor> Attacker;
	TWeakObjectPtr<AActor> Target;
	FknightMeleeHitParams HitParams;
	FknightMonsterTornadoParams Params;
	TSet<TWeakObjectPtr<AActor>> HitActors;
	FVector MoveDirection = FVector::ForwardVector;
	float Age = 0.f;
	/** 사라지기 시작한 나이. 음수면 아직 쫓는 중이다. */
	float FadeStartAge = -1.f;
	float Spin = 0.f;
};
