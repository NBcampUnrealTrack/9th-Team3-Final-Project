#include "Animation/knightAnimNode_StanceLegs.h"

#include "Animation/AnimInstanceProxy.h"
#include "HAL/IConsoleManager.h"
#include "TwoBoneIK.h"

namespace knightStanceLegs
{
	static TAutoConsoleVariable<int32> CVarEnabled(TEXT("knight.Anim.StanceLegs"), 1,
		TEXT("1 = narrow and straighten the knight's combat stance legs (Knight Stance Legs anim node), 0 = off for A/B comparison."));
}

void FAnimNode_knightStanceLegs::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	PelvisBone.Initialize(RequiredBones);
	Chains.Reset();
	const FReferenceSkeleton& RefSkeleton = RequiredBones.GetReferenceSkeleton();
	for (FBoneReference& Foot : FootBones)
	{
		Foot.Initialize(RequiredBones);
		// 발 -> 무릎 -> 허벅지 순서로 부모를 따라 올라간다.
		const int32 FootIndex = RefSkeleton.FindBoneIndex(Foot.BoneName);
		const int32 JointIndex = FootIndex != INDEX_NONE ? RefSkeleton.GetParentIndex(FootIndex) : INDEX_NONE;
		const int32 RootIndex = JointIndex != INDEX_NONE ? RefSkeleton.GetParentIndex(JointIndex) : INDEX_NONE;
		if (RootIndex == INDEX_NONE)
		{
			continue;
		}
		FChain Chain;
		Chain.Foot = Foot;
		Chain.Joint = FBoneReference(RefSkeleton.GetBoneName(JointIndex));
		Chain.Root = FBoneReference(RefSkeleton.GetBoneName(RootIndex));
		Chain.Joint.Initialize(RequiredBones);
		Chain.Root.Initialize(RequiredBones);
		if (Chain.Foot.IsValidToEvaluate(RequiredBones) && Chain.Joint.IsValidToEvaluate(RequiredBones) && Chain.Root.IsValidToEvaluate(RequiredBones))
		{
			Chains.Add(Chain);
		}
	}
}

bool FAnimNode_knightStanceLegs::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	return knightStanceLegs::CVarEnabled.GetValueOnAnyThread() != 0 && PelvisBone.IsValidToEvaluate(RequiredBones) && !Chains.IsEmpty();
}

void FAnimNode_knightStanceLegs::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	const FVector Lateral = LateralAxis.GetSafeNormal();
	const FVector Forward = ForwardAxis.GetSafeNormal();

	// 1) 골반을 올린다. 다리 체인은 모두 골반의 자손이라 같은 만큼 따라 올라간다.
	const FCompactPoseBoneIndex PelvisIndex = PelvisBone.GetCompactPoseIndex(BoneContainer);
	const FTransform PelvisOld = Output.Pose.GetComponentSpaceTransform(PelvisIndex);
	FTransform PelvisNew = PelvisOld;
	PelvisNew.AddToTranslation(FVector(0.0, 0.0, PelvisRaise));
	OutBoneTransforms.Add(FBoneTransform(PelvisIndex, PelvisNew));
	const FTransform PelvisDelta = PelvisOld.Inverse() * PelvisNew;
	const FVector PelvisLocation = PelvisOld.GetLocation();

	for (const FChain& Chain : Chains)
	{
		const FCompactPoseBoneIndex Root = Chain.Root.GetCompactPoseIndex(BoneContainer);
		const FCompactPoseBoneIndex Joint = Chain.Joint.GetCompactPoseIndex(BoneContainer);
		const FCompactPoseBoneIndex Foot = Chain.Foot.GetCompactPoseIndex(BoneContainer);
		const FTransform FootOld = Output.Pose.GetComponentSpaceTransform(Foot);
		FTransform RootCS = Output.Pose.GetComponentSpaceTransform(Root) * PelvisDelta;
		FTransform JointCS = Output.Pose.GetComponentSpaceTransform(Joint) * PelvisDelta;
		FTransform FootCS = FootOld * PelvisDelta;

		// 2) 발 목표: 원래 위치(바닥 높이 그대로)에서 좌우 간격만 골반 쪽으로 줄인다.
		const FVector FootLocation = FootOld.GetLocation();
		const double Side = FVector::DotProduct(FootLocation - PelvisLocation, Lateral);
		const FVector Effector = FootLocation + Lateral * (Side * (WidthScale - 1.0));

		// 무릎 목표: 원래 굽은 쪽(엉덩이-발 중점에서 무릎 방향)으로 밀고, 앞으로도 민다.
		const FVector Mid = (RootCS.GetLocation() + FootCS.GetLocation()) * 0.5;
		const FVector JointTarget = JointCS.GetLocation() + (JointCS.GetLocation() - Mid) + Forward * KneeForward;
		AnimationCore::SolveTwoBoneIK(RootCS, JointCS, FootCS, JointTarget, Effector, false, 1.0, 1.0);
		// 발 회전은 원래 애니메이션 그대로.
		FootCS.SetRotation(FootOld.GetRotation());

		OutBoneTransforms.Add(FBoneTransform(Root, RootCS));
		OutBoneTransforms.Add(FBoneTransform(Joint, JointCS));
		OutBoneTransforms.Add(FBoneTransform(Foot, FootCS));
	}
	OutBoneTransforms.Sort(FCompareBoneTransformIndex());
}
