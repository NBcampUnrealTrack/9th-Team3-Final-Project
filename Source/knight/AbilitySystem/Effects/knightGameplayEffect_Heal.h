#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "knightGameplayEffect_Heal.generated.h"

/**
 * 체력 회복(수리 키트). 즉시 적용, Healing(메타 속성) += SetByCaller.Healing.
 * Core 속성 세트가 Healing을 Health 증가로 바꾼다(최대 체력까지, 사망 상태면 무시).
 * 부위 체력은 이 이펙트가 아니라 UknightPartHealthComponent::ApplyRepair가 따로 채운다.
 */
UCLASS()
class KNIGHT_API UknightGameplayEffect_Heal : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UknightGameplayEffect_Heal(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
