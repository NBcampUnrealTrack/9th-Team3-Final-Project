#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "knightMonsterStressSpawner.generated.h"

class AknightMonsterCharacterBase;

/**
 * 반복 가능한 몬스터 성능 측정을 위한 테스트 맵 도우미. 자기 주변 격자에 Count마리의 몬스터를 스폰하고,
 * 선택적으로 <CaptureLabel>_<Count> 이름으로 knightPerfTracker 측정(캡처)을 기록한다.
 *
 * 일반적인 전후 비교 방법: 테스트 맵에 하나 배치하고 MonsterClass, Count, 고정 RandomSeed를 설정한 뒤
 * bAutoCapture를 켜고 플레이한다 (PIE보다 Standalone이 더 깨끗한 수치를 준다). 이전 코드에서 Before 라벨로,
 * 새 코드에서 After 라벨로 한 번씩 실행한 다음 비교한다: knight.Perf.Compare Before_20 After_20
 *
 * 콘솔: knight.Monster.StressSpawn <Count>로 월드의 모든 스포너를 해당 수만큼 다시 스폰한다.
 */
UCLASS(Blueprintable)
class KNIGHTDEV_API AknightMonsterStressSpawner : public AActor
{
	GENERATED_BODY()

public:
	AknightMonsterStressSpawner();

	/** 이 스포너가 이전에 스폰한 몬스터를 제거하고 InCount마리를 새로 스폰한다. */
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

	/** 격자 칸 크기. 스폰된 몬스터끼리 겹치지 않도록 몬스터보다 크게 잡는다. */
	UPROPERTY(EditAnywhere, Category="Spawn", meta=(ClampMin=100, Units="cm"))
	float Spacing = 800.f;

	UPROPERTY(EditAnywhere, Category="Spawn")
	bool bSpawnOnBeginPlay = true;

	/** FMath::Rand의 시드를 고정해, 프레임 타이밍이 허용하는 한 실행마다 같은 기술 선택이 반복되게 한다. 0이면 시드를 고정하지 않는다. */
	UPROPERTY(EditAnywhere, Category="Spawn")
	int32 RandomSeed = 0;

	/** 스폰 후 측정(캡처)을 시작한다. */
	UPROPERTY(EditAnywhere, Category="Capture")
	bool bAutoCapture = false;

	/** <CaptureLabel>_<Count> 이름으로 저장된다. */
	UPROPERTY(EditAnywhere, Category="Capture", meta=(EditCondition="bAutoCapture"))
	FString CaptureLabel = TEXT("Monster");

	UPROPERTY(EditAnywhere, Category="Capture", meta=(EditCondition="bAutoCapture", ClampMin=1, Units="s"))
	float CaptureDuration = 30.f;

	/** 스폰 히치, 셰이더 컴파일, 첫 인지(perception) 처리를 측정에서 건너뛰기 위한 워밍업 시간. */
	UPROPERTY(EditAnywhere, Category="Capture", meta=(EditCondition="bAutoCapture", ClampMin=0, Units="s"))
	float CaptureWarmup = 5.f;

	/** 무인 실행용. 측정 결과가 저장되면 게임(또는 PIE)을 종료한다. */
	UPROPERTY(EditAnywhere, Category="Capture", meta=(EditCondition="bAutoCapture"))
	bool bQuitAfterCapture = false;

private:
	void HandleCaptureFinished();

	UPROPERTY(Transient)
	TArray<TObjectPtr<AknightMonsterCharacterBase>> SpawnedMonsters;

	FTimerHandle CaptureTimer;
};
