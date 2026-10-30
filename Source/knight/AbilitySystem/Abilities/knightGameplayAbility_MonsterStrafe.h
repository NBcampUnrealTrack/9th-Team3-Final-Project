#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "knightGameplayAbility_MonsterStrafe.generated.h"

class UAbilityTask_Repeat;
class UAnimMontage;

/**
 * 타깃을 바라본 채 옆으로 걸으며 타깃 둘레를 조금 도는 몬스터 행동. 공격 판정은 없다.
 * 공격 사이에 멀뚱히 서 있지 않고 틈을 노리는 것처럼 보이게 하는 용도라, 몬스터 공격으로 두어 Selection으로 가끔 고른다.
 *
 * 이동: 몽타주에 루트 모션이 있으면(발 기준으로 구운 옆걸음, knightRootMotionLibrary::BakeRootMotionFromFeet) 그대로 따라가서
 * 걸음과 이동 속도가 정확히 맞는다. 몬스터가 타깃을 바라보며 돌기 때문에 옆으로 가는 길이 저절로 타깃 둘레의 호가 된다.
 * 루트 모션이 없는 제자리 몽타주면 StrafeStartTime부터 StrafeSpeed의 일정한 속도로 옮긴다(타깃에서 멀어지지 않게 현 방향).
 * 회전: 매 틱 남은 각도에 비례하는 속도(최대 FaceTurnRate)로 돌아 멈췄다 섰다 하지 않고 부드럽게 따라간다.
 * 옆으로 벽이 있으면 반대쪽으로 걷는다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_MonsterStrafe : public UknightGameplayAbility_MonsterAttack
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** 몸이 왼쪽으로 가는 옆걸음. 걷는 시간보다 길어야 한다(같은 애니메이션을 섹션 두 개로 이어 붙인다). */
	UPROPERTY(EditDefaultsOnly, Category="Strafe")
	TObjectPtr<UAnimMontage> LeftMontage;

	UPROPERTY(EditDefaultsOnly, Category="Strafe")
	TObjectPtr<UAnimMontage> RightMontage;

	/** 루트 모션이 없는 몽타주일 때만 쓴다. 옆으로 걷는 속도. 애니메이션에서 디딘 발이 미끄러지지 않는 속도에 맞춘다. */
	UPROPERTY(EditDefaultsOnly, Category="Strafe", meta=(ClampMin=0, Units="cm/s"))
	float StrafeSpeed = 230.f;

	/** 옆걸음 전체 시간. 끝나면 몽타주를 StopBlendTime 동안 섞어 내린다. */
	UPROPERTY(EditDefaultsOnly, Category="Strafe", meta=(ClampMin=0.1, Units="s"))
	float StrafeTime = 2.5f;

	/** 몽타주가 섞여 들어오는 동안은 제자리에 둔다. */
	UPROPERTY(EditDefaultsOnly, Category="Strafe", meta=(ClampMin=0, Units="s"))
	float StrafeStartTime = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category="Strafe", meta=(ClampMin=0, Units="s"))
	float StopBlendTime = 0.35f;

	/** 타깃을 바라보는 최대 회전 속도(도/초). */
	UPROPERTY(EditDefaultsOnly, Category="Strafe", meta=(ClampMin=0, Units="deg"))
	float FaceTurnRate = 60.f;

	/** 남은 각도에 곱해 회전 속도를 정한다(1/초). 클수록 빨리 따라붙고, 작을수록 느긋하다. */
	UPROPERTY(EditDefaultsOnly, Category="Strafe", meta=(ClampMin=0))
	float FaceTurnGain = 3.f;

	/** 걸어갈 쪽으로 이 거리 안에 벽이 있으면 반대쪽으로 걷는다. */
	UPROPERTY(EditDefaultsOnly, Category="Strafe", meta=(ClampMin=0, Units="cm"))
	float WallCheckDistance = 800.f;

private:
	/** 걸어갈 쪽(왼쪽이면 true). 벽이 있는 쪽은 피한다. */
	bool ChooseLeft() const;

	UFUNCTION()
	void HandleTick(int32 ActionNumber);

	UFUNCTION()
	void HandleMontageEnded();

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	bool bLeft = false;
	bool bStepStarted = false;
	float Elapsed = 0.f;
};
