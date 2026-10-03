#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTagContainer.h"
#include "knightMonsterMovementComponent.generated.h"

class UAbilitySystemComponent;

/**
 * Movement for giant monsters. Most motion comes from root motion in attack and locomotion montages;
 * this component supplies slow, heavy defaults for the rest and a turn helper for AI.
 *
 * While the monster has any of LockTags (a down, flinch or roar, death), it neither walks nor turns, whatever
 * the StateTree requests: a staggered monster reels in place instead of tracking the Knight. Root motion
 * from reaction montages still plays.
 *
 * The capsule is kept small (see AknightMonsterCharacterBase); body collision is per part via the physics
 * asset. Set NavAgentProps to the navigation agent matching the monster's size class.
 */
UCLASS()
class KNIGHT_API UknightMonsterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UknightMonsterMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Turns the owner toward WorldLocation by at most TurnRate degrees per second this frame.
	 * Returns true once facing within ToleranceDegrees. Does nothing and returns false while locked.
	 * For turns with a montage, play the montage instead.
	 */
	bool TurnTowards(const FVector& WorldLocation, float DeltaTime, float TurnRate, float ToleranceDegrees);

	/** True while the owner has any of LockTags. */
	bool IsMovementLocked() const;

	virtual float GetMaxSpeed() const override;
	virtual void PhysicsRotation(float DeltaTime) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category="Knight|Monster")
	FGameplayTagContainer LockTags;

private:
	mutable TWeakObjectPtr<UAbilitySystemComponent> CachedASC;
};
