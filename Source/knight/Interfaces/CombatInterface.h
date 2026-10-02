#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "CombatInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
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
	virtual bool CanReceiveHit(const AActor* Attacker) const { return false; }

	/** Maps a trace hit to a part tag under Monster.Part. Returns an empty tag for actors without parts. */
	virtual FGameplayTag ResolveHitPart(const FHitResult& Hit) const { return FGameplayTag(); }

	/** Hitzone multiplier for a part from ResolveHitPart. Weak points are simply parts above 1. */
	virtual float GetPartDamageMultiplier(const FGameplayTag& PartTag) const { return 1.f; }

	virtual bool IsDead() const { return false; }

	/** Blueprint entry points delegate to the existing native contract by default. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Knight|Combat")
	bool CanReceiveHitBP(AActor* Attacker) const;
	virtual bool CanReceiveHitBP_Implementation(AActor* Attacker) const { return CanReceiveHit(Attacker); }

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Knight|Combat")
	bool IsDeadBP() const;
	virtual bool IsDeadBP_Implementation() const { return IsDead(); }

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Knight|Combat")
	FGameplayTag ResolveHitPartBP(const FHitResult& Hit) const;
	virtual FGameplayTag ResolveHitPartBP_Implementation(const FHitResult& Hit) const { return ResolveHitPart(Hit); }

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Knight|Combat")
	float GetPartDamageMultiplierBP(FGameplayTag PartTag) const;
	virtual float GetPartDamageMultiplierBP_Implementation(FGameplayTag PartTag) const { return GetPartDamageMultiplier(PartTag); }

	/** Optional requests for a Blueprint combat implementation. GAS remains authoritative for existing actors. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Knight|Combat")
	bool RequestAttack(AActor* Target);
	virtual bool RequestAttack_Implementation(AActor* Target) { return false; }

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Knight|Combat")
	void ReceiveHit(AActor* Attacker, const FHitResult& Hit, float BaseDamage);
	virtual void ReceiveHit_Implementation(AActor* Attacker, const FHitResult& Hit, float BaseDamage) {}
};
