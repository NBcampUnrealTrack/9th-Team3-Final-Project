#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Burst.h"
#include "knightGameplayCueNotify_Hit.generated.h"

/**
 * 피격 큐 (2026-10-12). 기본 Burst 큐와 같지만, 맞은 쪽(큐의 Target)이 플레이어가 조종하는 폰이면
 * BurstEffects 대신 PlayerHitEffects(이펙트·소리·흔들림)를 낸다. "내가 맞았다"를 때릴 때와 다르게 보여 주려는 것.
 * 공용 타격 함수(UknightCombatLibrary::ApplyMeleeHit)는 그대로 두고, 큐 쪽에서만 나눈다.
 * GCN_Hit_Melee / GCN_Hit_Weakpoint / GCN_Guard의 부모를 이 클래스로 바꿔 쓴다. bUsePlayerHitEffects가 꺼져 있으면 기본 큐와 같다.
 */
UCLASS(Blueprintable, meta=(DisplayName="Knight GCN Hit (Burst)"))
class KNIGHT_API UknightGameplayCueNotify_Hit : public UGameplayCueNotify_Burst
{
	GENERATED_BODY()

protected:
	virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;

	/** 켜면 플레이어가 맞을 때 PlayerHitEffects를 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category="GCN Effects|Player Hit")
	bool bUsePlayerHitEffects = false;

	/** 플레이어가 맞을 때 내는 이펙트·소리·흔들림. */
	UPROPERTY(EditDefaultsOnly, Category="GCN Effects|Player Hit", meta=(EditCondition="bUsePlayerHitEffects"))
	FGameplayCueNotify_BurstEffects PlayerHitEffects;
};
