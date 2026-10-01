#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "knightMovementComponent.generated.h"

/**
 * Knight movement. Defaults are tuned heavier than the template (slower acceleration and turning,
 * stronger gravity) to sell the mech's weight.
 *
 * Boost flight uses the engine's MOVE_Flying physics; the flight ability starts and stops it and
 * pays the gauge. Boost dash is not a movement mode: the dash ability applies a root motion source.
 *
 * Movement state is mirrored onto the owner's ASC as loose tags (State.Flying, State.Airborne),
 * so abilities can require or block on it without asking the movement component.
 */
UCLASS()
class KNIGHT_API UknightMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UknightMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Enters boost flight. Returns false when flight is not possible (no updated component, already flying). */
	UFUNCTION(BlueprintCallable, Category="Knight|Movement")
	bool StartFlight();

	/** Leaves boost flight and falls. */
	UFUNCTION(BlueprintCallable, Category="Knight|Movement")
	void StopFlight();

	UFUNCTION(BlueprintPure, Category="Knight|Movement")
	bool IsInFlight() const { return MovementMode == MOVE_Flying; }

	/** Adds vertical input during flight. Scale is -1..1 (descend..ascend). */
	void AddFlightVerticalInput(float Scale);

protected:
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	void SyncMovementTags() const;
};
