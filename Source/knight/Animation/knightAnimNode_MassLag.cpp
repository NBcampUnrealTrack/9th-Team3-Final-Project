#include "Animation/knightAnimNode_MassLag.h"

#include "Animation/AnimInstanceProxy.h"
#include "HAL/IConsoleManager.h"

namespace knightMassLag
{
	static TAutoConsoleVariable<int32> CVarEnabled(TEXT("knight.Anim.MassLag"), 1,
		TEXT("1 = upper body follows the animation with spring lag like a heavy machine (Knight Mass Lag anim node), 0 = off for A/B comparison."));
}

void FAnimNode_knightMassLag::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	States.Reset();
	for (FBoneReference& Bone : LagBones)
	{
		Bone.Initialize(RequiredBones);
		if (Bone.IsValidToEvaluate(RequiredBones))
		{
			FLagState State;
			State.Bone = Bone;
			States.Add(State);
		}
	}
	// 부모가 먼저 오도록 뼈 순서대로 정렬한다(자식의 새 위치는 부모의 새 위치를 기준으로 만든다).
	States.Sort([&RequiredBones](const FLagState& A, const FLagState& B)
	{
		return A.Bone.GetCompactPoseIndex(RequiredBones).GetInt() < B.Bone.GetCompactPoseIndex(RequiredBones).GetInt();
	});
}

bool FAnimNode_knightMassLag::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	return knightMassLag::CVarEnabled.GetValueOnAnyThread() != 0 && !States.IsEmpty();
}

void FAnimNode_knightMassLag::UpdateInternal(const FAnimationUpdateContext& Context)
{
	FAnimNode_SkeletalControlBase::UpdateInternal(Context);
	PendingDeltaTime += Context.GetDeltaTime();
}

void FAnimNode_knightMassLag::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	const float DeltaTime = PendingDeltaTime;
	PendingDeltaTime = 0.f;
	// 멈춤(일시정지·히트스톱으로 0) 또는 순간이동·끊김(너무 큼)이면 스프링을 진행하지 않거나 원래 자세로 맞춘다.
	const bool bReset = DeltaTime > 0.2f;
	const float MaxAngleRad = FMath::DegreesToRadians(MaxLagAngle);

	// 이번에 바꾼 뼈의 새 컴포넌트 공간 변환. 자식 뼈는 부모가 바뀐 경우 이것을 기준으로 다시 만든다.
	TMap<int32, FTransform> NewComponentTransforms;
	for (FLagState& State : States)
	{
		const FCompactPoseBoneIndex BoneIndex = State.Bone.GetCompactPoseIndex(BoneContainer);
		const FCompactPoseBoneIndex ParentIndex = BoneContainer.GetParentBoneIndex(BoneIndex);
		if (ParentIndex.GetInt() == INDEX_NONE)
		{
			continue;
		}
		const FTransform ParentOld = Output.Pose.GetComponentSpaceTransform(ParentIndex);
		const FTransform* const ParentChanged = NewComponentTransforms.Find(ParentIndex.GetInt());
		const FTransform ParentNew = ParentChanged ? *ParentChanged : ParentOld;
		const FTransform BoneOld = Output.Pose.GetComponentSpaceTransform(BoneIndex);
		FTransform Local = BoneOld.GetRelativeTransform(ParentOld);
		const FQuat Target = Local.GetRotation().GetNormalized();

		if (!State.bInitialized || bReset)
		{
			State.Current = Target;
			State.AngularVelocity = FVector::ZeroVector;
			State.bInitialized = true;
		}
		else if (DeltaTime > 0.f)
		{
			// 감쇠 스프링: 목표와의 차이(회전 벡터)로 각속도를 바꾸고, 그 각속도로 현재 회전을 돌린다.
			// 큰 스텝(낮은 프레임)에서도 터지지 않게 나눠 적분한다.
			const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaTime * 120.f), 1, 8);
			const float StepDelta = DeltaTime / Steps;
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				FQuat StepError = Target * State.Current.Inverse();
				StepError.EnforceShortestArcWith(FQuat::Identity);
				const FVector E = StepError.ToRotationVector();
				State.AngularVelocity += (E * Stiffness - State.AngularVelocity * Damping) * StepDelta;
				State.Current = (FQuat::MakeFromRotationVector(State.AngularVelocity * StepDelta) * State.Current).GetNormalized();
			}
			// 원래 애니에서 너무 멀어지지 않게 제한한다.
			FQuat Offset = State.Current * Target.Inverse();
			Offset.EnforceShortestArcWith(FQuat::Identity);
			const FVector OffsetVector = Offset.ToRotationVector();
			if (OffsetVector.Size() > MaxAngleRad)
			{
				State.Current = (FQuat::MakeFromRotationVector(OffsetVector.GetSafeNormal() * MaxAngleRad) * Target).GetNormalized();
				State.AngularVelocity *= 0.5f;
			}
		}

		Local.SetRotation(State.Current);
		const FTransform BoneNew = Local * ParentNew;
		NewComponentTransforms.Add(BoneIndex.GetInt(), BoneNew);
		OutBoneTransforms.Add(FBoneTransform(BoneIndex, BoneNew));
	}
	OutBoneTransforms.Sort(FCompareBoneTransformIndex());
}
