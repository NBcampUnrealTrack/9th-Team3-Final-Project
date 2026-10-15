#pragma once

#include "CoreMinimal.h"
#include "BoneContainer.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "knightAnimNode_StanceLegs.generated.h"

/**
 * (2026-10-15) 전투 자세 다리 보정. 데모(EssentialAnimation) 대검 자세를 기사에 옮기면 다리를 너무 벌리고
 * 무릎을 깊게 굽힌다(사람 마네킹 기준 자세라 키 큰 기사에게는 과하다). 매 프레임 포즈 위에서
 *   1) 골반을 PelvisRaise만큼 올리고(무릎이 덜 굽는다),
 *   2) 발을 골반 쪽으로 모아 좌우 간격을 WidthScale배로 줄인 뒤(높이·앞뒤는 그대로라 발은 바닥에 붙어 있다),
 *   3) 다리마다 2본 IK로 맞춘다. 무릎은 원래 굽은 쪽 + KneeForward만큼 앞쪽을 향한다.
 * 발의 회전은 원래 애니메이션 그대로 둔다. 애니메이션의 발 움직임(걷기 보폭)은 앞뒤 방향이라 그대로 남는다.
 *
 * 콘솔: knight.Anim.StanceLegs 0 으로 끄고 비교한다.
 */
USTRUCT(BlueprintInternalUseOnly)
struct KNIGHT_API FAnimNode_knightStanceLegs : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	/** 다리 끝 뼈(발목). 부모(무릎)와 조부모(허벅지)로 2본 체인을 만든다. */
	UPROPERTY(EditAnywhere, Category="Legs")
	TArray<FBoneReference> FootBones;

	/** 올리는 뼈. 다리 체인의 조상이어야 한다. */
	UPROPERTY(EditAnywhere, Category="Body")
	FBoneReference PelvisBone;

	/** 골반을 올리는 높이(컴포넌트 공간 cm). 클수록 무릎이 펴진다. 너무 크면 다리가 닿지 않아 발이 뜬다. */
	UPROPERTY(EditAnywhere, Category="Body", meta=(PinHiddenByDefault, Units="cm"))
	float PelvisRaise = 6.f;

	/** 골반 기준 발의 좌우 간격 배율. 1 = 그대로, 0.7 = 30% 모은다. */
	UPROPERTY(EditAnywhere, Category="Legs", meta=(PinHiddenByDefault, ClampMin=0.3, ClampMax=1.5))
	float WidthScale = 0.75f;

	/** 무릎 목표를 앞으로 미는 거리(cm). 무릎이 바깥으로 벌어지는 것을 앞쪽으로 돌린다. */
	UPROPERTY(EditAnywhere, Category="Legs", meta=(PinHiddenByDefault, Units="cm"))
	float KneeForward = 30.f;

	/** 컴포넌트 공간의 좌우 축 (기사 메시는 X). */
	UPROPERTY(EditAnywhere, Category="Axes")
	FVector LateralAxis = FVector(1.0, 0.0, 0.0);

	/** 컴포넌트 공간의 앞 축 (기사 메시는 Y). */
	UPROPERTY(EditAnywhere, Category="Axes")
	FVector ForwardAxis = FVector(0.0, 1.0, 0.0);

	//~ FAnimNode_SkeletalControlBase
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;

private:
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;

	struct FChain
	{
		FBoneReference Root;
		FBoneReference Joint;
		FBoneReference Foot;
	};
	TArray<FChain> Chains;
};
