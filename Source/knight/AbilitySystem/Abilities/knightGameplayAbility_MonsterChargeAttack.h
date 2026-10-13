#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "Combat/knightCombatTypes.h"
#include "Monster/knightMonsterRushEffects.h"
#include "knightGameplayAbility_MonsterChargeAttack.generated.h"

class UAbilityTask_Repeat;
class UAnimMontage;

/**
 * 멀리 있는 타깃에게 달려드는 몬스터 돌진. 몽타주는 StartSection → LoopSection(자기 자신으로 반복) → StopSection 순서이고,
 * 이동은 몽타주 루트 모션이 맡는다. 어빌리티는 출발하는 동안 타깃 쪽으로 크게 돌고(StartSteerRate), 달리는 동안에는 조금씩만
 * 틀고(SteerRate, 타깃을 맞힌 뒤에는 직진), 머리 앞 구체에 닿은 대상을 한 번씩 때려 옆·앞으로 날려 보내고(맞혀도 멈추지 않는다), 언제 멈출지 정한다.
 *
 * 멈추는 조건: 타깃을 PassDistance만큼 지나쳤거나, Loop를 MaxLoopTime 동안 달렸거나, 타깃을 잃었으면 지금 Loop 한 바퀴를 마치고
 * Stop으로 넘어간다. 벽 등에 막혀 거의 움직이지 못하면 바로 Stop으로 넘어간다. 몽타주가 끝난 뒤 RecoveryTime 동안 서 있는
 * 시간이 반격 기회다.
 *
 * 디버그: knight.Monster.DrawAttacks 1 이면 판정 구체를 그린다(빨강, 맞으면 자홍).
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_MonsterChargeAttack : public UknightGameplayAbility_MonsterAttack
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_MonsterChargeAttack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Start, Loop, Stop 섹션이 있는 몽타주. Loop 섹션은 자기 자신으로 이어져야 한다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack")
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FName StartSection = TEXT("Start");

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FName LoopSection = TEXT("Loop");

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FName StopSection = TEXT("Stop");

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FknightMeleeHitParams HitParams;

	/** 판정 구체가 따라가는 소켓(본). 없으면 액터 위치를 기준으로 한다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Hit")
	FName HitSocket = TEXT("head");

	/** HitSocket 위치에서 몬스터 방향 기준으로 더하는 오프셋(X가 정면). 주둥이 앞을 덮도록 둔다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Hit")
	FVector HitOffset = FVector(100.f, 0.f, -100.f);

	UPROPERTY(EditDefaultsOnly, Category="Attack|Hit", meta=(ClampMin=1, Units="cm"))
	float HitRadius = 250.f;

	/** Start 섹션 안에서 판정이 열리는 시각(섹션 시작부터 초). 출발 직후 제자리 동작에는 맞지 않게 한다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Hit", meta=(ClampMin=0, Units="s"))
	float HitStartTime = 0.4f;

	/** Stop 섹션이 시작된 뒤에도 판정이 남는 시간. 미끄러지며 멈추는 동안에도 맞는다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Hit", meta=(ClampMin=0, Units="s"))
	float StopHitTime = 0.3f;

	/** Start 섹션 동안 타깃 쪽으로 몸을 트는 속도(도/초, 가속 없음). 출발하면서 타깃을 겨눈다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Charge", meta=(ClampMin=0, Units="deg"))
	float StartSteerRate = 240.f;

	/** Loop 섹션 동안 타깃 쪽으로 몸을 트는 최대 속도(도/초). 0이면 직진한다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Charge", meta=(ClampMin=0, Units="deg"))
	float SteerRate = 30.f;

	/** 타깃이 몬스터 중심보다 이 거리만큼 뒤로 가면(지나치면) 멈추기 시작한다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Charge", meta=(ClampMin=0, Units="cm"))
	float PassDistance = 300.f;

	/** Loop 섹션을 최대 이 시간만큼 달린다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Charge", meta=(ClampMin=0, Units="s"))
	float MaxLoopTime = 3.f;

	/** Loop 중 수평 속도가 이 값보다 느린 상태가 BlockedTime 동안 이어지면 막힌 것으로 보고 바로 멈춘다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Charge", meta=(ClampMin=0, Units="cm/s"))
	float BlockedSpeed = 200.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Charge", meta=(ClampMin=0, Units="s"))
	float BlockedTime = 0.2f;

	/** 맞은 대상을 날려 보내는 수평 속도(돌진 경로 밖, 정면에서 약 63도). 0이면 날리지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Hit", meta=(ClampMin=0, Units="cm/s"))
	float KnockbackSpeed = 2000.f;

	/** 날려 보내는 시간. 이동 거리는 KnockbackSpeed x KnockbackTime이다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Hit", meta=(ClampMin=0, Units="s"))
	float KnockbackTime = 0.4f;

	/** 가드로 막았을 때 날려 보내는 세기 배율. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Hit", meta=(ClampMin=0, ClampMax=1))
	float GuardKnockbackScale = 0.5f;

	/** 몽타주가 끝난 뒤 어빌리티가 끝나기까지 서 있는 시간. 플레이어가 반격할 수 있는 구간이다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Timing", meta=(ClampMin=0, Units="s"))
	float RecoveryTime = 1.5f;

	/** 판정이 열린 뒤부터 Stop 섹션 전까지 켜는 바람 줄기와 흙먼지. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Effects")
	FknightMonsterRushEffects RushEffects;

private:
	UFUNCTION()
	void HandleTick(int32 ActionNumber);

	UFUNCTION()
	void HandleMontageEnded();

	UFUNCTION()
	void HandleMontageInterrupted();

	UFUNCTION()
	void HandleRecoveryFinished();

	void ApplyHits();
	void Knockback(const AActor& Avatar, AActor& Target, float Scale);
	void TickKnockback(float DeltaTime);
	void BeginStop(bool bImmediate, const TCHAR* Reason);

	TWeakObjectPtr<UAbilityTask_Repeat> TickTask;
	TWeakObjectPtr<AActor> KnockbackActor;
	FVector KnockbackVelocity = FVector::ZeroVector;
	float KnockbackRemaining = 0.f;
	FVector KnockbackSide = FVector::ZeroVector;
	bool bKnockbackFlipped = false;
	TSet<TWeakObjectPtr<AActor>> HitActors;
	FVector LastLocation = FVector::ZeroVector;
	float LoopTime = 0.f;
	float SlowTime = 0.f;
	bool bStopping = false;
	bool bMontageEnded = false;
	/** 이번 돌진에서 현재 타깃을 맞혔다. 이후로는 방향을 틀지 않는다. */
	bool bHitTarget = false;
};
