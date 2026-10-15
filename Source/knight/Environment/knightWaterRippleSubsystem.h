#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "knightWaterRippleSubsystem.generated.h"

class ACharacter;
class APawn;
class UMaterialInstanceDynamic;
class UMaterialParameterCollection;
class UNiagaraSystem;
class UPhysicsAsset;
class USkeletalMeshComponent;
class USoundBase;
class UTextureRenderTarget2D;

/**
 * 얕은 물 위를 움직이는 캐릭터와 몬스터 주변에 물결을 만든다.
 *
 * 플레이어 중심 정사각형(SimulationSize)의 수면 높이를 렌더 타깃에서 파동 방정식으로 계산하고, 결과를 엔진 물 머티리얼의
 * 파동 입력(NormalAndHeight 텍스처 + MPC_Water의 SimLocation, FluidSimSize)으로 넘긴다. 물 머티리얼은 그대로 두고
 * NormalAndHeight에 OutputTarget을 넣은 인스턴스만 맵에 쓰면 된다.
 *
 * 물결의 원인은 두 가지다.
 * - 캐릭터 캡슐과 피직스 에셋의 각 바디(발, 손, 몸통, 꼬리 등)가 물에 잠긴 채 움직이면 속도에 비례해 누른다.
 *   애니메이션에 노티파이를 넣을 필요가 없다.
 * - AddImpulse(내려찍기 노티파이 등)로 한 번 세게 누른다.
 *
 * 첨벙은 바디의 가장 낮은 점으로 판정하므로 발 디딤뿐 아니라 구르기, 다운처럼 몸이 물에 부딪힐 때도 난다.
 * - 빠르게 내려오다 멈추면(디딤, 착지, 쓰러짐) SplashSystem 또는 SplashHeavySystem을 터뜨린다.
 * - 물에 잠긴 채 옆으로 빠르게 쓸고 지나가면(꼬리 휘두르기, 돌진) SplashWakeInterval마다 물보라를 남긴다.
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

	/** Bone에 붙은 피직스 바디의 가로 반폭(cm). 바디가 없으면 0. */
	static float GetBodyHalfWidth(const USkeletalMeshComponent* Mesh, FName Bone);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	/** 피직스 에셋 바디 하나. 위치는 바디 경계 상자의 가장 낮은 면 중심이다. */
	struct FBodyContact
	{
		int32 BodySetupIndex = INDEX_NONE;
		int32 BoneIndex = INDEX_NONE;
		FVector PrevLowest = FVector::ZeroVector;
		/** 이번에 내려오는 동안의 최고 속도(SplashMinDownSpeed보다 느리면 0). */
		float PeakDownSpeed = 0.f;
		double NextSplashTime = 0.0;
		double NextWakeTime = 0.0;
	};

	struct FRecentSplash
	{
		FVector2D Location;
		float Radius;
		double Time;
	};

	struct FTrackedCharacter
	{
		TWeakObjectPtr<ACharacter> Character;
		TWeakObjectPtr<UPhysicsAsset> PhysicsAsset;
		TArray<FBodyContact> Contacts;
		FVector PrevBottom = FVector::ZeroVector;
		TArray<FRecentSplash> RecentSplashes;
		float CapsuleRadius = 0.f;
		/** 1초 동안 바디가 내려오던 속도의 최댓값(디버그 로그용). */
		float MaxDownSpeed = 0.f;
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
	void UpdateContact(FTrackedCharacter& Tracked, FBodyContact& Contact, const USkeletalMeshComponent& Mesh, float MinRadius, float MinSpeed, double Now, float Elapsed);
	void AddMovementImpulse(const FVector& Location, const FVector& Prev, float SurfaceZ, float Margin, float Radius, float MinSpeed, float Elapsed);
	/** 내려오다 멈춘 순간이면 내려오던 최고 속도, 아니면 0. */
	float DetectStrike(FBodyContact& Contact, float DownSpeed, double Now) const;
	/** SizeScale = 바디 크기 / SplashReferenceBodySize x 세기. */
	void SpawnSplash(const FVector& Location, float SizeScale) const;
	/** 같은 캐릭터가 방금 근처에서 첨벙했으면 false. 아니면 기록하고 true. */
	bool ClaimSplash(FTrackedCharacter& Tracked, const FVector& Location, float Radius, double Now) const;
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

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> SplashSystem;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> SplashHeavySystem;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> SplashSound;

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
