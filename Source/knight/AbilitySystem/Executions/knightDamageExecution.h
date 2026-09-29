#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "knightDamageExecution.generated.h"

/**
 * Converts one hit into Damage and PoiseDamage meta attribute outputs.
 *
 *   Raw      = SetByCaller.Damage + AttackPower * SetByCaller.MotionValue / 100
 *   Damage   = Raw * PartMultiplier * Critical * 100 / (100 + Defense) * Guard
 *   Poise    = SetByCaller.PoiseDamage * GuardPoise
 *
 * PartMultiplier comes from ICombatInterface on the target, using the hit result in the effect context,
 * so the hit pipeline must add the FHitResult to the context before applying the effect.
 * State.Invulnerable and State.Parrying on the target zero the hit. State.Guarding reduces it
 * when the attacker is inside the guard arc.
 *
 * Every capture reads UknightCoreAttributeSet. Source and target ASCs must both have that set:
 * GAS dereferences the owning set when it builds a capture aggregator and crashes if it is missing.
 *
 * Tune the multipliers in a Blueprint subclass and reference that subclass from the damage effects.
 */
UCLASS(Blueprintable)
class KNIGHT_API UknightDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UknightDamageExecution();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;

protected:
	/** Damage multiplier for a guarded hit. */
	UPROPERTY(EditDefaultsOnly, Category="Guard", meta=(ClampMin=0, ClampMax=1))
	float GuardDamageMultiplier = 0.2f;

	/** Poise damage multiplier for a guarded hit. */
	UPROPERTY(EditDefaultsOnly, Category="Guard", meta=(ClampMin=0, ClampMax=1))
	float GuardPoiseDamageMultiplier = 0.5f;

	/** Full width of the guard arc in degrees, centered on the target's forward vector. */
	UPROPERTY(EditDefaultsOnly, Category="Guard", meta=(ClampMin=0, ClampMax=360))
	float GuardArcDegrees = 150.f;

private:
	bool IsAttackerInGuardArc(const AActor* Target, const AActor* Attacker) const;
};
