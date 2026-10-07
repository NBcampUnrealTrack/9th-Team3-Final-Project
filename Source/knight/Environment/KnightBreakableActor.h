#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KnightBreakableActor.generated.h"

class UBoxComponent;
class UNiagaraSystem;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;

/** A placed mesh that disappears when the player-controlled Knight touches its trigger. */
UCLASS(Blueprintable)
class KNIGHT_API AKnightBreakableActor : public AActor
{
	GENERATED_BODY()

public:
	AKnightBreakableActor();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Uses the same one-shot path as player contact. */
	UFUNCTION(BlueprintCallable, Category = "Breakable")
	void BreakObject();

private:
	UFUNCTION()
	void OnContactBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breakable|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	/** Set Static Mesh and Materials on this component for each placed instance. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breakable|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	/** Query-only trigger. With auto fit off, edit its Box Extent and transform directly. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breakable|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> ContactBox;

	/** Fit an axis-aligned box around the mesh's relative location, rotation, and scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Contact", meta = (AllowPrivateAccess = "true"))
	bool bAutoFitContactBox = true;

	/** Extra reach beyond each side of the mesh, in actor local units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Contact", meta = (AllowPrivateAccess = "true", EditCondition = "bAutoFitContactBox", ClampMin = "0.0", UIMin = "0.0"))
	FVector ContactPadding = FVector(40.0, 40.0, 40.0);

	/** Optional one-shot Niagara system, spawned independently in the world. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|FX", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> DustEffect;

	/** Spawn offset in the actor's local space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|FX", meta = (AllowPrivateAccess = "true", MakeEditWidget = "true"))
	FVector DustOffset = FVector::ZeroVector;

	/** Niagara world scale, independent of the mesh scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|FX", meta = (AllowPrivateAccess = "true"))
	FVector DustScale = FVector::OneVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Breakable", meta = (AllowPrivateAccess = "true"))
	bool bIsBroken = false;
};
