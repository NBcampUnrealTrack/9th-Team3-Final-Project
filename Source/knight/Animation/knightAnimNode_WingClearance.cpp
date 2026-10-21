#include "Animation/knightAnimNode_WingClearance.h"

#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"

namespace knightWingClearance
{
	static TAutoConsoleVariable<int32> CVarEnabled(TEXT("knight.Anim.WingClearance"), 1,
		TEXT("1 = flying monsters lift their wings to keep the tips above the ground (knight Wing Clearance anim node), 0 = off."));
}

void FAnimNode_knightWingClearance::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	for (FknightWingClearanceWing& Wing : Wings)
	{
		Wing.RootBone.Initialize(RequiredBones);
		for (FBoneReference& Tip : Wing.TipBones)
		{
			Tip.Initialize(RequiredBones);
		}
		Wing.LiftAngle = 0.f;
	}
}

bool FAnimNode_knightWingClearance::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	if (knightWingClearance::CVarEnabled.GetValueOnAnyThread() == 0 || !bGroundValid)
	{
		return false;
	}
	return Wings.ContainsByPredicate([&RequiredBones](const FknightWingClearanceWing& Wing)
	{
		return Wing.RootBone.IsValidToEvaluate(RequiredBones) && !Wing.TipBones.IsEmpty();
	});
}

void FAnimNode_knightWingClearance::PreUpdate(const UAnimInstance* InAnimInstance)
{
	const USkeletalMeshComponent* const Mesh = InAnimInstance ? InAnimInstance->GetSkelMeshComponent() : nullptr;
	const ACharacter* const Character = Mesh ? Cast<ACharacter>(Mesh->GetOwner()) : nullptr;
	const UCapsuleComponent* const Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	bGroundValid = Capsule != nullptr;
	if (!bGroundValid)
	{
		return;
	}
	// 바닥 = 캡슐 바닥. 메시는 호버로 바닥 아래까지 내려가 있을 수 있으니 메시 원점이 아니라 캡슐로 잰다.
	WorldToComponent = 1.0 / FMath::Max(FMath::Abs(Mesh->GetComponentScale().Z), UE_KINDA_SMALL_NUMBER);
	const double GroundZ = Capsule->GetComponentLocation().Z - Capsule->GetScaledCapsuleHalfHeight();
	GroundCS = (GroundZ - Mesh->GetComponentLocation().Z) * WorldToComponent;
}

void FAnimNode_knightWingClearance::UpdateInternal(const FAnimationUpdateContext& Context)
{
	FAnimNode_SkeletalControlBase::UpdateInternal(Context);
	DeltaTime = Context.GetDeltaTime();
}

void FAnimNode_knightWingClearance::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	const double MinTipZ = GroundCS + Clearance * WorldToComponent;

	for (FknightWingClearanceWing& Wing : Wings)
	{
		if (!Wing.RootBone.IsValidToEvaluate(BoneContainer))
		{
			continue;
		}
		const FCompactPoseBoneIndex RootIndex = Wing.RootBone.GetCompactPoseIndex(BoneContainer);
		const FCompactPoseBoneIndex ParentIndex = BoneContainer.GetParentBoneIndex(RootIndex);
		if (ParentIndex == INDEX_NONE)
		{
			continue;
		}
		const FTransform RootCS = Output.Pose.GetComponentSpaceTransform(RootIndex);
		const FVector Pivot = RootCS.GetLocation();

		// 날갯짓 축 = 몸의 앞뒤 축. 몸통(어깨의 부모)에서 어깨로 가는 수평 방향과 수직축에 모두 수직이다.
		// 날개 끝들은 부채꼴로 퍼져 있어서, 가장 낮은 끝 방향으로 축을 잡으면 다른 끝이 덜 올라간다.
		FVector Lateral = Pivot - Output.Pose.GetComponentSpaceTransform(ParentIndex).GetLocation();
		Lateral.Z = 0.0;
		const FVector Axis = FVector::CrossProduct(Lateral, FVector::UpVector).GetSafeNormal();
		if (Axis.IsNearlyZero())
		{
			continue;
		}

		TArray<FVector, TInlineAllocator<8>> TipOffsets;
		for (const FBoneReference& Tip : Wing.TipBones)
		{
			if (Tip.IsValidToEvaluate(BoneContainer))
			{
				TipOffsets.Add(Output.Pose.GetComponentSpaceTransform(Tip.GetCompactPoseIndex(BoneContainer)).GetLocation() - Pivot);
			}
		}
		// 모든 끝이 바닥 위에 오는 가장 작은 각도(1도 단위).
		auto AllClear = [&](float Angle)
		{
			const FQuat Rotation(Axis, FMath::DegreesToRadians(Angle));
			return !TipOffsets.ContainsByPredicate([&](const FVector& Offset) { return Pivot.Z + Rotation.RotateVector(Offset).Z < MinTipZ; });
		};
		float NeededAngle = 0.f;
		while (NeededAngle < MaxLiftAngle && !AllClear(NeededAngle))
		{
			NeededAngle = FMath::Min(NeededAngle + 1.f, MaxLiftAngle);
		}

		// 바로 올리고 천천히 내린다. 다음 날갯짓에서도 든 채로 시작해 날갯짓이 작아진다.
		Wing.LiftAngle = NeededAngle >= Wing.LiftAngle ? NeededAngle : FMath::Max(NeededAngle, Wing.LiftAngle - ReleaseSpeed * DeltaTime);
		if (Wing.LiftAngle <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		FTransform Lifted = RootCS;
		Lifted.SetRotation(FQuat(Axis, FMath::DegreesToRadians(Wing.LiftAngle)) * RootCS.GetRotation());
		OutBoneTransforms.Add(FBoneTransform(RootIndex, Lifted));
	}
	OutBoneTransforms.Sort(FCompareBoneTransformIndex());
}
