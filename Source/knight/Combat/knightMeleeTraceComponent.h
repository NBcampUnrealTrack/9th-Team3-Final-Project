#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/knightCombatTypes.h"
#include "Engine/EngineTypes.h"
#include "knightMeleeTraceComponent.generated.h"

class UMeshComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FknightMeleeHitEvent, const FHitResult& /*Hit*/);

/**
 * Sweeps the weapon between frames while a UknightAnimNotifyState_MeleeTrace is active.
 *
 * Each frame, several points along the blade are swept from last frame's position to this frame's,
 * so fast swings do not tunnel. Each actor is hit at most once per trace window. Valid hits
 * (ICombatInterface::CanReceiveHit and hostile team) are sent to the owner's ASC as Event.Melee.Hit
 * with the hit result as target data; the running attack ability turns them into damage.
 *
 * Debug: knight.Combat.DrawMeleeTraces 1
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightMeleeTraceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UknightMeleeTraceComponent();

	void BeginTrace(const FknightMeleeTraceParams& Params);
	void EndTrace();

	UFUNCTION(BlueprintPure, Category="Knight|Combat")
	bool IsTracing() const { return TraceDepth > 0; }

	/** Native listeners (debugger, tests). Gameplay reacts to Event.Melee.Hit instead. */
	FknightMeleeHitEvent OnMeleeHit;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Object types the weapon can hit. Monster part bodies are usually PhysicsBody or Pawn. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat")
	TArray<TEnumAsByte<EObjectTypeQuery>> HitObjectTypes;

private:
	UMeshComponent* FindTraceMesh(FName ComponentTag) const;
	void SampleBlade(TArray<FVector>& OutPoints) const;
	void SweepFrame();
	void HandleHit(const FHitResult& Hit);
	bool IsValidTarget(AActor* Target) const;

	FknightMeleeTraceParams ActiveParams;
	TWeakObjectPtr<UMeshComponent> TraceMesh;
	TArray<FVector> PreviousPoints;
	TSet<TWeakObjectPtr<AActor>> HitActors;
	/** Supports overlapping trace notifies; the window closes when the last one ends. */
	int32 TraceDepth = 0;
};
