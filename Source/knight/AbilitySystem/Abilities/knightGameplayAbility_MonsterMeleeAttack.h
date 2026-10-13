#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "Combat/knightCombatTypes.h"
#include "knightGameplayAbility_MonsterMeleeAttack.generated.h"

class UAnimMontage;

/**
 * 몽타주가 타이밍을 잡는 몬스터 공격(할퀴기, 물기, 연타, 꼬리 휘두르기). 몽타주의 Knight Melee Trace 노티파이 구간
 * 동안 몬스터의 UknightMeleeTraceComponent가 보낸 Event.Melee.Hit를 UknightCombatLibrary::ApplyMeleeHit으로
 * 처리하므로, 가드, 패링, 회피, 부위 배율이 플레이어 무기 타격과 같은 규칙을 따른다.
 *
 * 판정 구간이 여러 개면 노티파이를 여러 개 놓는다. 연타의 마무리처럼 착지 순간 넓게 덮는 타격은 Knight Melee Area 노티파이로
 * 같은 몽타주 안에 섞을 수 있다(같은 HitParams). 몽타주가 끝나면 어빌리티도 끝난다.
 * 한 번의 오버랩으로 충분한 공격(내려찍기 착지 등)은 UknightGameplayAbility_MonsterAreaAttack을 쓴다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_MonsterMeleeAttack : public UknightGameplayAbility_MonsterAttack
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** 판정 구간(Knight Melee Trace 노티파이)이 들어 있는 공격 몽타주. */
	UPROPERTY(EditDefaultsOnly, Category="Attack")
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditDefaultsOnly, Category="Attack", meta=(ClampMin=0.01))
	float PlayRate = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FknightMeleeHitParams HitParams;

private:
	UFUNCTION()
	void HandleMontageFinished();

	UFUNCTION()
	void HandleMeleeHit(FGameplayEventData Payload);
};
