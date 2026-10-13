#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "Combat/knightCombatTypes.h"
#include "Monster/knightMonsterRushEffects.h"
#include "knightGameplayAbility_MonsterLeapAttack.generated.h"

class UAbilityTask_Repeat;
class UAnimMontage;

/**
 * 달려오다 도약해 공중에서 무는 몬스터 공격(점프 돌진). 몽타주 하나에 달리기 → 도약 → 물기 → 착지가 들어 있고,
 * 이동은 몽타주 루트 모션이 맡는다.
 *
 * 거리 맞추기: 타깃이 가까우면 달리기 앞부분을 건너뛰고 시작한다(시작 시각을 거리로 고른다). 남은 차이는 LeapStartTime부터
 * 루트 모션 이동 배율(AnimRootMotionTranslationScale)로 늘리거나 줄여 BiteTime에 입 앞(BiteReach)이 타깃에 오게 맞춘다.
 * 배율과 방향은 TrackTargetUntil까지 타깃을 따라가고 그 뒤에는 고정되므로, 도약을 본 뒤 옆으로 피할 수 있다.
 * 도약 높이는 루트 모션의 Z라서 공중 구간(TakeoffTime ~ LandTime)에는 비행 모드로 바꿔 Z 이동을 살린다.
 *
 * 판정은 HitStartTime ~ HitEndTime 동안 HitSocket 앞 구체에 닿은 대상을 한 번씩 때린다.
 * 디버그: knight.Monster.DrawAttacks 1 이면 판정 구체(빨강, 맞으면 자홍)와 BiteTime의 예상 입 위치(노랑)를 그린다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_MonsterLeapAttack : public UknightGameplayAbility_MonsterAttack
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_MonsterLeapAttack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category="Attack")
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FknightMeleeHitParams HitParams;

	/** 이 시각보다 늦게 시작하지 않는다(몽타주 초). 가까운 타깃이어도 도약 전에 조금은 달린다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Leap", meta=(ClampMin=0, Units="s"))
	float MaxStartTime = 2.f;

	/** 이동 배율을 적용하기 시작하는 시각(몽타주 초). 달리기가 끝나고 도약하기 직전. 그 전의 달리기는 원래 속도다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Leap", meta=(ClampMin=0, Units="s"))
	float LeapStartTime = 2.2f;

	/** 이때(몽타주 초) 입 앞이 타깃 위치에 온다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Leap", meta=(ClampMin=0, Units="s"))
	float BiteTime = 3.f;

	/** BiteTime에 몬스터 중심에서 입 앞까지의 수평 거리. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Leap", meta=(ClampMin=0, Units="cm"))
	float BiteReach = 650.f;

	/** 이 시각(몽타주 초)까지 이동 배율과 방향이 타깃을 따라간다. 그 뒤로는 고정된다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Leap", meta=(ClampMin=0, Units="s"))
	float TrackTargetUntil = 2.3f;

	/** 도약 이동 배율의 범위. 높이도 같은 배율로 바뀐다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Leap", meta=(ClampMin=0.1))
	float MinLeapScale = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Leap", meta=(ClampMin=0.1))
	float MaxLeapScale = 1.5f;

	/** 공중 구간(몽타주 초). 이 동안 비행 모드로 루트 모션의 Z를 따른다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Leap", meta=(ClampMin=0, Units="s"))
	float TakeoffTime = 2.2f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Leap", meta=(ClampMin=0, Units="s"))
	float LandTime = 3.3f;

	/** TrackTargetUntil까지 타깃 쪽으로 몸을 트는 속도(도/초, 가속 없음). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Leap", meta=(ClampMin=0, Units="deg"))
	float RunSteerRate = 90.f;

	/** 판정 구체가 따라가는 소켓(본). 없으면 액터 위치를 기준으로 한다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Hit")
	FName HitSocket = TEXT("jaw");

	/** HitSocket 위치에서 몬스터 방향 기준으로 더하는 오프셋(X가 정면). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Hit")
	FVector HitOffset = FVector(100.f, 0.f, 0.f);

	UPROPERTY(EditDefaultsOnly, Category="Attack|Hit", meta=(ClampMin=1, Units="cm"))
	float HitRadius = 250.f;

	/** 판정 구간(몽타주 초). 턱이 닫히는 동안. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Hit", meta=(ClampMin=0, Units="s"))
	float HitStartTime = 2.9f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Hit", meta=(ClampMin=0, Units="s"))
	float HitEndTime = 3.1f;

	/** 몽타주가 끝난 뒤 어빌리티가 끝나기까지 서 있는 시간. 플레이어가 반격할 수 있는 구간이다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Timing", meta=(ClampMin=0, Units="s"))
	float RecoveryTime = 1.f;

	/** 시작부터 LandTime까지 켜는 바람 줄기와 흙먼지(공중에서는 흙먼지를 남기지 않는다). */
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

	/** From부터 To까지(몽타주 초) 루트 모션의 수평 이동 거리(cm, 배율 적용 전). */
	float GetTravel(float From, float To) const;
	/** 타깃까지 Distance일 때 BiteTime에 입 앞이 타깃에 닿는 시작 시각. */
	float FindStartTime(float Distance) const;
	/** 지금 위치(Position)에서 BiteTime에 입 앞이 타깃에 닿도록 도약 이동 배율을 정한다. */
	void UpdateLeapScale(const AActor& Target, float Position);
	void ApplyHits();
	void SetAirborne(bool bInAirborne);

	TWeakObjectPtr<UAbilityTask_Repeat> TickTask;
	TSet<TWeakObjectPtr<AActor>> HitActors;
	float MeshScale = 1.f;
	float LeapScale = 1.f;
	bool bAirborne = false;
	bool bMontageEnded = false;
};
