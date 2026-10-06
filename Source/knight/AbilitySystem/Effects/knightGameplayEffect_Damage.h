#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "knightGameplayEffect_Damage.generated.h"

/**
 * Default hit effect: instant, runs UknightDamageExecution. Pick it as FknightMeleeHitParams::DamageEffect
 * when no tuned Blueprint damage effect exists yet (test maps, new attacks). For tuned multipliers, make a
 * Blueprint effect that references a Blueprint subclass of the execution instead.
 */
UCLASS()
class KNIGHT_API UknightGameplayEffect_Damage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UknightGameplayEffect_Damage(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
