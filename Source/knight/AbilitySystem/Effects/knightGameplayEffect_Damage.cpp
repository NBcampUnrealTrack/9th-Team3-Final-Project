#include "AbilitySystem/Effects/knightGameplayEffect_Damage.h"

#include "AbilitySystem/Executions/knightDamageExecution.h"

UknightGameplayEffect_Damage::UknightGameplayEffect_Damage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UknightDamageExecution::StaticClass();
	Executions.Add(Execution);
}
