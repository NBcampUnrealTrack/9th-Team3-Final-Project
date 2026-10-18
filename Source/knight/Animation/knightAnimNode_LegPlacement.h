#pragma once

#include "CoreMinimal.h"
#include "BoneContainer.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "knightAnimNode_LegPlacement.generated.h"

/** 다리 하나. 발 뼈만 지정하면 그 부모(무릎·팔꿈치)와 조부모(허벅지·위팔)로 2본 IK 체인을 만든다. */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightLegPlacementLeg
{
	GENERATED_BODY()

	/** IK 끝 뼈(발목). 잔열이면 front_paw_L, hind_paw_L처럼 발가락이 아닌 발목 뼈. */
	UPROPERTY(EditAnywhere, Category="Leg")
	FBoneReference FootBone;

	// 아래는 런타임 상태(직렬화하지 않음).
	FBoneReference JointBone;
	FBoneReference RootBone;
	/** 캡슐 바닥(애니메이션이 가정한 평지) 대비 발밑 지면 높이. 월드 cm, 보간된 값. */
	float Offset = 0.f;
	bool bChainValid = false;

	/** 접지 커브 이름 "FootLock_<발 뼈>". 값이 0.5를 넘으면 발이 땅에 닿은 것으로 본다. */
	FName LockCurve;
	/** 고정한 발의 월드 위치(XY만 쓴다)와, 고정한 순간 컴포넌트의 요(도). */
	FVector LockedLocation = FVector::ZeroVector;
	double LockedYaw = 0.0;
	/** 1 = 고정 위치, 0 = 애니메이션 위치. 고정할 때 바로 1, 풀 때 FootLockReleaseTime 동안 0으로. */
	float LockAlpha = 0.f;
	bool bLocked = false;
	/** 다리가 닿지 않아 강제로 풀었으면, 커브가 한 번 내려갈(발을 들) 때까지 다시 고정하지 않는다. */
	bool bWaitForLift = false;
};

/**
 * 큰 몬스터의 발을 지형에 맞춘다. 캡슐 하나가 몸 전체를 받치므로 경사나 요철에서 발이 뜨거나 묻힌다.
 * 다리마다 발밑 지면 높이를 재서 (1) 골반을 그 지면 평면에 맞춰 내리고 기울인 뒤 (2) 남은 차이를 다리별
 * 2본 IK로 맞춘다. 평지에서는 모든 높이 차가 0이라 포즈를 바꾸지 않는다.
 *
 * 지면 트레이스는 게임 스레드(PreUpdate)에서 이전 프레임의 발 위치로 하고, 포즈 계산은 워커 스레드에서 한다.
 * 다리 수는 자유다. 3개 미만이면 기울이기 없이 가장 낮은 발만큼 골반을 내린다(2족).
 *
 * 발 고정: 애니메이션에 "FootLock_<발 뼈>" 커브가 있으면, 커브가 켜진(발이 땅에 닿은) 동안 발의 월드 XY와 요를
 * 붙잡아 둔다. 몸이 코드로 돌거나 밀려도 디딘 발이 미끄러지지 않는다. 커브가 없는 애니메이션은 지금과 같다.
 *
 * 콘솔: knight.Anim.LegPlacement 0 으로 끄고, knight.Anim.DrawLegPlacement 1 로 트레이스를 그린다.
 * knight.Anim.FootLock 0 으로 발 고정만 끈다. 그리기를 켜면 고정된 발 위치도 노란 구로 보인다.
 */
USTRUCT(BlueprintInternalUseOnly)
struct KNIGHT_API FAnimNode_knightLegPlacement : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	FAnimNode_knightLegPlacement()
	{
		// 멀리 있어 낮은 LOD로 그려질 때는 발 위치가 눈에 띄지 않으니 트레이스와 IK를 건너뛴다.
		LODThreshold = 2;
	}

	UPROPERTY(EditAnywhere, Category="Legs")
	TArray<FknightLegPlacementLeg> Legs;

	/** 몸통을 내리고 기울이는 뼈. 모든 다리 체인의 조상이어야 한다(네발짐승은 pelvis). */
	UPROPERTY(EditAnywhere, Category="Body")
	FBoneReference PelvisBone;

	/** 발밑 지면 평면의 기울기를 몸통에 얼마나 반영할지. 0이면 높이만 맞추고 기울이지 않는다. */
	UPROPERTY(EditAnywhere, Category="Body", meta=(ClampMin=0, ClampMax=1))
	float BodyTiltWeight = 1.f;

	UPROPERTY(EditAnywhere, Category="Body", meta=(ClampMin=0, ClampMax=45, Units="deg"))
	float MaxBodyTilt = 20.f;

	/** 캡슐 바닥보다 이만큼 높은 지면까지 발을 올린다(월드 cm). */
	UPROPERTY(EditAnywhere, Category="Trace", meta=(ClampMin=0, Units="cm"))
	float MaxStepUp = 200.f;

	/** 캡슐 바닥보다 이만큼 낮은 지면까지 발을 내린다(월드 cm). */
	UPROPERTY(EditAnywhere, Category="Trace", meta=(ClampMin=0, Units="cm"))
	float MaxStepDown = 300.f;

	/** 발밑을 훑는 구의 반지름(월드 cm). 발바닥 크기에 맞추면 작은 요철에 덜 떨린다. */
	UPROPERTY(EditAnywhere, Category="Trace", meta=(ClampMin=0, Units="cm"))
	float TraceRadius = 30.f;

	/** 높이 차가 목표를 따라가는 속도. 클수록 빨리 붙고, 작을수록 부드럽다. */
	UPROPERTY(EditAnywhere, Category="Trace", meta=(ClampMin=0))
	float InterpSpeed = 10.f;

	/** 접지 커브가 있는 애니메이션에서 디딘 발을 월드에 고정한다. */
	UPROPERTY(EditAnywhere, Category="Foot Lock")
	bool bEnableFootLock = true;

	/** 발을 들 때 고정 위치에서 애니메이션 위치로 돌아가는 시간. */
	UPROPERTY(EditAnywhere, Category="Foot Lock", meta=(ClampMin=0, Units="s"))
	float FootLockReleaseTime = 0.15f;

	/** 고정 위치와 애니메이션 발 위치가 이만큼(월드 cm, 수평) 벌어지면 다리가 닿지 않는 것으로 보고 푼다. */
	UPROPERTY(EditAnywhere, Category="Foot Lock", meta=(ClampMin=0, Units="cm"))
	float MaxFootLockDistance = 200.f;

	/** 발 뼈의 접지 커브 이름 "FootLock_<발 뼈>". 에디터의 커브 굽기 도구도 이 이름으로 쓴다. */
	static FName MakeFootLockCurveName(FName FootBone);

	//~ FAnimNode_Base
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;

	//~ FAnimNode_SkeletalControlBase
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;

private:
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;

	/** 다리 하나의 발 고정을 갱신하고 IK 목표의 XY와 발 요 보정(도)을 돌려준다. */
	void UpdateFootLock(FknightLegPlacementLeg& Leg, float CurveValue, const FTransform& ComponentToWorld,
		const FVector& AnimFootCS, FVector& InOutEffectorCS, double& OutYawCorrection);

	/** 월드 cm를 컴포넌트 공간 단위로 바꾼다(메시 스케일). 게임 스레드에서 갱신. */
	float WorldToComponent = 1.f;

	/** 땅 위에 있고 발 고정이 켜져 있는지. 게임 스레드(PreUpdate)에서 갱신. */
	bool bFootLockActive = false;

	/** 이번 프레임 DeltaTime(UpdateInternal)과, 평가를 건너뛴 업데이트 수(LOD 등으로 끊기면 고정을 버린다). */
	float FootLockDeltaTime = 0.f;
	int32 UpdatesSinceEvaluate = 0;
};
