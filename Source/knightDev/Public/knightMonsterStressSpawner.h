#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "knightMonsterStressSpawner.generated.h"

class AknightMonsterCharacterBase;

/**
 * Test-map helper for repeatable monster measurements: spawns Count monsters on a grid around itself and,
 * optionally, records a knightPerfTracker capture labelled <CaptureLabel>_<Count>.
 *
 * Typical before/after run: place one in the test map, set MonsterClass, Count and a fixed RandomSeed,
 * enable bAutoCapture, and play (Standalone gives cleaner numbers than PIE). Run once on the old code with
 * label Before and once on the new code with label After, then: knight.Perf.Compare Before_20 After_20
 *
 * Console: knight.Monster.StressSpawn <Count> respawns every spawner in the world with that count.
 */
UCLASS(Blueprintable)
class KNIGHTDEV_API AknightMonsterStressSpawner : public AActor
{
	GENERATED_BODY()

public:
	AknightMonsterStressSpawner();

	/** Despawns what this spawner spawned before and spawns InCount monsters. */
	UFUNCTION(BlueprintCallable, Category="Knight|Test")
	void SpawnMonsters(int32 InCount);

	UFUNCTION(BlueprintCallable, Category="Knight|Test")
	void DespawnMonsters();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, Category="Spawn")
	TSubclassOf<AknightMonsterCharacterBase> MonsterClass;

	UPROPERTY(EditAnywhere, Category="Spawn", meta=(ClampMin=1, ClampMax=200))
	int32 Count = 1;

	/** Grid cell size. Keep it larger than the monster so spawns do not overlap. */
	UPROPERTY(EditAnywhere, Category="Spawn", meta=(ClampMin=100, Units="cm"))
	float Spacing = 800.f;

	UPROPERTY(EditAnywhere, Category="Spawn")
	bool bSpawnOnBeginPlay = true;

	/** Seeds FMath::Rand so move choices repeat between runs as far as frame timing allows. 0 leaves it unseeded. */
	UPROPERTY(EditAnywhere, Category="Spawn")
	int32 RandomSeed = 0;

	/** Starts a capture after spawning. */
	UPROPERTY(EditAnywhere, Category="Capture")
	bool bAutoCapture = false;

	/** Saved as <CaptureLabel>_<Count>. */
	UPROPERTY(EditAnywhere, Category="Capture", meta=(EditCondition="bAutoCapture"))
	FString CaptureLabel = TEXT("Monster");

	UPROPERTY(EditAnywhere, Category="Capture", meta=(EditCondition="bAutoCapture", ClampMin=1, Units="s"))
	float CaptureDuration = 30.f;

	/** Skips spawn hitches, shader compiles and the first perception pass. */
	UPROPERTY(EditAnywhere, Category="Capture", meta=(EditCondition="bAutoCapture", ClampMin=0, Units="s"))
	float CaptureWarmup = 5.f;

	/** Ends the game (or PIE) once the capture has been saved, for unattended runs. */
	UPROPERTY(EditAnywhere, Category="Capture", meta=(EditCondition="bAutoCapture"))
	bool bQuitAfterCapture = false;

private:
	void HandleCaptureFinished();

	UPROPERTY(Transient)
	TArray<TObjectPtr<AknightMonsterCharacterBase>> SpawnedMonsters;

	FTimerHandle CaptureTimer;
};
