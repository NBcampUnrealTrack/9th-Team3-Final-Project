#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "knightAttackTrailComponent.generated.h"

class UMaterialInterface;
class USkeletalMeshComponent;

/**
 * 공격 궤적. 뼈 두 개(Base → Tip)가 지나간 자리를 띠 모양 면으로 그리고 Lifetime초 동안 남겼다가 지운다.
 *
 * 판정 노티파이(Knight Melee Trace)와 같은 뼈를 쓰면 궤적이 곧 판정이 쓸고 간 범위가 된다.
 * 프레임마다 뼈 위치를 하나씩 기록하고, 프레임 사이는 Catmull-Rom 곡선으로 메워 빠른 휘두르기도 각지지 않게 한다.
 *
 * 머티리얼에 넘기는 값:
 * - UV.x: 나이 (0 = 방금 지나간 앞쪽, 1 = 사라지기 직전)
 * - UV.y: Base 쪽 0 → Tip 쪽 1
 * - 정점 색 A: 세기. 궤적이 시작·끝나는 순간에 0에서 올라오고 내려간다.
 *
 * 월드 원점에 고정한 채 정점을 월드 좌표로 만든다. 메시를 따라 움직이지 않으므로 지나간 자리가 그대로 남는다.
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightAttackTrailComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	UknightAttackTrailComponent();

	struct FSettings
	{
		FName BaseSocket;
		FName TipSocket;
		/** Base에서 Tip 쪽으로 이만큼(0~1) 올라간 곳부터 그린다. 팔 전체가 아니라 발톱 끝 쪽만 남길 때 쓴다. */
		float BaseRatio = 0.f;
		/** Tip 바깥으로 더 늘이는 길이(월드 cm). 판정 반지름만큼 늘이면 판정 바깥 테두리와 맞는다. */
		float TipExtension = 0.f;
		float Lifetime = 0.25f;
		/** 궤적이 켜질 때 세기가 0에서 오르는 시간. */
		float FadeTime = 0.05f;
		/** Tip이 이 속도(월드 cm/s)보다 느리면 그리지 않고, FullSpeed에서 다 그린다. 휘두르기 앞뒤 느린 구간이 뭉쳐 보이지 않게 한다. */
		float MinSpeed = 0.f;
		float FullSpeed = 0.f;
	};

	/** Source의 주인 액터에 궤적을 새로 붙이고 바로 그리기 시작한다. */
	static UknightAttackTrailComponent* SpawnOn(USkeletalMeshComponent* InSource, UMaterialInterface* Material, const FSettings& InSettings);

	void Start(USkeletalMeshComponent* InSource, const FSettings& InSettings);
	/** 새 자리를 그만 남긴다. 남은 궤적이 다 사라지면 스스로 지워진다. */
	void Stop();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual int32 GetNumMaterials() const override { return 1; }

protected:
	virtual void SendRenderDynamicData_Concurrent() override;
	virtual void CreateRenderState_Concurrent(FRegisterComponentContext* Context) override;

private:
	struct FSample
	{
		FVector Base;
		FVector Tip;
		float Time = 0.f;
		/** 켜질 때의 세기와 속도 세기를 곱한 값(0~1). */
		float Strength = 1.f;
	};

	TWeakObjectPtr<USkeletalMeshComponent> Source;
	FSettings Settings;
	TArray<FSample> Samples;
	bool bEmitting = false;
	float StartTime = 0.f;

	/**
	 * 프레임 사이 보간용. 빠른 휘두르기는 한 프레임에 1m 넘게 움직여 점을 곡선으로 이어도 꺾여 보이므로,
	 * 지난 프레임과 이번 프레임 사이의 몽타주 시각에서 애니메이션 자세를 직접 읽어 중간 점을 채운다.
	 * 블렌드·IK로 실제 자세와 생기는 차이는 양 끝에서 재어 중간에 나눠 보정한다.
	 */
	struct FPrevFrame
	{
		TWeakObjectPtr<const class UAnimMontage> Montage;
		float MontagePosition = 0.f;
		FTransform MeshToWorld;
		FVector BaseError = FVector::ZeroVector;
		FVector TipError = FVector::ZeroVector;
		bool bValid = false;
	};
	FPrevFrame Prev;
	/** 루트 다음 뼈부터 BaseSocket/TipSocket까지의 스켈레톤 뼈 번호(부모 → 자식). 소켓이 뼈가 아니면 비어 있다. */
	TArray<int32> BaseChain;
	TArray<int32> TipChain;

	float GetNow() const;
	bool ReadSource(FVector& OutBase, FVector& OutTip) const;
	void MakeEdge(const FVector& RawBase, const FVector& RawTip, FVector& OutBase, FVector& OutTip) const;
	void BuildChain(FName Bone, TArray<int32>& OutChain) const;
	/** 몽타주 위치 Position에서 두 뼈의 월드 위치를 애니메이션에서 바로 계산한다. */
	bool EvaluateAt(const class UAnimMontage* Montage, float Position, const FTransform& MeshToWorld, FVector& OutBase, FVector& OutTip) const;
	/** 이번 프레임 직전까지의 중간 점을 넣고, 다음 프레임을 위해 지금 상태를 기록한다. */
	void AddInBetweenSamples(const FVector& Base, const FVector& Tip, float Now, float Strength);
	void BuildVertices(TArray<struct FDynamicMeshVertex>& OutVertices, TArray<uint32>& OutIndices) const;
};
