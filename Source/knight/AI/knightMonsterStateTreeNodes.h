#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "StateTreeConditionBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeTaskBase.h"
#include "knightMonsterStateTreeNodes.generated.h"

class AknightMonsterCharacterBase;
class UAbilitySystemComponent;

/*
 * StateTree building blocks for monsters. The intended layout:
 *   Evaluator  Knight Monster Target   -> outputs target, distance, angle, airborne, health, rage state
 *   Conditions engine Float/Bool compare nodes bound to those outputs, plus Knight Has Gameplay Tag
 *   Tasks      engine Move To for approach, Knight Face Target for turning, Knight Activate Ability for moves
 * Attacks stay GAS abilities, so hit detection, damage and reactions match the player's rules.
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

	/** Horizontal distance to the target in cm. */
	UPROPERTY(EditAnywhere, Category="Output")
	float Distance = 0.f;

	/** Signed yaw from the monster's forward to the target in degrees; positive is to the right. */
	UPROPERTY(EditAnywhere, Category="Output")
	float AngleToTarget = 0.f;

	/** Target height minus monster height in cm. */
	UPROPERTY(EditAnywhere, Category="Output")
	float HeightDifference = 0.f;

	/** Target has State.Airborne (boost flight or a jump); pick anti-air moves. */
	UPROPERTY(EditAnywhere, Category="Output")
	bool bTargetAirborne = false;

	/** 0..1 */
	UPROPERTY(EditAnywhere, Category="Output")
	float HealthPercent = 1.f;

	UPROPERTY(EditAnywhere, Category="Output")
	bool bEnraged = false;

	UPROPERTY(EditAnywhere, Category="Output")
	bool bExhausted = false;

	/** A reaction (down, flinch, roar) is running: State.Monster.Reacting. Wait instead of acting. */
	UPROPERTY(EditAnywhere, Category="Output")
	bool bIsReacting = false;
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

	/** Optional. Bind to the evaluator's Target so monster attacks are filtered by range, angle and airborne rules. */
	UPROPERTY(EditAnywhere, Category="Input", meta=(Optional))
	TObjectPtr<AActor> Target;

	/** Abilities whose asset tags match all of these are candidates. */
	UPROPERTY(EditAnywhere, Category="Parameter")
	FGameplayTagContainer AbilityTags;

	/** Cancel the ability if the state is left before it ends (a transition interrupts the move). */
	UPROPERTY(EditAnywhere, Category="Parameter")
	bool bCancelOnExit = true;

	/**
	 * When nothing can be used (all on cooldown, target out of range), wait this long before failing, so the
	 * tree does not re-enter the attack state every frame. 0 fails at once.
	 */
	UPROPERTY(EditAnywhere, Category="Parameter", meta=(ClampMin=0, Units="s"))
	float RetryDelay = 0.5f;

	FGameplayAbilitySpecHandle ActiveHandle;
	FDelegateHandle EndedDelegateHandle;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	double FailAtTime = -1.0;
};

/**
 * Activates one GAS ability on the actor and succeeds when it ends (fails if it was cancelled or nothing
 * could start). Monster attacks (UknightGameplayAbility_MonsterAttack) are chosen by weighted random among
 * those whose Selection allows the current target; other matching abilities count with weight 1.
 */
USTRUCT(meta=(DisplayName="Knight Activate Ability", Category="Knight"))
struct KNIGHT_API FknightStateTreeActivateAbilityTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FknightStateTreeActivateAbilityTask()
	{
		// Ticks only to end the retry wait; a running ability finishes the task through its end delegate.
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

	/** Usually bound to the Knight Monster Target evaluator's Target output. */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<AActor> Target;

	UPROPERTY(EditAnywhere, Category="Parameter", meta=(ClampMin=1, Units="deg"))
	float TurnRate = 90.f;

	UPROPERTY(EditAnywhere, Category="Parameter", meta=(ClampMin=0, Units="deg"))
	float Tolerance = 10.f;
};

/** Turns the monster in place toward the target; succeeds once facing it. For turn montages use Activate Ability. */
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
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FknightHasGameplayTagConditionInstanceData
{
	GENERATED_BODY()

	/** Any actor with an ASC: the monster itself, or the target from the evaluator. */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<AActor> Actor;

	UPROPERTY(EditAnywhere, Category="Parameter")
	FGameplayTagContainer Tags;

	/** True: the actor needs all Tags. False: any one is enough. */
	UPROPERTY(EditAnywhere, Category="Parameter")
	bool bRequireAll = false;

	UPROPERTY(EditAnywhere, Category="Parameter")
	bool bInvert = false;
};

/** Tests gameplay tags on an actor's ASC (State.Monster.Enraged, State.HitReact, State.Flying on the target). */
USTRUCT(meta=(DisplayName="Knight Has Gameplay Tag", Category="Knight"))
struct KNIGHT_API FknightStateTreeHasGameplayTagCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FknightHasGameplayTagConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
