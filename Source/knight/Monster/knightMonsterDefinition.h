#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "knightMonsterDefinition.generated.h"

class UGameplayEffect;
class UknightAbilitySet;
class UknightAbilityTagRelationshipMapping;
class UknightMonsterPartData;
class UNiagaraSystem;

/**
 * Everything that defines a monster species, separate from its Blueprint (mesh, anim BP, StateTree).
 * Starting attributes (Health, AttackPower, Poise, Rage limits) come from an init effect in an ability set.
 */
UCLASS(BlueprintType, Const)
class KNIGHT_API UknightMonsterDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster")
	FText DisplayName;

	/** Attack moves, reactions and the attribute init effect. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Abilities")
	TArray<TObjectPtr<const UknightAbilitySet>> AbilitySets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Abilities")
	TObjectPtr<UknightAbilityTagRelationshipMapping> TagRelationshipMapping;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Parts")
	TObjectPtr<const UknightMonsterPartData> PartData;

	/** Applied when Rage fills: grants State.Monster.Enraged for a duration and resets Rage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="States")
	TSubclassOf<UGameplayEffect> EnrageEffect;

	/** Applied when Stamina runs out: grants State.Monster.Exhausted for a duration and restores Stamina. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="States")
	TSubclassOf<UGameplayEffect> ExhaustEffect;

	/**
	 * 퍼펙트 가드를 당할 때마다 깎이는 경직도. 최대 경직도에 대한 비율(0.25 = 4번에 다운). 0이면 깎지 않는다.
	 * 일반 타격의 경직도 피해와 같은 게이지를 쓰므로 섞어서 쌓인다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="States", meta=(ClampMin=0, ClampMax=1))
	float ParriedPoiseDamageRatio = 0.f;

	/** Rough radius for lock-on camera framing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Targeting", meta=(ClampMin=0, Units="cm"))
	float TargetRadius = 600.f;

	/** Lock-on point when PartData defines none. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Targeting")
	FName FallbackLockOnSocket = TEXT("spine_03");

	/** 공용 StateTree(ST_Monster_Default)의 추격이 멈추는 거리. 몸 크기에 맞춘다. Knight Monster Target 평가기가 내보낸다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI", meta=(ClampMin=0, Units="cm"))
	float ChaseStopDistance = 900.f;

	/** 공용 StateTree가 원거리 공격(Ability.Attack.Ranged)으로 넘어가는 거리 구간. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI", meta=(ClampMin=0, Units="cm"))
	float RangedMinDistance = 533.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI", meta=(ClampMin=0, Units="cm"))
	float RangedMaxDistance = 2000.f;

	/**
	 * 등장할 때 셰이더 파이프라인(PSO)을 미리 준비해 둘 이펙트(브레스, 땅울림 먼지). 처음 쓰일 때 셰이더를 즉석에서 만드느라
	 * 한 프레임이 튀는 것을 막는다. 몽타주 노티파이로 처음 등장하는 Niagara를 넣는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Presentation")
	TArray<TObjectPtr<UNiagaraSystem>> PrewarmEffects;
};
