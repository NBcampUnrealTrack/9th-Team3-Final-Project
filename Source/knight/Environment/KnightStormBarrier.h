#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KnightStormBarrier.generated.h"

class UBoxComponent;
class ANiagaraActor;
class UNiagaraComponent;
class UPrimitiveComponent;
class USceneComponent;

/** One independently controlled section of an invisible arena wall and its storm effect. */
UCLASS(Blueprintable)
class KNIGHT_API AKnightStormBarrier : public AActor
{
	GENERATED_BODY()

public:
	AKnightStormBarrier();

	/** Assign a NiagaraActor already placed in this level to each barrier instance. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Storm Barrier")
	TObjectPtr<ANiagaraActor> TargetStormActor;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnDetectionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnDetectionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	bool IsControlledKnightCapsule(const AActor* OtherActor, const UPrimitiveComponent* OtherComp) const;
	UNiagaraComponent* GetTargetStormComponent() const;
	void RefreshStormState();
	void SetStormEnabled(bool bEnable);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm Barrier|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	/** Blocking wall; edit Box Extent and Transform on each placed instance. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm Barrier|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> WallCollision;

	/** Query-only proximity volume; make this larger than WallCollision. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm Barrier|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> DetectionCollision;

	bool bStormEnabled = false;
};
