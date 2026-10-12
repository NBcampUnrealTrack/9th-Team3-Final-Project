#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterMeleeAttack.h"
#include "knightGameplayAbility_MonsterSwingAttack.generated.h"

class UAbilityTask_Repeat;

/**
 * 몸을 크게 돌려 휩쓰는 근접 공격(꼬리 휩쓸기). 예비 동작으로 휘두를 반대쪽으로 몸을 틀었다가, 몽타주의 회전 구간을
 * MotionWarping으로 SwingYaw만큼 늘려 빠르게 휘두른다. 완급(천천히 틀기 → 빠르게 휘두르기)은 몽타주의 Knight Play Rate로 잡는다.
 *
 * 예비 동작은 항상 SwingYaw의 반대 방향으로 돈다. 타깃이 예비 동작 뒤 WindupTargetAngle(휘두르는 쪽 뒤)에 오도록 돌되,
 * 그러려면 WindupMinYaw~WindupMaxYaw를 벗어날 때는 WindupMinYaw만 돈다. 시작 회전(StartTurnYaw)은 쓰지 않는다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_MonsterSwingAttack : public UknightGameplayAbility_MonsterMeleeAttack
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** 휘두르는 각도. 음수는 왼쪽으로 돌며 휘두른다(꼬리는 오른쪽을 쓴다). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Swing", meta=(ClampMin=-360, ClampMax=360, Units="deg"))
	float SwingYaw = -180.f;

	/** 몽타주에서 몸이 실제로 도는 구간(몽타주 시간, 초). 이 구간의 회전량을 SwingYaw에 맞춘다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Swing", meta=(ClampMin=0, Units="s"))
	float SwingWarpStartTime = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Swing", meta=(ClampMin=0, Units="s"))
	float SwingWarpEndTime = 3.f;

	/** 예비 동작이 끝났을 때 타깃이 있을 각도(정면 기준, 휘두르는 쪽이 양수). 꼬리가 휘두르기 초반에 지나가는 자리. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Windup", meta=(ClampMin=0, ClampMax=180, Units="deg"))
	float WindupTargetAngle = 150.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Windup", meta=(ClampMin=0, ClampMax=180, Units="deg"))
	float WindupMinYaw = 30.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Windup", meta=(ClampMin=0, ClampMax=180, Units="deg"))
	float WindupMaxYaw = 120.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Windup", meta=(ClampMin=1, Units="deg"))
	float WindupTurnRate = 120.f;

	/** 예비 동작으로 도는 최대 시간(실제 초). SwingWarpStartTime에 닿기 전에 끝나야 한다(몽타주 재생 속도 고려). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Windup", meta=(ClampMin=0, Units="s"))
	float WindupMaxTime = 1.1f;

private:
	UFUNCTION()
	void HandleWindupTick(int32 ActionNumber);

	TWeakObjectPtr<UAbilityTask_Repeat> WindupTask;
	float WindupTargetYaw = 0.f;
};
