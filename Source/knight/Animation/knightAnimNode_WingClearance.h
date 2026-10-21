#pragma once

#include "CoreMinimal.h"
#include "BoneContainer.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "knightAnimNode_WingClearance.generated.h"

/** 날개 하나. 어깨 뼈를 돌려 날개 전체를 들어 올린다. */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightWingClearanceWing
{
	GENERATED_BODY()

	/** 돌리는 뼈(날개 어깨). 날개 끝 뼈들의 조상이어야 한다. */
	UPROPERTY(EditAnywhere, Category="Wing")
	FBoneReference RootBone;

	/** 바닥 위에 있어야 하는 뼈(날개 끝들). 가장 낮은 뼈를 기준으로 든다. */
	UPROPERTY(EditAnywhere, Category="Wing")
	TArray<FBoneReference> TipBones;

	/** 지금 어깨에 더한 회전(컴포넌트 공간). 런타임 상태. */
	FQuat LiftRotation = FQuat::Identity;
};

/**
 * 바닥 가까이 떠 있는 날짐승의 날개 끝이 바닥을 뚫지 않게 한다. 날개 끝이 바닥(캡슐 바닥) + Clearance보다 내려가는 프레임에는
 * 어깨 뼈를 돌려 날개를 그만큼 들어 올린다. 든 각도는 바로 올라가고 ReleaseSpeed로 천천히 돌아오므로, 바닥 가까이에서는
 * 날갯짓이 작아지고 높이 날 때는 원래 날갯짓 그대로다.
 *
 * 먼저 몸의 앞뒤 축으로 날개를 들어 올린다(날갯짓 방향). 날개 끝이 몸 앞뒤로 뻗어 이 축으로는 안 올라가면(물기처럼 날개를 앞으로 뻗음)
 * 가장 낮은 끝을 바로 위로 드는 축으로 돌린다.
 *
 * 콘솔: knight.Anim.WingClearance 0 으로 끈다.
 */
USTRUCT(BlueprintInternalUseOnly)
struct KNIGHT_API FAnimNode_knightWingClearance : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Wings")
	TArray<FknightWingClearanceWing> Wings;

	/** 날개 끝 뼈를 바닥에서 이만큼 위에 둔다(월드 cm). 뼈는 날개 막 속에 있으니 막 두께만큼 띄운다. */
	UPROPERTY(EditAnywhere, Category="Wings", meta=(ClampMin=0, Units="cm"))
	float Clearance = 30.f;

	/** 들어 올린 각도가 돌아오는 속도(도/초). 작을수록 바닥 가까이에서 날갯짓이 오래 작게 유지된다. */
	UPROPERTY(EditAnywhere, Category="Wings", meta=(ClampMin=1, Units="deg"))
	float ReleaseSpeed = 60.f;

	/** 들어 올리는 최대 각도. 날갯짓 맨 아래에서 날개가 거의 수직으로 내려가는 몬스터(잔향)는 80도 넘게 든다. */
	UPROPERTY(EditAnywhere, Category="Wings", meta=(ClampMin=0, ClampMax=90, Units="deg"))
	float MaxLiftAngle = 90.f;

	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool HasPreUpdate() const override { return true; }

private:
	/** 바닥 높이(컴포넌트 공간, 메시 원점 기준). 게임 스레드(PreUpdate)에서 잰다. */
	double GroundCS = 0.0;
	/** 월드 cm를 컴포넌트 공간 단위로 바꾸는 배율(1 / 메시 배율). */
	double WorldToComponent = 1.0;
	bool bGroundValid = false;
	float DeltaTime = 0.f;
};
