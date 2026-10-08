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
};

/**
 * 큰 몬스터의 발을 지형에 맞춘다. 캡슐 하나가 몸 전체를 받치므로 경사나 요철에서 발이 뜨거나 묻힌다.
 * 다리마다 발밑 지면 높이를 재서 (1) 골반을 그 지면 평면에 맞춰 내리고 기울인 뒤 (2) 남은 차이를 다리별
 * 2본 IK로 맞춘다. 평지에서는 모든 높이 차가 0이라 포즈를 바꾸지 않는다.
 *
 * 지면 트레이스는 게임 스레드(PreUpdate)에서 이전 프레임의 발 위치로 하고, 포즈 계산은 워커 스레드에서 한다.
 * 다리 수는 자유다. 3개 미만이면 기울이기 없이 가장 낮은 발만큼 골반을 내린다(2족).
 *
 * 콘솔: knight.Anim.LegPlacement 0 으로 끄고, knight.Anim.DrawLegPlacement 1 로 트레이스를 그린다.
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

	//~ FAnimNode_Base
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;

	//~ FAnimNode_SkeletalControlBase
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;

private:
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;

	/** 월드 cm를 컴포넌트 공간 단위로 바꾼다(메시 스케일). 게임 스레드에서 갱신. */
	float WorldToComponent = 1.f;
};
