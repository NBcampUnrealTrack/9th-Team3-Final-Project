#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "knightPartScanSubsystem.generated.h"

class APostProcessVolume;
class UknightMonsterPartShellComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/**
 * 부위 탐지 스캔. 한 점에서 링이 맵 전역으로 퍼지는 포스트 프로세스를 띄우고, 링이 닿은 몬스터의 부위 셸을 켠다.
 *
 * 경계 없는 포스트 프로세스 볼륨을 하나 만들어 쓰므로 플레이어 카메라 설정은 건드리지 않는다.
 * 콘솔 `knight.PartScan`으로 플레이어 위치에서 바로 쏠 수 있다.
 */
UCLASS()
class KNIGHT_API UknightPartScanSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterShell(UknightMonsterPartShellComponent* Shell);
	void UnregisterShell(UknightMonsterPartShellComponent* Shell);

	/** Origin에서 스캔을 쏜다. 이미 퍼지는 중이면 처음부터 다시 쏜다. */
	void StartScan(const FVector& Origin, UMaterialInterface* ScanMaterial, float Speed, float MaxRadius);

	bool IsScanning() const { return bScanning; }

	/** 등록된 셸 중 첫 번째(콘솔 명령이 스캔 머티리얼·속도를 빌릴 때). */
	UknightMonsterPartShellComponent* GetAnyShell() const;

	void ForEachShell(TFunctionRef<void(UknightMonsterPartShellComponent&)> Function) const;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bScanning; }
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

private:
	void StopScan();

	UPROPERTY(Transient)
	TObjectPtr<APostProcessVolume> Volume;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ScanMID;

	TArray<TWeakObjectPtr<UknightMonsterPartShellComponent>> Shells;
	/** 이번 스캔에서 이미 켠 셸. */
	TSet<TWeakObjectPtr<UknightMonsterPartShellComponent>> Revealed;

	FVector ScanOrigin = FVector::ZeroVector;
	float ScanRadius = 0.f;
	float ScanSpeed = 0.f;
	float ScanMaxRadius = 0.f;
	bool bScanning = false;
};
