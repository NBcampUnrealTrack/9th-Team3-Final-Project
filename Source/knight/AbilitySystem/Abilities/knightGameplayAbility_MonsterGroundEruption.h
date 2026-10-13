#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAreaAttack.h"
#include "knightGameplayAbility_MonsterGroundEruption.generated.h"

class UMaterialInterface;
class UNiagaraSystem;

/**
 * 몬스터 주변 바닥에서 불길이 솟는 범위 공격. 판정과 타이밍은 UknightGameplayAbility_MonsterAreaAttack 그대로이고,
 * 예고(WindupTime) 동안 범위 크기의 바닥 경고 원을 그리고, 타격 순간 범위 중앙에 ImpactEffects를, 범위 안 바닥
 * 여러 곳(ScatterCount)에 ScatterEffect를 터뜨린다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_MonsterGroundEruption : public UknightGameplayAbility_MonsterAreaAttack
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	virtual void OnImpact() override;

	/** 예고 동안 범위 중앙 바닥에 그리는 원(반지름 AreaRadius). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation")
	TObjectPtr<UMaterialInterface> WarningDecal;

	/** 타격 순간 범위 중앙 바닥에 터뜨리는 효과(충격파, 먼지). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation")
	TArray<TObjectPtr<UNiagaraSystem>> ImpactEffects;

	/** 타격 순간 범위 안 바닥 여러 곳에 터뜨리는 효과(솟구치는 불길). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation")
	TObjectPtr<UNiagaraSystem> ScatterEffect;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation", meta=(ClampMin=0))
	int32 ScatterCount = 12;

private:
	/** 범위 중앙(또는 그 주변 XY) 아래의 지형. 못 찾으면 몬스터 발밑 높이. */
	FVector FindGround(const FVector& Location) const;
};
