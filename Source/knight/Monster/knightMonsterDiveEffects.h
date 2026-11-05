#pragma once

#include "CoreMinimal.h"
#include "knightMonsterDiveEffects.generated.h"

class ACharacter;
class UknightAttackTrailComponent;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;

/** 돌진 동안 뼈 두 개를 따라 남기는 궤적 하나(Knight Attack Trail 노티파이와 같은 값). */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightDiveTrail
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Trail")
	FName BaseSocket;

	UPROPERTY(EditAnywhere, Category="Trail")
	FName TipSocket;

	UPROPERTY(EditAnywhere, Category="Trail")
	TObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, Category="Trail", meta=(ClampMin=0, ClampMax=1))
	float BaseRatio = 0.f;

	UPROPERTY(EditAnywhere, Category="Trail", meta=(Units="cm"))
	float TipExtension = 0.f;

	UPROPERTY(EditAnywhere, Category="Trail", meta=(ClampMin=0.01, Units="s"))
	float Lifetime = 0.4f;

	UPROPERTY(EditAnywhere, Category="Trail", meta=(ClampMin=0, Units="s"))
	float FadeIn = 0.08f;
};

/**
 * 급강하 돌진(잔향 다이빙) 연출. 어빌리티가 단계마다 부른다.
 * - 접기(StartFold): 가슴으로 공기가 빨려 드는 예고
 * - 출발(StartDive): 몸 뒤로 퍼지는 소닉붐 고리 한 번, 머리 앞 압축 파면·몸 둘레 바람 줄기·날개 끝과 꼬리 궤적을 켠다
 * - 돌진 중(Tick): 파면을 날아가는 방향으로 돌리고, 머리가 수면 가까이 내려오면 몸 아래 수면을 가르는 물보라를 켠다
 * - 끝(FinishDive): 붙어 있던 연출을 끄고, 머리 아래 바닥·수면에 충격파 고리를 남긴다
 * 붙는 효과는 몸을 가리지 않게 옅게 만든 것을 쓴다.
 */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightMonsterDiveEffects
{
	GENERATED_BODY()

	/** 날개를 접는 동안 FoldSocket에 붙는 예고(절대 스케일 1). */
	UPROPERTY(EditAnywhere, Category="Effects|Fold")
	TObjectPtr<UNiagaraSystem> FoldEffect;

	UPROPERTY(EditAnywhere, Category="Effects|Fold")
	FName FoldSocket = TEXT("spine_02");

	/** 출발 순간 LaunchSocket 자리에 한 번 스폰한다. 로컬 +X가 날아가는 방향이다. */
	UPROPERTY(EditAnywhere, Category="Effects|Launch")
	TObjectPtr<UNiagaraSystem> LaunchEffect;

	UPROPERTY(EditAnywhere, Category="Effects|Launch")
	FName LaunchSocket = TEXT("spine_02");

	/** 돌진 동안 머리 앞에 붙는 압축 파면. 로컬 +X를 날아가는 방향으로 계속 맞춘다. */
	UPROPERTY(EditAnywhere, Category="Effects|Dive")
	TObjectPtr<UNiagaraSystem> BowEffect;

	UPROPERTY(EditAnywhere, Category="Effects|Dive")
	FName BowSocket = TEXT("head");

	/** 머리에서 날아가는 방향으로 앞당기는 거리(월드 cm). */
	UPROPERTY(EditAnywhere, Category="Effects|Dive", meta=(Units="cm"))
	float BowForward = 150.f;

	/** 돌진 동안 캡슐(스케일 1)에 붙는 바람 줄기. 로컬 -X로 흐른다. */
	UPROPERTY(EditAnywhere, Category="Effects|Dive")
	TObjectPtr<UNiagaraSystem> StreakEffect;

	UPROPERTY(EditAnywhere, Category="Effects|Dive")
	FVector StreakOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category="Effects|Dive")
	TArray<FknightDiveTrail> Trails;

	/** 머리가 수면 위 SkimHeight 안으로 내려오면 수면에 붙여 켜는 물보라. 로컬 +X가 지나가는 방향이다. */
	UPROPERTY(EditAnywhere, Category="Effects|Water")
	TObjectPtr<UNiagaraSystem> SkimEffect;

	UPROPERTY(EditAnywhere, Category="Effects|Water", meta=(ClampMin=0, Units="cm"))
	float SkimHeight = 600.f;

	/** 물보라 자리 수면을 누르는 물결(물결이 켜진 레벨에서만). 0이면 끈다. */
	UPROPERTY(EditAnywhere, Category="Effects|Water", meta=(ClampMin=0, Units="cm"))
	float SkimRippleRadius = 250.f;

	UPROPERTY(EditAnywhere, Category="Effects|Water", meta=(ClampMin=0, Units="cm"))
	float SkimRippleDepth = 15.f;

	UPROPERTY(EditAnywhere, Category="Effects|Water", meta=(ClampMin=0.02, Units="s"))
	float SkimRippleInterval = 0.08f;

	/** 돌진이 끝나는 순간 머리 아래 바닥(또는 수면)에 한 번 스폰한다. 로컬 +X가 아래를 향하게 눕혀 고리가 바닥을 따라 퍼진다. */
	UPROPERTY(EditAnywhere, Category="Effects|Impact")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	/** 머리가 바닥·수면에서 이보다 높으면 충격파를 남기지 않는다. */
	UPROPERTY(EditAnywhere, Category="Effects|Impact", meta=(ClampMin=0, Units="cm"))
	float ImpactMaxHeight = 800.f;

	/** 내려꽂힌 자리에서 화면에 퍼지는 충격파 세기(1 = 기본, 0 = 없음). */
	UPROPERTY(EditAnywhere, Category="Effects|Impact", meta=(ClampMin=0, ClampMax=3))
	float ImpactScreenShockwave = 1.2f;

	void StartFold(ACharacter& Character);
	void StartDive(ACharacter& Character, const FVector& Direction);
	void Tick(ACharacter& Character, float DeltaTime);
	/** bImpact가 false면(패링으로 공중에서 끊겨 바닥에 닿지 않았을 때) 착지 충격파 없이 연출을 끄고, 출발 지점의 소닉 붐 고리도 바로 지운다. */
	void FinishDive(ACharacter& Character, bool bImpact = true);
	/** 남은 연출을 모두 끈다(어빌리티가 끝날 때). */
	void Stop();

private:
	TWeakObjectPtr<UNiagaraComponent> Fold;
	TWeakObjectPtr<UNiagaraComponent> Boom;
	TWeakObjectPtr<UNiagaraComponent> Bow;
	TWeakObjectPtr<UNiagaraComponent> Streak;
	TWeakObjectPtr<UNiagaraComponent> Skim;
	TArray<TWeakObjectPtr<UknightAttackTrailComponent>> ActiveTrails;
	FVector DiveDirection = FVector::ForwardVector;
	float RippleTimer = 0.f;

	void StopDiveEffects();
	/** 머리 아래 바닥·수면 높이. 둘 다 있으면 높은 쪽(물 위를 날면 수면). */
	bool FindSurfaceBelow(const ACharacter& Character, const FVector& From, FVector& OutSurface, bool& bOutWater) const;
};
