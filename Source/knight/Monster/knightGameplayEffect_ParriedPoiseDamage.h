#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "knightGameplayEffect_ParriedPoiseDamage.generated.h"

/**
 * 퍼펙트 가드를 당한 몬스터가 자기에게 거는 경직도 피해. 즉시 적용, PoiseDamage += SetByCaller.PoiseDamage.
 * 피해 계산(UknightDamageExecution)을 거치지 않으므로 부위 배율·방어력과 무관하게 넣은 값 그대로 깎인다.
 * 경직도가 0이 되면 일반 타격과 똑같이 Event.PoiseBroken(다운)이 나간다.
 */
UCLASS()
class KNIGHT_API UknightGameplayEffect_ParriedPoiseDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UknightGameplayEffect_ParriedPoiseDamage(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
