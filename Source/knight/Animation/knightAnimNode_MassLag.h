#pragma once

#include "CoreMinimal.h"
#include "BoneContainer.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "knightAnimNode_MassLag.generated.h"

/**
 * (2026-10-20) 몸의 관성("묵직한 거대 기계"). 사람 검술 애니를 기계처럼 보이게 하려고, 상체(척추) 회전이 애니메이션을
 * 바로 따라가지 않고 무거운 질량처럼 스프링으로 늦게 따라가게 한다.
 *   - 휘두르기 시작: 상체가 칼보다 살짝 늦게 돈다(끌려감).
 *   - 멈춤: 상체가 조금 더 밀렸다가(넘침) 제자리로 돌아온다.
 * 뼈마다 부모 기준 회전을 스프링(Stiffness·Damping)으로 따라가게 하고, 원래 애니에서 MaxLagAngle 이상 벗어나지 않게 한다.
 * 팔·머리는 척추의 자식이라 함께 따라온다. 왼손 IK(칼 손잡이)는 이 노드 뒤에 있어야 손이 칼에서 떨어지지 않는다.
 *
 * Damping이 작을수록 더 출렁이고(넘침이 큼), Stiffness가 작을수록 더 늦게 따라온다(더 무겁게).
 * 콘솔: knight.Anim.MassLag 0 으로 끄고 비교한다.
 */
USTRUCT(BlueprintInternalUseOnly)
struct KNIGHT_API FAnimNode_knightMassLag : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	/** 늦게 따라갈 뼈. 부모 → 자식 순서가 아니어도 된다. 예: spine_02 ~ spine_05. */
	UPROPERTY(EditAnywhere, Category="Mass Lag")
	TArray<FBoneReference> LagBones;

	/** 따라가는 힘. 작을수록 늦게(무겁게) 따라온다. */
	UPROPERTY(EditAnywhere, Category="Mass Lag", meta=(PinHiddenByDefault, ClampMin=1))
	float Stiffness = 90.f;

	/** 멈추는 힘. 작을수록 많이 출렁인다(2 x sqrt(Stiffness)이면 넘치지 않음). */
	UPROPERTY(EditAnywhere, Category="Mass Lag", meta=(PinHiddenByDefault, ClampMin=0))
	float Damping = 11.f;

	/** 원래 애니에서 벗어날 수 있는 최대 각도(뼈 하나당). */
	UPROPERTY(EditAnywhere, Category="Mass Lag", meta=(PinHiddenByDefault, ClampMin=0, ClampMax=60, Units="deg"))
	float MaxLagAngle = 12.f;

	//~ FAnimNode_SkeletalControlBase
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;

protected:
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;

private:
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;

	struct FLagState
	{
		FBoneReference Bone;
		FQuat Current = FQuat::Identity;
		FVector AngularVelocity = FVector::ZeroVector;
		bool bInitialized = false;
	};
	TArray<FLagState> States;
	float PendingDeltaTime = 0.f;
};
