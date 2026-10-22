#include "Animation/knightAnimNode_WingClearance.h"

#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "Monster/knightMonsterHoverComponent.h"
#include "Algo/MinElement.h"
#include "Algo/Reverse.h"

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
	TailRootBone.Initialize(RequiredBones);
	TailTipBone.Initialize(RequiredBones);
}

bool FAnimNode_knightWingClearance::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	if (knightWingClearance::CVarEnabled.GetValueOnAnyThread() == 0 || !bGroundValid)
	{
		return false;
	}
	return (TailRootBone.IsValidToEvaluate(RequiredBones) && TailTipBone.IsValidToEvaluate(RequiredBones))
		|| Wings.ContainsByPredicate([&RequiredBones](const FknightWingClearanceWing& Wing)
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
	// 호버는 이 포즈가 나온 뒤에 메시 높이를 바꾼다. 지난 틱만큼 또 바꾼다고 보고 바꾼 뒤의 메시 높이로 잰다.
	if (!Hover.IsValid())
	{
		Hover = Character->FindComponentByClass<UknightMonsterHoverComponent>();
	}
	const double HoverDelta = Hover.IsValid() ? Hover->GetLastHeightDelta() : 0.0;
	GroundCS = (GroundZ - Mesh->GetComponentLocation().Z - HoverDelta) * WorldToComponent;
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
	LiftTail(Output, OutBoneTransforms);
	OutBoneTransforms.Sort(FCompareBoneTransformIndex());
}

void FAnimNode_knightWingClearance::LiftTail(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) const
{
	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	if (!TailRootBone.IsValidToEvaluate(BoneContainer) || !TailTipBone.IsValidToEvaluate(BoneContainer))
	{
		return;
	}
	// 끝에서 부모를 따라 뿌리까지 올라가 사슬을 만든다. 뿌리에 닿지 않으면 쓰지 않는다.
	const FCompactPoseBoneIndex RootIndex = TailRootBone.GetCompactPoseIndex(BoneContainer);
	TArray<FCompactPoseBoneIndex, TInlineAllocator<16>> Chain;
	for (FCompactPoseBoneIndex Index = TailTipBone.GetCompactPoseIndex(BoneContainer); Index != INDEX_NONE; Index = BoneContainer.GetParentBoneIndex(Index))
	{
		Chain.Add(Index);
		if (Index == RootIndex)
		{
			break;
		}
	}
	if (Chain.Num() < 2 || Chain.Last() != RootIndex)
	{
		return;
	}
	Algo::Reverse(Chain);
	TArray<FTransform, TInlineAllocator<16>> Transforms;
	for (const FCompactPoseBoneIndex Index : Chain)
	{
		Transforms.Add(Output.Pose.GetComponentSpaceTransform(Index));
	}

	const double MinZ = GroundCS + TailClearance * WorldToComponent;
	int32 FirstLifted = INDEX_NONE;
	for (int32 i = 0; i + 1 < Chain.Num(); ++i)
	{
		const FVector Pivot = Transforms[i].GetLocation();
		const FVector Offset = Transforms[i + 1].GetLocation() - Pivot;
		const double Target = MinZ - Pivot.Z;
		if (Offset.Z >= Target)
		{
			continue;
		}
		// 다음 뼈가 뻗은 수평 방향으로 세운다. 바로 아래로 늘어졌으면 앞 뼈에서 이 뼈로 오는 방향(꼬리 뒤쪽)으로 세운다.
		FVector Horizontal(Offset.X, Offset.Y, 0.0);
		if (Horizontal.IsNearlyZero() && i > 0)
		{
			const FVector Back = Pivot - Transforms[i - 1].GetLocation();
			Horizontal = FVector(Back.X, Back.Y, 0.0);
		}
		const FVector Axis = FVector::CrossProduct(Horizontal, FVector::UpVector).GetSafeNormal();
		if (Axis.IsNearlyZero())
		{
			continue;
		}
		// 축에 수직인 성분 U만 돈다: Z(a) = U.Z cos a + W.Z sin a = R cos(a - Phi). 다음 뼈가 Target에 닿는 가장 작은 각도를 쓴다.
		// 위 축이면 W.Z >= 0이라 Phi는 0~180도이고, 닿을 수 없으면 Phi(가장 높이 든 각도)가 된다.
		const FVector U = Offset - Axis * FVector::DotProduct(Offset, Axis);
		const FVector W = FVector::CrossProduct(Axis, U);
		const double R = FMath::Sqrt(FMath::Square(U.Z) + FMath::Square(W.Z));
		if (R <= UE_KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const double Angle = FMath::Atan2(W.Z, U.Z) - FMath::Acos(FMath::Clamp(Target / R, -1.0, 1.0));
		if (Angle <= 0.0)
		{
			continue;
		}
		// 이 뼈를 돌리면 아래 뼈들도 함께 돈다.
		const FQuat Lift(Axis, Angle);
		Transforms[i].SetRotation(Lift * Transforms[i].GetRotation());
		for (int32 j = i + 1; j < Chain.Num(); ++j)
		{
			Transforms[j].SetLocation(Pivot + Lift.RotateVector(Transforms[j].GetLocation() - Pivot));
			Transforms[j].SetRotation(Lift * Transforms[j].GetRotation());
		}
		if (FirstLifted == INDEX_NONE)
		{
			FirstLifted = i;
		}
	}
	for (int32 i = FirstLifted; FirstLifted != INDEX_NONE && i < Chain.Num(); ++i)
	{
		OutBoneTransforms.Add(FBoneTransform(Chain[i], Transforms[i]));
	}
}
