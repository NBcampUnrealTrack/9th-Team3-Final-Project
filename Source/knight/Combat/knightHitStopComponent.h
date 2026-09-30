#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "knightHitStopComponent.generated.h"

/**
 * Freezes only the owner for a moment by lowering its CustomTimeDilation. The world, UI, audio and
 * camera keep running, unlike global time dilation. Overlapping requests extend the freeze.
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightHitStopComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UknightHitStopComponent();

	UFUNCTION(BlueprintCallable, Category="Knight|Combat")
	void ApplyHitStop(float Duration);

	UFUNCTION(BlueprintCallable, Category="Knight|Combat")
	void StopHitStop();

	UFUNCTION(BlueprintPure, Category="Knight|Combat")
	bool IsInHitStop() const { return bActive; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Owner time dilation during a hit stop. Near zero, not zero, so animation keeps a trace of motion. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat", meta=(ClampMin=0.001, ClampMax=1))
	float HitStopTimeDilation = 0.02f;

private:
	FTimerHandle HitStopTimer;
	float RestoreTimeDilation = 1.f;
	bool bActive = false;
};
