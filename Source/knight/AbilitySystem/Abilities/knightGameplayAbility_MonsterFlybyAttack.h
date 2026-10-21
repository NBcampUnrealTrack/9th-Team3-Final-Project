#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "Combat/knightCombatTypes.h"
#include "knightGameplayAbility_MonsterFlybyAttack.generated.h"

class UAbilityTask_Repeat;
class UAnimMontage;

/**
 * 공중 턴의 날짐승이 타깃 머리 위를 일자로 빠르게 지나가며 아래 바닥을 띠 모양으로 때리는 공격(잔향 초음파 브레스).
 *
 *   경고: WarningMontage를 재생하며 WarningTime 동안 타깃 쪽으로 돈다. 그동안 몬스터에 State.Monster.Warning이 붙는다(HUD WARNING 띠용).
 *   지나가기: 경고가 끝나는 순간의 타깃 쪽으로 방향을 고정하고, 타깃 너머 PassOvershoot까지 PassSpeed로 날아간다.
 *             지나가는 동안 BeamBone(가슴) 아래 바닥에서 반지름 HalfWidth 구체로 때린다(폭 HalfWidth*2 띠). 대상마다 한 번.
 *
 * 회피만 되게 하려면 HitParams.DamageEffect에 가드 범위 0인 피해 실행을 쓰고 bCanBeParried를 끈다.
 * 디버그: knight.Monster.DrawAttacks 1 이면 경고 중 지나갈 띠와 판정 구체를 그린다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_MonsterFlybyAttack : public UknightGameplayAbility_MonsterAttack
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_MonsterFlybyAttack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** 선택 사항. 경고 동안 재생하고 지나가기 시작할 때 멈춘다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack")
	TObjectPtr<UAnimMontage> WarningMontage;

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FknightMeleeHitParams HitParams;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Warning", meta=(ClampMin=0, Units="s"))
	float WarningTime = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Warning", meta=(ClampMin=0, Units="deg"))
	float TurnRate = 240.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Pass", meta=(ClampMin=1, Units="cm/s"))
	float PassSpeed = 2500.f;

	/** 타깃 너머 이만큼 더 지나간 뒤 멈춘다(몸 기준). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Pass", meta=(ClampMin=0, Units="cm"))
	float PassOvershoot = 1500.f;

	/** 바닥 판정이 따라가는 뼈(가슴). 이 뼈 바로 아래 바닥을 때린다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Hit")
	FName BeamBone = TEXT("spine_02");

	/** 띠의 절반 폭. 바닥 위 같은 높이까지 닿는다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Hit", meta=(ClampMin=1, Units="cm"))
	float HalfWidth = 300.f;

private:
	UFUNCTION()
	void HandleTick(int32 ActionNumber);

	void StartPass();
	void ApplyHits();
	void SetWarning(bool bOn);

	TWeakObjectPtr<UAbilityTask_Repeat> TickTask;
	TWeakObjectPtr<UAnimMontage> PlayingWarning;
	TSet<TWeakObjectPtr<AActor>> HitActors;
	bool bPassing = false;
	bool bWarningApplied = false;
	float PhaseTime = 0.f;
	float PassDuration = 0.f;
};
