// 부스터(추진기) 컴포넌트
//
// 나이트 몸의 노즐 위치(등·팔꿈치·발)를 이름으로 모아 두고, 행동(어빌리티)이 "어느 노즐을, 몇 초, 어떤 세기, 어느 방향으로"만
// 정해서 뿜게 한다. 노즐마다 이펙트를 만들고 위치를 계산하는 코드를 행동마다 따로 두지 않기 위한 것.
//
// 사용법:
//   1. 노즐 목록(Nozzles)은 BP_Knight → Thrusters에서 고친다. 뼈(또는 소켓) + 몸 기준 위치(X 앞, Y 오른쪽, Z 위).
//      뿜는 순간 그 위치를 뼈 기준으로 바꿔 붙이므로, 구르기처럼 몸이 뒤집혀도 노즐이 몸에 붙어 따라간다.
//   2. 행동은 FknightThrustSettings 칸(노즐 이름·방향·세기·시간·이펙트)을 두고 StartThrust를 부른다. 시간이 0이면 StopThrust까지.
//   3. 위치를 맞출 때는 bDebugShowNozzles를 켜면 PIE에서 모든 노즐 위치(노란 구)가 보이고,
//      bDebugDrawThrusts를 켜면 뿜는 동안 노즐(초록 구)과 뿜는 방향(빨간 선)이 보인다.
// 이펙트는 로컬 -Z로 뿜는 시스템(NS_Knight_Thruster)을 기준으로 방향을 맞춘다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "knightThrusterComponent.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class USkeletalMeshComponent;

/** 노즐 하나. */
USTRUCT(BlueprintType)
struct FknightThrusterNozzle
{
	GENERATED_BODY()

	/** 행동이 이 이름으로 노즐을 고른다. 예: Back_C, Arm_R, Foot_L. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thruster")
	FName Name;

	/** 붙일 뼈 또는 소켓. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thruster")
	FName Bone;

	/** 뼈 위치에서 더할 위치(몸 기준: X 앞, Y 오른쪽, Z 위). 뿜는 순간 뼈 기준으로 바꿔 붙인다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thruster")
	FVector Offset = FVector::ZeroVector;

	/** 이 노즐만의 크기 배율(행동의 세기에 곱한다). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thruster", meta=(ClampMin=0.1))
	float Scale = 1.f;
};

/** 뿜는 방향. */
UENUM(BlueprintType)
enum class EknightThrustDirection : uint8
{
	/** 행동이 준 이동 방향의 반대로 뿜는다(그쪽으로 밀어 주는 느낌). 이동 방향이 없으면 몸 뒤쪽. */
	OppositeMove,
	/** 몸 뒤쪽으로 뿜는다. */
	ActorBack,
	/** 아래로 뿜는다(떠 있는 느낌). */
	Down,
	/** 위로 뿜는다(아래로 꽂히는 느낌). */
	Up,
};

/** 행동 하나가 부스터를 뿜는 방법. 어빌리티 칸으로 두고 StartThrust에 넘긴다. */
USTRUCT(BlueprintType)
struct FknightThrustSettings
{
	GENERATED_BODY()

	/** 켤 노즐 이름(부스터 컴포넌트의 Nozzles). 비우면 뿜지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thruster")
	TArray<FName> Nozzles;

	/** 이펙트. 비우면 뿜지 않는다. 로컬 -Z로 뿜는 시스템(NS_Knight_Thruster). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thruster")
	TObjectPtr<UNiagaraSystem> Effect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thruster")
	EknightThrustDirection Direction = EknightThrustDirection::OppositeMove;

	/** OppositeMove·ActorBack일 때 뿜는 방향을 아래로 기울이는 각도. 0이면 수평. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thruster", meta=(ClampMin=0, ClampMax=80, Units="deg"))
	float DownAngle = 20.f;

	/** 이펙트 크기(세기). 노즐 배율과 곱한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thruster", meta=(ClampMin=0.1))
	float Scale = 1.f;

	/** 0보다 크면 이 시간 뒤 스스로 끈다. 0이면 StopThrust를 부를 때까지 뿜는다. 끈 뒤에도 이미 나온 불꽃은 끝까지 보인다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thruster", meta=(ClampMin=0, Units="s"))
	float Duration = 0.f;
};

UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightThrusterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UknightThrusterComponent();

	/**
	 * Settings의 노즐들에서 뿜기 시작한다. MoveDirection은 OppositeMove에서 쓰는 이동 방향(수평).
	 * 돌려준 번호로 StopThrust를 부른다. 뿜은 노즐이 하나도 없으면 0.
	 */
	int32 StartThrust(const FknightThrustSettings& Settings, const FVector& MoveDirection = FVector::ZeroVector);

	/** StartThrust가 돌려준 번호의 분사를 끈다. 이미 꺼졌으면 아무것도 하지 않는다. */
	void StopThrust(int32 ThrustId);

	/** 뿜고 있는 모든 분사를 끈다. */
	void StopAllThrusts();

	const TArray<FknightThrusterNozzle>& GetNozzles() const { return Nozzles; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 노즐 목록. 기본값: 등 가운데·양쪽(가슴뼈), 팔꿈치 양쪽(아래팔 뼈), 발 양쪽(thruster 소켓). 바뀔 수 있는 값. */
	UPROPERTY(EditDefaultsOnly, Category="Thruster")
	TArray<FknightThrusterNozzle> Nozzles;

	/** 켜면 PIE에서 모든 노즐 위치를 노란 구로 계속 그린다(위치 맞추기용). */
	UPROPERTY(EditDefaultsOnly, Category="Thruster|Debug")
	bool bDebugShowNozzles = false;

	/** 켜면 뿜는 동안 노즐(초록 구)과 뿜는 방향(빨간 선)을 그리고 로그를 남긴다. */
	UPROPERTY(EditDefaultsOnly, Category="Thruster|Debug")
	bool bDebugDrawThrusts = false;

private:
	/** 진행 중인 분사 하나(StartThrust 한 번). */
	struct FActiveThrust
	{
		int32 Id = 0;
		TArray<TWeakObjectPtr<UNiagaraComponent>> Jets;
		FVector EmitDirection = FVector::ZeroVector;
		FTimerHandle StopTimer;
	};

	const FknightThrusterNozzle* FindNozzle(FName Name) const;
	USkeletalMeshComponent* GetOwnerMesh() const;

	/** 노즐의 현재 세계 위치(뼈 위치 + 몸 기준 위치). */
	FVector GetNozzleWorldLocation(const USkeletalMeshComponent& Mesh, const FknightThrusterNozzle& Nozzle) const;

	/** 뿜는 방향(세계, 단위 벡터). */
	FVector GetEmitDirection(const FknightThrustSettings& Settings, const FVector& MoveDirection) const;

	/** 디버그 그리기가 필요할 때만 틱을 켠다. */
	void UpdateTickEnabled();

	TArray<FActiveThrust> ActiveThrusts;
	int32 NextThrustId = 1;
};
