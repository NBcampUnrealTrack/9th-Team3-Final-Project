#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "knightWaterRippleSubsystem.generated.h"

class ACharacter;
class APawn;
class UMaterialInstanceDynamic;
class UMaterialParameterCollection;
class UTextureRenderTarget2D;

/**
 * 얕은 물 위를 움직이는 캐릭터와 몬스터 주변에 물결을 만든다.
 *
 * 플레이어 중심 정사각형(SimulationSize)의 수면 높이를 렌더 타깃에서 파동 방정식으로 계산하고, 결과를 엔진 물 머티리얼의
 * 파동 입력(NormalAndHeight 텍스처 + MPC_Water의 SimLocation, FluidSimSize)으로 넘긴다. 물 머티리얼은 그대로 두고
 * NormalAndHeight에 OutputTarget을 넣은 인스턴스만 맵에 쓰면 된다.
 *
 * 물결의 원인은 두 가지다.
 * - 캐릭터 몸통과 BoneKeywords에 걸리는 뼈(발, 앞발 등)가 수면 근처에서 움직이면 속도에 비례해 누른다. 애니메이션에 노티파이를 넣을 필요가 없다.
 * - AddImpulse(내려찍기 노티파이 등)로 한 번 세게 누른다.
 *
 * 맵의 물 머티리얼이 OutputTarget을 쓰지 않으면(물이 없는 레벨) InactiveCheckInterval마다 확인만 하고 아무것도 계산하지 않는다.
 */
UCLASS()
class KNIGHT_API UknightWaterRippleSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 수면 높이를 찾는다. 물 경계 밖이거나 물이 없는 레벨이면 false. 시뮬레이션이 꺼져 있어도 동작한다. */
	static bool FindWaterSurface(const UWorld* World, const FVector& Location, float& OutSurfaceZ);

	/** Location 주변 Radius 안을 Depth(cm)만큼 한 번 누른다. 계산 범위 밖이거나 꺼져 있으면 무시한다. */
	void AddImpulse(const FVector& Location, float Radius, float Depth);

	bool IsSimulating() const { return bActive; }

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	struct FTrackedCharacter
	{
		TWeakObjectPtr<ACharacter> Character;
		TArray<FName> Bones;
		/** [0] = 몸통(캡슐 바닥), [1..] = Bones. */
		TArray<FVector> PrevLocations;
		float SurfaceZ = 0.f;
		/** 디버그 표시용. */
		float BottomZ = 0.f;
		bool bWaterFound = false;
		bool bInWater = false;
		bool bHasPrev = false;
		double NextQueryTime = 0.0;
	};

	struct FPendingImpulse
	{
		FVector2D Location;
		float Radius;
		float Depth;
		/** 디버그 표시용 높이. */
		float Z;
	};

	bool TryActivate();
	void RefreshCharacters();
	void UpdateCharacter(FTrackedCharacter& Tracked, double Now, float Elapsed);
	void AddMovementImpulse(const FVector& Location, const FVector& Prev, float SurfaceZ, float Margin, float Radius, float MinSpeed, float Elapsed);
	bool IsInsideSimulation(const FVector2D& Location, float Radius) const;
	void RunStep(const FVector2D& Shift, bool bWithImpulses);
	void ClearTargets();
	void DrawDebug(const APawn& Pawn) const;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> OutputTarget;

	/** 지금 높이(R)와 한 단계 전 높이(G). 번갈아 쓴다. */
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> StateTargets[2];

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> StepMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> NormalMID;

	/** 엔진 물 머티리얼이 파동 위치를 읽는 곳(MPC_Water). */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> WaterCollection;

	TArray<FTrackedCharacter> Characters;
	TArray<FPendingImpulse> PendingImpulses;

	/** 렌더 타깃 한 칸 단위로 맞춘 계산 범위 중심. */
	FVector2D Center = FVector2D::ZeroVector;
	double LastSourceTime = 0.0;
	int32 CurrentState = 0;
	int32 Resolution = 0;
	float StepAccumulator = 0.f;
	double NextActivationCheck = 0.0;
	double NextCharacterRefresh = 0.0;
	double NextTestPulse = 0.0;
	double NextDebugLog = 0.0;
	bool bActive = false;
	bool bHasCenter = false;
};
