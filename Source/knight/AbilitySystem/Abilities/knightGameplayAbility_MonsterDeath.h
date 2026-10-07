#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_MonsterDeath.generated.h"

class UAnimMontage;

/**
 * 몬스터 사망 연출. AknightMonsterCharacterBase가 체력이 0이 될 때 보내는 Event.Death로 발동한다.
 *
 * 몬스터는 State.Dead를 붙인 뒤 Event.Death를 보내고 Ability.Death를 제외한 어빌리티를 모두 취소하므로, 이 어빌리티는
 * State.Dead에 막히지 않고 Ability.Death 태그로 취소를 피한다.
 *
 * 시체는 사망 몽타주의 마지막 포즈를 유지해야 하므로 어빌리티를 끝내지 않는다(끝내면 몽타주가 멈추고 대기 포즈로
 * 돌아간다). DeathMontage는 Enable Auto Blend Out을 꺼 둔다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_MonsterDeath : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_MonsterDeath(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** 선택 사항. 없으면 연출 없이 현재 포즈로 남는다(스켈레톤 없는 대역). */
	UPROPERTY(EditDefaultsOnly, Category="Death")
	TObjectPtr<UAnimMontage> DeathMontage;
};
