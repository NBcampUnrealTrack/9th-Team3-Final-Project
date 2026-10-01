#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "knightMonsterMovementComponent.generated.h"

/**
 * Movement for giant monsters. Most motion comes from root motion in attack and locomotion montages;
 * this component supplies slow, heavy defaults for the rest and a turn helper for AI.
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
	 * Returns true once facing within ToleranceDegrees. For turns with a montage, play the montage instead.
	 */
	bool TurnTowards(const FVector& WorldLocation, float DeltaTime, float TurnRate, float ToleranceDegrees);
};
