#include "Animation/knightAnimNode_WingClearance.h"

#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "Algo/MinElement.h"

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
		Wing.LiftRotation = FQuat::Identity;
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

		TArray<FVector, TInlineAllocator<16>> TipOffsets;
		for (const FBoneReference& Tip : Wing.TipBones)
		{
			if (Tip.IsValidToEvaluate(BoneContainer))
			{
				TipOffsets.Add(Output.Pose.GetComponentSpaceTransform(Tip.GetCompactPoseIndex(BoneContainer)).GetLocation() - Pivot);
			}
		}
		if (TipOffsets.IsEmpty())
		{
			continue;
		}
		auto LowestTipZ = [&](const FQuat& Rotation)
		{
			double Lowest = TNumericLimits<double>::Max();
			for (const FVector& Offset : TipOffsets)
			{
				Lowest = FMath::Min(Lowest, Pivot.Z + Rotation.RotateVector(Offset).Z);
			}
			return Lowest;
		};
		// 이 축으로 모든 끝이 바닥 위에 오는 가장 작은 각도(1도 단위). 못 찾으면 최대 각도.
		auto FindLift = [&](const FVector& LiftAxis, bool& bOutClear)
		{
			for (float Angle = 0.f; ; Angle = FMath::Min(Angle + 1.f, MaxLiftAngle))
			{
				const FQuat Rotation(LiftAxis, FMath::DegreesToRadians(Angle));
				bOutClear = LowestTipZ(Rotation) >= MinTipZ;
				if (bOutClear || Angle >= MaxLiftAngle)
				{
					return Rotation;
				}
			}
		};
		bool bClear = true;
		FQuat Needed = FindLift(Axis, bClear);
		if (!bClear)
		{
			// 앞뒤 축으로는 안 올라간다. 가장 낮은 끝을 바로 위로 드는 축(끝의 수평 방향 x 수직축)으로 돌린다.
			const FVector* const LowestOffset = Algo::MinElementBy(TipOffsets, [](const FVector& Offset) { return Offset.Z; });
			const FVector TipAxis = FVector::CrossProduct(FVector(LowestOffset->X, LowestOffset->Y, 0.0), FVector::UpVector).GetSafeNormal();
			bool bTipClear = false;
			const FQuat TipNeeded = TipAxis.IsNearlyZero() ? Needed : FindLift(TipAxis, bTipClear);
			if (bTipClear || LowestTipZ(TipNeeded) > LowestTipZ(Needed))
			{
				Needed = TipNeeded;
			}
		}

		// 바로 올리고 천천히 내린다. 다음 날갯짓에서도 든 채로 시작해 날갯짓이 작아진다.
		// 지금 든 회전에서 필요한 회전 쪽으로 ReleaseSpeed만큼 옮겨 보고, 그래도 끝이 바닥 위면 그만큼만 내린다.
		const float Gap = FMath::RadiansToDegrees(static_cast<float>(Wing.LiftRotation.AngularDistance(Needed)));
		const FQuat Released = Gap > KINDA_SMALL_NUMBER
			? FQuat::Slerp(Wing.LiftRotation, Needed, FMath::Min(1.f, ReleaseSpeed * DeltaTime / Gap)) : Needed;
		Wing.LiftRotation = LowestTipZ(Released) >= FMath::Min(MinTipZ, LowestTipZ(Needed)) ? Released : Needed;
		Wing.LiftRotation.Normalize();
		if (Wing.LiftRotation.GetAngle() <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		FTransform Lifted = RootCS;
		Lifted.SetRotation(Wing.LiftRotation * RootCS.GetRotation());
		OutBoneTransforms.Add(FBoneTransform(RootIndex, Lifted));
	}
	OutBoneTransforms.Sort(FCompareBoneTransformIndex());
}
