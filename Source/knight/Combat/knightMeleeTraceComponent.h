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
 * so fast swings do not tunnel. Overlapping windows (both claws at once) each sweep their own shape and
 * share one hit list, so each actor is hit at most once until the last overlapping window ends. Valid hits
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
	/** Ends the window that began with the same shape. */
	void EndTrace(const FknightMeleeTraceParams& Params);

	UFUNCTION(BlueprintPure, Category="Knight|Combat")
	bool IsTracing() const { return !ActiveTraces.IsEmpty(); }

	/** Native listeners (debugger, tests). Gameplay reacts to Event.Melee.Hit instead. */
	FknightMeleeHitEvent OnMeleeHit;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Object types the weapon can hit. Monster part bodies are usually PhysicsBody or Pawn. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat")
	TArray<TEnumAsByte<EObjectTypeQuery>> HitObjectTypes;

private:
	struct FActiveTrace
	{
		FknightMeleeTraceParams Params;
		TWeakObjectPtr<UMeshComponent> Mesh;
		TArray<FVector> PreviousPoints;
	};

	UMeshComponent* FindTraceMesh(FName ComponentTag) const;
	static void SampleBlade(const FActiveTrace& Trace, TArray<FVector>& OutPoints);
	void SweepFrame();
	void HandleHit(const FHitResult& Hit);
	bool IsValidTarget(AActor* Target) const;
	void StopAllTraces();

	TArray<FActiveTrace> ActiveTraces;
	/** Shared by overlapping windows; cleared when the last one ends. */
	TSet<TWeakObjectPtr<AActor>> HitActors;
};
