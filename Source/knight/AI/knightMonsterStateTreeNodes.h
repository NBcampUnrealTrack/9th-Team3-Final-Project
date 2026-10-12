#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "StateTreeConditionBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeTaskBase.h"
#include "knightMonsterStateTreeNodes.generated.h"

class AknightMonsterCharacterBase;
class UAnimMontage;
class UAbilitySystemComponent;

/*
 * 몬스터용 StateTree 구성 요소. 의도한 구성은 다음과 같다.
 *   Evaluator  Knight Monster Target   -> 타깃, 거리, 각도, 공중 여부, 체력, 분노 상태를 출력한다
 *   Conditions 위 출력에 바인딩한 엔진 Float/Bool 비교 노드, Knight Has Gameplay Tag, Knight Can Use Ability
 *   Tasks      접근은 엔진 Move To, 회전은 Knight Face Target, 기술 사용은 Knight Activate Ability
 * 공격은 GAS 어빌리티로 유지하므로 타격 판정, 피해, 반응이 플레이어와 같은 규칙을 따른다.
 */

USTRUCT()
struct FknightMonsterTargetEvaluatorInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Context")
	TObjectPtr<AknightMonsterCharacterBase> Monster;

	UPROPERTY(EditAnywhere, Category="Output")
	TObjectPtr<AActor> Target;

	UPROPERTY(EditAnywhere, Category="Output")
	bool bHasTarget = false;

	/** 타깃까지의 수평 거리(cm). */
	UPROPERTY(EditAnywhere, Category="Output")
	float Distance = 0.f;

	/** 몬스터 정면에서 타깃까지의 부호 있는 yaw(도). 양수면 오른쪽이다. */
	UPROPERTY(EditAnywhere, Category="Output")
	float AngleToTarget = 0.f;

	/** 타깃 높이에서 몬스터 높이를 뺀 값(cm). */
	UPROPERTY(EditAnywhere, Category="Output")
	float HeightDifference = 0.f;

	/** 타깃에 State.Airborne이 있다(부스트 비행 또는 점프). 대공 기술을 고른다. */
	UPROPERTY(EditAnywhere, Category="Output")
	bool bTargetAirborne = false;

	/** 0..1 범위 */
	UPROPERTY(EditAnywhere, Category="Output")
	float HealthPercent = 1.f;

	UPROPERTY(EditAnywhere, Category="Output")
	bool bEnraged = false;

	UPROPERTY(EditAnywhere, Category="Output")
	bool bExhausted = false;

	/** 반응(다운, 경직, 포효)이 진행 중이다: State.Monster.Reacting. 행동하지 말고 기다린다. */
	UPROPERTY(EditAnywhere, Category="Output")
	bool bIsReacting = false;

	/** 몬스터 정의(UknightMonsterDefinition)의 AI 거리. 공용 StateTree가 몬스터 크기에 맞춰 추격·원거리 판단을 하도록 바인딩한다. */
	UPROPERTY(EditAnywhere, Category="Output")
	float ChaseStopDistance = 900.f;

	UPROPERTY(EditAnywhere, Category="Output")
	float RangedMinDistance = 533.f;

	UPROPERTY(EditAnywhere, Category="Output")
	float RangedMaxDistance = 2000.f;
};

USTRUCT(meta=(DisplayName="Knight Monster Target", Category="Knight"))
struct KNIGHT_API FknightStateTreeMonsterTargetEvaluator : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FknightMonsterTargetEvaluatorInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void TreeStart(FStateTreeExecutionContext& Context) const override;
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

private:
	static void Update(FInstanceDataType& InstanceData);
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FknightActivateAbilityTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Context")
	TObjectPtr<AActor> Actor;

	/** 선택 사항. Evaluator의 Target에 바인딩하면 몬스터 공격을 사거리, 각도, 공중 규칙으로 거른다. */
	UPROPERTY(EditAnywhere, Category="Input", meta=(Optional))
	TObjectPtr<AActor> Target;

	/** 에셋 태그가 이 태그를 모두 만족하는 어빌리티가 후보가 된다. */
	UPROPERTY(EditAnywhere, Category="Parameter")
	FGameplayTagContainer AbilityTags;

	/** 어빌리티가 끝나기 전에 상태를 벗어나면 취소한다(전이가 기술을 끊는 경우). */
	UPROPERTY(EditAnywhere, Category="Parameter")
	bool bCancelOnExit = true;

	/**
	 * 쓸 수 있는 것이 없을 때(모두 쿨다운 중이거나 타깃이 사거리 밖) 이 시간만큼 기다린 뒤 실패한다.
	 * 트리가 매 프레임 공격 상태에 다시 들어가지 않게 하기 위함이다. 0이면 즉시 실패한다.
	 */
	UPROPERTY(EditAnywhere, Category="Parameter", meta=(ClampMin=0, Units="s"))
	float RetryDelay = 0.5f;

	FGameplayAbilitySpecHandle ActiveHandle;
	FDelegateHandle EndedDelegateHandle;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	double FailAtTime = -1.0;
};

/**
 * 액터의 GAS 어빌리티 하나를 발동하고, 그 어빌리티가 끝나면 성공한다(취소되었거나 아무것도 시작하지
 * 못하면 실패). 몬스터 공격(UknightGameplayAbility_MonsterAttack)은 Selection이 현재 타깃을 허용하는 것들 중
 * 가중치 랜덤으로 고른다. 조건에 맞는 다른 어빌리티는 가중치 1로 친다.
 */
USTRUCT(meta=(DisplayName="Knight Activate Ability", Category="Knight"))
struct KNIGHT_API FknightStateTreeActivateAbilityTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FknightStateTreeActivateAbilityTask()
	{
		// Tick은 재시도 대기를 끝내는 용도로만 쓴다. 실행 중인 어빌리티는 종료 델리게이트로 Task를 끝낸다.
		bShouldCallTick = true;
	}

	using FInstanceDataType = FknightActivateAbilityTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FknightFaceTargetTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Context")
	TObjectPtr<AknightMonsterCharacterBase> Monster;

	/** 보통 Knight Monster Target Evaluator의 Target 출력에 바인딩한다. */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<AActor> Target;

	UPROPERTY(EditAnywhere, Category="Parameter", meta=(ClampMin=1, Units="deg"))
	float TurnRate = 90.f;

	UPROPERTY(EditAnywhere, Category="Parameter", meta=(ClampMin=0, Units="deg"))
	float Tolerance = 10.f;

	/** 이 시간이 지나면 다 돌지 못해도 성공으로 끝낸다(타깃이 계속 옆으로 돌 때 공격이 영영 안 나가지 않게). 0이면 제한 없음. */
	UPROPERTY(EditAnywhere, Category="Parameter", meta=(ClampMin=0, Units="s"))
	float MaxTime = 1.5f;

	/**
	 * 지금 각도에서 쓸 수 있는 옆·뒤 전용 공격(Selection.MinAngle이 있는 공격, 예: 꼬리 휩쓸기)이 있으면 돌지 않고 바로 성공한다.
	 * 돌아서 타깃을 정면에 두면 그런 공격은 고를 수 없게 되기 때문이다.
	 */
	UPROPERTY(EditAnywhere, Category="Parameter")
	bool bSkipForSideAttacks = true;

	float ElapsedTime = 0.f;
	/** 재생 중인 회전 몽타주(몬스터 정의의 TurnLeft/RightMontage). */
	TWeakObjectPtr<UAnimMontage> TurnMontage;
	float TurnStartYaw = 0.f;
};

/**
 * 몬스터를 제자리에서 타깃 쪽으로 돌리고, 타깃을 바라보면 성공한다.
 * 몬스터 정의에 회전 몽타주가 있고 타깃이 TurnMontageMinAngle 이상 벗어나 있으면 몽타주로 돌고(발을 디디며 돈다),
 * 남은 작은 각도는 이동 컴포넌트의 보간 회전으로 맞춘다. MaxTime은 몽타주가 끝난 뒤부터 센다.
 */
USTRUCT(meta=(DisplayName="Knight Face Target", Category="Knight"))
struct KNIGHT_API FknightStateTreeFaceTargetTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FknightStateTreeFaceTargetTask()
	{
		bShouldCallTick = true;
	}

	using FInstanceDataType = FknightFaceTargetTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

private:
	static bool TryStartTurnMontage(FInstanceDataType& InstanceData);
	static bool IsTurnMontagePlaying(const FInstanceDataType& InstanceData);
	static void UpdateTurnWarpTarget(const FInstanceDataType& InstanceData);
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FknightHasGameplayTagConditionInstanceData
{
	GENERATED_BODY()

	/** ASC가 있는 아무 액터. 몬스터 자신이나 Evaluator가 준 타깃. */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<AActor> Actor;

	UPROPERTY(EditAnywhere, Category="Parameter")
	FGameplayTagContainer Tags;

	/** true면 액터에 Tags가 모두 있어야 한다. false면 하나만 있어도 된다. */
	UPROPERTY(EditAnywhere, Category="Parameter")
	bool bRequireAll = false;

	UPROPERTY(EditAnywhere, Category="Parameter")
	bool bInvert = false;
};

/** 액터 ASC의 게임플레이 태그를 검사한다(State.Monster.Enraged, State.HitReact, 타깃의 State.Flying 등). */
USTRUCT(meta=(DisplayName="Knight Has Gameplay Tag", Category="Knight"))
struct KNIGHT_API FknightStateTreeHasGameplayTagCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FknightHasGameplayTagConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FknightCanUseAbilityConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Context")
	TObjectPtr<AActor> Actor;

	/** Evaluator의 Target에 바인딩한다. 몬스터 공격의 사거리와 각도를 이 타깃 기준으로 본다. */
	UPROPERTY(EditAnywhere, Category="Input", meta=(Optional))
	TObjectPtr<AActor> Target;

	/** Knight Activate Ability와 같은 규칙: 에셋 태그가 이 태그를 모두 만족하는 어빌리티가 후보다. */
	UPROPERTY(EditAnywhere, Category="Parameter")
	FGameplayTagContainer AbilityTags;

	/** 각도 제한을 무시한다. 발동 전에 Knight Face Target으로 몸을 돌리는 상태에 쓴다. */
	UPROPERTY(EditAnywhere, Category="Parameter")
	bool bIgnoreAngle = true;

	UPROPERTY(EditAnywhere, Category="Parameter")
	bool bInvert = false;
};

/**
 * Knight Activate Ability가 지금 고를 수 있는 어빌리티가 있는지 검사한다(사거리, 쿨다운, 소유 태그).
 * 추적 중 원거리 공격으로 끼어드는 전이처럼, 쿨다운 중이면 상태를 오가지 않아야 하는 곳에 쓴다.
 */
USTRUCT(meta=(DisplayName="Knight Can Use Ability", Category="Knight"))
struct KNIGHT_API FknightStateTreeCanUseAbilityCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FknightCanUseAbilityConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
