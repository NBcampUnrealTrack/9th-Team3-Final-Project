#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "Combat/knightCombatTypes.h"
#include "knightGameplayAbility_MonsterAreaAttack.generated.h"

class UAnimMontage;

/**
 * 몽타주 대신 어빌리티가 타이밍을 잡는 몬스터 공격. 예고 → 몬스터 앞쪽 구체 오버랩(내려찍기, 발구르기는 한 번,
 * 브레스처럼 이어지는 공격은 ActiveTime 동안 계속) → 후딜 순서로 진행한다. 대상마다 한 번만 맞는다.
 *
 * 구체 안의 적대적인 ICombatInterface 액터마다 UknightCombatLibrary::ApplyMeleeHit을 한 번씩 거치므로
 * 가드, 패링, 회피, 피해가 무기 타격과 같은 규칙을 따른다. 스켈레톤 없이도 동작하므로 테스트 맵의 큐브
 * 대역에 쓸 수 있고, 실제 몬스터도 범위 공격에 쓸 수 있다. 이때 Montage는 연출용으로만 재생하고 타이밍은 여기서 잡는다.
 *
 * 디버그: knight.Monster.DrawAttacks 1 이면 예고(주황)와 타격 지점(빨강)을 그린다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_MonsterAreaAttack : public UknightGameplayAbility_MonsterAttack
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_MonsterAreaAttack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** 오버랩을 실행하고 타격을 적용한다. 타격 순간에 효과를 더하려면 오버라이드한다. */
	virtual void OnImpact();

	/** 발동부터 타격까지의 시간. 플레이어가 공격을 읽고 피할 수 있는 구간이다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Timing", meta=(ClampMin=0, Units="s"))
	float WindupTime = 0.8f;

	/** 타격 순간부터 판정이 유지되는 시간. 0이면 타격 순간 한 번만 판정한다. 유지되는 동안 구체에 들어온 대상도 맞는다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Timing", meta=(ClampMin=0, Units="s"))
	float ActiveTime = 0.f;

	/** 판정이 끝난 뒤 어빌리티가 끝나기까지의 시간. 플레이어가 반격할 수 있는 구간이다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Timing", meta=(ClampMin=0, Units="s"))
	float RecoveryTime = 1.f;

	/** 몬스터 로컬 공간 기준 구체 중심(X가 정면). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Area")
	FVector AreaOffset = FVector(350.f, 0.f, 0.f);

	UPROPERTY(EditDefaultsOnly, Category="Attack|Area", meta=(ClampMin=1, Units="cm"))
	float AreaRadius = 300.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FknightMeleeHitParams HitParams;

	/** 선택 사항. 연출용으로만 재생하며 어빌리티가 끝나면 멈춘다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation")
	TObjectPtr<UAnimMontage> Montage;

	/** 선택 사항. 예고가 시작될 때 범위 중심에서 실행하는 큐(바닥 데칼, 포효 등). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation", meta=(Categories="GameplayCue"))
	FGameplayTag TelegraphCue;

private:
	UFUNCTION()
	void HandleWindupFinished();

	UFUNCTION()
	void HandleActiveTick(int32 ActionNumber);

	UFUNCTION()
	void StartRecovery(int32 ActionNumber = 0);

	UFUNCTION()
	void HandleRecoveryFinished();

	FVector GetAreaCenter() const;

	/** 이번 발동에서 이미 맞은 대상. 판정이 유지되는 동안 같은 대상을 다시 때리지 않는다. */
	TSet<TWeakObjectPtr<AActor>> HitActors;
};
