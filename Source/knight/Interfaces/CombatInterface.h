#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "CombatInterface.generated.h"

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UCombatInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Rules for attacking and being hit. Anything that can take damage implements this and owns an
 * AbilitySystemComponent with UknightCoreAttributeSet; damage itself always flows through GAS.
 * Lock-on is separate (ITargetableInterface), so a breakable structure can be hit without being lockable.
 */
class KNIGHT_API ICombatInterface
{
	GENERATED_BODY()

public:
	/** False for dead actors and anything that should ignore this attacker right now. */
	virtual bool CanReceiveHit(const AActor* Attacker) const = 0;

	/** Maps a trace hit to a part tag under Monster.Part. Returns an empty tag for actors without parts. */
	virtual FGameplayTag ResolveHitPart(const FHitResult& Hit) const { return FGameplayTag(); }

	virtual bool IsDead() const = 0;
};
