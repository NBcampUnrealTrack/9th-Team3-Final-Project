#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "knightGameplayEffect_RageGain.generated.h"

/**
 * 피해를 받은 몬스터가 자기에게 거는 분노 증가. 즉시 적용, Rage += SetByCaller.Rage.
 * 효과로 올려야 PostGameplayEffectExecute에서 Rage가 가득 찬 것(OnRageMaxed → EnrageEffect)을 감지한다.
 */
UCLASS()
class KNIGHT_API UknightGameplayEffect_RageGain : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UknightGameplayEffect_RageGain(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
