#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "knightGameplayAbility_MonsterStepBack.generated.h"

class UAnimMontage;

/**
 * 타깃을 바라본 채 뒤로 물러나 거리를 벌리는 몬스터 행동. 공격 판정은 없다.
 * 몬스터 공격으로 두어서 Selection(가까이 붙었을 때)으로 고르고, 물러난 뒤에는 FollowUpAttacks(원거리 공격)로 바로 잇는다.
 *
 * 뒷걸음 애니메이션은 제자리 동작이라 StepStartTime ~ StepEndTime 동안 타깃 반대쪽으로 일정한 속도의 루트 모션을 준다.
 * 속도(StepDistance / 구간 길이)는 애니메이션에서 디딘 발이 미끄러지지 않는 속도에 맞춘다. 회전은 하지 않는다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_MonsterStepBack : public UknightGameplayAbility_MonsterAttack
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category="Step Back")
	TObjectPtr<UAnimMontage> Montage;

	/** 물러나는 거리. */
	UPROPERTY(EditDefaultsOnly, Category="Step Back", meta=(ClampMin=0, Units="cm"))
	float StepDistance = 630.f;

	/** 발동 후 물러나기 시작하는 시각. 몽타주가 섞여 들어오는 동안은 서 있게 둔다. */
	UPROPERTY(EditDefaultsOnly, Category="Step Back", meta=(ClampMin=0, Units="s"))
	float StepStartTime = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category="Step Back", meta=(ClampMin=0, Units="s"))
	float StepEndTime = 1.5f;

private:
	UFUNCTION()
	void StartStep();

	UFUNCTION()
	void HandleMontageEnded();
};
