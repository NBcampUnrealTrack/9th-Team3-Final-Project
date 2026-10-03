#pragma once

#include "CoreMinimal.h"
#include "Combat/knightCombatTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "knightCombatLibrary.generated.h"

/** Shared combat rules, so every attack (player, monster, projectile) resolves hits the same way. */
UCLASS()
class KNIGHT_API UknightCombatLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Resolves one weapon hit from Attacker on Hit.GetActor():
	 *   invulnerable -> Evaded, nothing applied
	 *   parrying     -> Parried: Event.Parried to the attacker, parry cue, long hit stop, no damage
	 *   otherwise    -> damage effect with the hit result in its context (part multipliers need it),
	 *                   hit or weak point cue, hit stop; Event.Guarded to a guarding target
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Combat")
	static EknightHitOutcome ApplyMeleeHit(AActor* Attacker, const FHitResult& Hit, const FknightMeleeHitParams& Params);

	/**
	 * Reduces sweep hits to one per actor, in first-seen actor order. For each actor it keeps the first hit
	 * that resolves to a part (ICombatInterface), else its first hit. Without this, a monster's movement
	 * capsule, which overlaps its part bodies, often comes first and the hit loses its part.
	 */
	static void SelectBestHitPerActor(TArrayView<const FHitResult> Hits, TArray<FHitResult>& OutHits);

	/** Points a motion warp target at the attacker's locked target, clamped to MaxDistance. Returns false with no lock. */
	UFUNCTION(BlueprintCallable, Category="Knight|Combat")
	static bool SetWarpTargetToLockedTarget(AActor* Attacker, FName WarpTargetName, float MaxDistance);

private:
	static void ApplyHitStopPair(AActor* Attacker, AActor* Target, float Duration);
};
