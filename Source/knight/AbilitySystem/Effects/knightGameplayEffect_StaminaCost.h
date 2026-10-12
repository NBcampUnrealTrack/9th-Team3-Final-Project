#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "knightGameplayEffect_StaminaCost.generated.h"

/**
 * 행동 에너지(BoostGauge) 비용 (2026-10-09). 즉시 적용, BoostGauge += SetByCaller.BoostCost.
 * UknightGameplayAbility::ApplyCost가 어빌리티의 StaminaCost를 음수로 넣어 쓴다. 행동마다 비용 GE 에셋을 만들지 않으려고 하나로 둔다.
 * BoostGauge는 속성 세트에서 0 ~ 최대치로 잘리므로 남은 양보다 많이 빼도 0에서 멈춘다.
 */
UCLASS()
class KNIGHT_API UknightGameplayEffect_StaminaCost : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UknightGameplayEffect_StaminaCost(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
