#pragma once

#include "CoreMinimal.h"
#include "BoneContainer.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "knightAnimNode_LookAt.generated.h"

class AknightMonsterCharacterBase;

/** 타깃을 볼 때 돌리는 뼈 하나와 몫. 목뿌리부터 머리까지 순서대로 둔다. */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightLookAtBone
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Look At")
	FBoneReference Bone;

	/** 전체 회전 중 이 뼈가 맡는 몫. 사슬의 몫을 더하면 1이 되게 둔다(머리만 돌리면 목이 꺾여 보인다). */
	UPROPERTY(EditAnywhere, Category="Look At", meta=(ClampMin=0, ClampMax=1))
	float Weight = 0.25f;
};

/**
 * 몬스터가 대기·이동·노려보기·옆걸음 중에 목과 머리를 타깃 쪽으로 돌린다.
 * 공격(Ability.Attack, 이동 행동 Ability.Move 제외), 반응(State.Monster.Reacting), 사망 중에는 BlendTime 동안 풀어서
 * 공격 모션의 머리 움직임을 덮지 않는다. bDisableWhenAirborne이면 공중 턴(State.Monster.Airborne)에도 푼다.
 *
 * 각도: 몸(액터) 정면 기준으로 타깃까지의 yaw·pitch를 MaxYaw·MaxPitch로 자르고, TurnSpeed로 따라간다.
 * 타깃이 등 뒤로 넘어가도 머리가 튀지 않고 한계 각도에 머문다. 회전은 Bones 사슬에 Weight 몫씩 나눠 뿌리부터 얹는다.
 * 메시마다 정면 축이 달라도(잔열 +Y, 잔향 +X) 액터 정면을 컴포넌트 공간으로 바꿔 계산하므로 같은 값을 쓴다.
 *
 * 무엇을 볼지는 게임 스레드(PreUpdate)에서 몬스터의 현재 타깃과 머리 뼈 위치로 정한다.
 * 콘솔: knight.Anim.LookAt 0 으로 끈다.
 */
USTRUCT(BlueprintInternalUseOnly)
struct KNIGHT_API FAnimNode_knightLookAt : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	/** 목뿌리 → 머리 순서. 마지막 뼈 위치에서 타깃을 본다. */
	UPROPERTY(EditAnywhere, Category="Look At")
	TArray<FknightLookAtBone> Bones;

	/** 좌우로 돌리는 최대 각도. */
	UPROPERTY(EditAnywhere, Category="Look At", meta=(ClampMin=0, ClampMax=120, Units="deg"))
	float MaxYaw = 60.f;

	/** 위아래로 돌리는 최대 각도. */
	UPROPERTY(EditAnywhere, Category="Look At", meta=(ClampMin=0, ClampMax=80, Units="deg"))
	float MaxPitch = 25.f;

	/** 머리가 타깃 방향을 따라가는 최대 속도(도/초). */
	UPROPERTY(EditAnywhere, Category="Look At", meta=(ClampMin=1, Units="deg"))
	float TurnSpeed = 120.f;

	/** 켜고 끌 때 섞는 시간(초). */
	UPROPERTY(EditAnywhere, Category="Look At", meta=(ClampMin=0.01, Units="s"))
	float BlendTime = 0.25f;

	/** 공중 턴(State.Monster.Airborne)에는 보지 않는다. 비행 애니메이션은 목이 몸통과 함께 크게 움직인다. */
	UPROPERTY(EditAnywhere, Category="Look At")
	bool bDisableWhenAirborne = true;

	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual bool HasPreUpdate() const override { return true; }

	/** 지금 얹고 있는 yaw·pitch(도)와 세기(0..1). 테스트·디버그용. */
	float GetCurrentYaw() const { return CurrentYaw; }
	float GetCurrentPitch() const { return CurrentPitch; }
	float GetCurrentStrength() const { return Strength; }

private:
	/** 지금 볼 수 있는 상태인지(타깃 있음, 공격·반응·사망·공중 아님). */
	static bool WantsToLook(const AknightMonsterCharacterBase& Monster, bool bDisableWhenAirborne);

	/** 게임 스레드에서 정한 목표. */
	float DesiredYaw = 0.f;
	float DesiredPitch = 0.f;
	bool bWantLook = false;
	/** 액터 공간 → 컴포넌트 공간 회전. 메시가 액터에 대해 돌아가 있는 만큼이다. */
	FQuat ActorToComponent = FQuat::Identity;

	float CurrentYaw = 0.f;
	float CurrentPitch = 0.f;
	float Strength = 0.f;
};
