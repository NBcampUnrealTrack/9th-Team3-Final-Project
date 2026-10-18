#include "Animation/knightAnimNode_LegPlacement.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "TwoBoneIK.h"
#include "WaterBodyActor.h"

namespace knightLegPlacement
{
	static TAutoConsoleVariable<int32> CVarEnabled(TEXT("knight.Anim.LegPlacement"), 1,
		TEXT("1 = monster feet follow the ground (knight Leg Placement anim node), 0 = off for A/B comparison."));
	static TAutoConsoleVariable<int32> CVarDraw(TEXT("knight.Anim.DrawLegPlacement"), 0,
		TEXT("1 = draw the leg placement ground traces (green hit, red miss) and locked feet (yellow)."));
	static TAutoConsoleVariable<int32> CVarFootLock(TEXT("knight.Anim.FootLock"), 1,
		TEXT("1 = planted feet stay in place while the FootLock_<foot bone> curve is on, 0 = off for A/B comparison."));
}

FName FAnimNode_knightLegPlacement::MakeFootLockCurveName(FName FootBone)
{
	return FName(*(TEXT("FootLock_") + FootBone.ToString()));
}

void FAnimNode_knightLegPlacement::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	PelvisBone.Initialize(RequiredBones);
	const FReferenceSkeleton& RefSkeleton = RequiredBones.GetReferenceSkeleton();
	for (FknightLegPlacementLeg& Leg : Legs)
	{
		Leg.FootBone.Initialize(RequiredBones);
		Leg.bChainValid = false;
		Leg.LockCurve = MakeFootLockCurveName(Leg.FootBone.BoneName);
		Leg.bLocked = false;
		Leg.bWaitForLift = false;
		Leg.LockAlpha = 0.f;
		// 발 -> 무릎 -> 허벅지 순서로 부모를 따라 올라가 2본 체인을 만든다.
		const int32 Foot = RefSkeleton.FindBoneIndex(Leg.FootBone.BoneName);
		const int32 Joint = Foot != INDEX_NONE ? RefSkeleton.GetParentIndex(Foot) : INDEX_NONE;
		const int32 Root = Joint != INDEX_NONE ? RefSkeleton.GetParentIndex(Joint) : INDEX_NONE;
		if (Root == INDEX_NONE)
		{
			continue;
		}
		Leg.JointBone = FBoneReference(RefSkeleton.GetBoneName(Joint));
		Leg.RootBone = FBoneReference(RefSkeleton.GetBoneName(Root));
		Leg.JointBone.Initialize(RequiredBones);
		Leg.RootBone.Initialize(RequiredBones);
		Leg.bChainValid = Leg.FootBone.IsValidToEvaluate(RequiredBones) && Leg.JointBone.IsValidToEvaluate(RequiredBones)
			&& Leg.RootBone.IsValidToEvaluate(RequiredBones);
	}
}

bool FAnimNode_knightLegPlacement::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	if (knightLegPlacement::CVarEnabled.GetValueOnAnyThread() == 0 || !PelvisBone.IsValidToEvaluate(RequiredBones))
	{
		return false;
	}
	return Legs.ContainsByPredicate([](const FknightLegPlacementLeg& Leg) { return Leg.bChainValid; });
}

void FAnimNode_knightLegPlacement::PreUpdate(const UAnimInstance* InAnimInstance)
{
	const USkeletalMeshComponent* const Mesh = InAnimInstance ? InAnimInstance->GetSkelMeshComponent() : nullptr;
	UWorld* const World = Mesh ? Mesh->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	const float DeltaTime = World->GetDeltaSeconds();
	WorldToComponent = 1.f / FMath::Max(FMath::Abs(Mesh->GetComponentScale().Z), UE_KINDA_SMALL_NUMBER);

	// 떨어지는 중이거나 꺼져 있으면 높이 차를 0으로 되돌린다.
	const ACharacter* const Character = Cast<ACharacter>(Mesh->GetOwner());
	const UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const bool bActive = knightLegPlacement::CVarEnabled.GetValueOnGameThread() != 0 && (!Movement || Movement->IsMovingOnGround());
	const bool bDraw = knightLegPlacement::CVarDraw.GetValueOnGameThread() != 0;
	bFootLockActive = bActive && bEnableFootLock && knightLegPlacement::CVarFootLock.GetValueOnGameThread() != 0;

	// 메시 원점 = 캡슐 바닥 = 애니메이션이 가정한 평지 높이.
	const float BaseZ = Mesh->GetComponentLocation().Z;
	// 폰(플레이어)이나 자기 피직스 바디가 아니라 지형·정적 메시만 밟는다.
	const FCollisionObjectQueryParams ObjectParams(ECC_WorldStatic);
	const FCollisionQueryParams BaseQueryParams(SCENE_QUERY_STAT(knightLegPlacement), false, Mesh->GetOwner());

	for (FknightLegPlacementLeg& Leg : Legs)
	{
		float Target = 0.f;
		const int32 MeshFoot = Leg.bChainValid ? Mesh->GetBoneIndex(Leg.FootBone.BoneName) : INDEX_NONE;
		if (bActive && MeshFoot != INDEX_NONE)
		{
			// 이전 프레임의 발 위치. IK가 바꾸는 것은 주로 높이라 XY는 거의 그대로다.
			const FVector Foot = Mesh->GetBoneTransform(MeshFoot).GetLocation();
			const FVector Start(Foot.X, Foot.Y, BaseZ + MaxStepUp + TraceRadius);
			const FVector End(Foot.X, Foot.Y, BaseZ - MaxStepDown);
			FHitResult Hit;
			FCollisionQueryParams QueryParams(BaseQueryParams);
			bool bHit = false;
			// WaterBody collision is WorldStatic in this map, so the first sweep hits the lake surface.
			// Ignore only water actors and continue to the actual walkable ground below them.
			for (int32 Attempt = 0; Attempt < 16; ++Attempt)
			{
				bHit = World->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity, ObjectParams,
					FCollisionShape::MakeSphere(TraceRadius), QueryParams);
				if (!bHit || !Cast<AWaterBody>(Hit.GetActor()))
				{
					break;
				}
				if (Attempt == 15)
				{
					bHit = false;
					break;
				}
				QueryParams.AddIgnoredActor(Hit.GetActor());
			}
			if (bHit)
			{
				Target = FMath::Clamp(static_cast<float>(Hit.ImpactPoint.Z - BaseZ), -MaxStepDown, MaxStepUp);
			}
#if ENABLE_DRAW_DEBUG
			if (bDraw)
			{
				DrawDebugLine(World, Start, bHit ? Hit.ImpactPoint : End, bHit ? FColor::Green : FColor::Red, false, -1.f, 0, 3.f);
				if (bHit)
				{
					DrawDebugSphere(World, Hit.ImpactPoint, TraceRadius, 8, FColor::Green, false, -1.f, 0, 2.f);
				}
			}
#endif
		}
		Leg.Offset = FMath::FInterpTo(Leg.Offset, Target, DeltaTime, InterpSpeed);
	}
}

void FAnimNode_knightLegPlacement::UpdateInternal(const FAnimationUpdateContext& Context)
{
	FAnimNode_SkeletalControlBase::UpdateInternal(Context);
	FootLockDeltaTime = Context.GetDeltaTime();
	++UpdatesSinceEvaluate;
}

void FAnimNode_knightLegPlacement::UpdateFootLock(FknightLegPlacementLeg& Leg, float CurveValue, const FTransform& ComponentToWorld,
	const FVector& AnimFootCS, FVector& InOutEffectorCS, double& OutYawCorrection)
{
	const bool bPlanted = CurveValue > 0.5f;
	if (!bPlanted)
	{
		Leg.bWaitForLift = false;
	}
	const FVector AnimFootWorld = ComponentToWorld.TransformPosition(AnimFootCS);
	const double ComponentYaw = ComponentToWorld.Rotator().Yaw;

	if (Leg.bLocked)
	{
		if (!bPlanted)
		{
			Leg.bLocked = false;
		}
		else if (FVector::Dist2D(Leg.LockedLocation, AnimFootWorld) > MaxFootLockDistance)
		{
			// 다리가 닿지 않는 거리. 붙잡고 있으면 다리가 펴진 채 끌리니 풀고, 다음 디딤까지 기다린다.
			Leg.bLocked = false;
			Leg.bWaitForLift = true;
		}
	}
	else if (bPlanted && !Leg.bWaitForLift)
	{
		// 풀리던 중이면 지금 보이는 위치(고정과 애니메이션 사이)에서 다시 잡아야 발이 튀지 않는다.
		Leg.LockedLocation = FMath::Lerp(AnimFootWorld, Leg.LockedLocation, static_cast<double>(Leg.LockAlpha));
		Leg.LockedYaw = ComponentYaw + Leg.LockAlpha * FRotator::NormalizeAxis(Leg.LockedYaw - ComponentYaw);
		Leg.LockAlpha = 1.f;
		Leg.bLocked = true;
	}

	if (!Leg.bLocked)
	{
		Leg.LockAlpha = FootLockReleaseTime > 0.f ? FMath::Max(0.f, Leg.LockAlpha - FootLockDeltaTime / FootLockReleaseTime) : 0.f;
	}
	if (Leg.LockAlpha <= 0.f)
	{
		return;
	}

	// 높이는 지면 맞추기 몫이라 XY만 붙잡는다. 컴포넌트는 요만 도니 컴포넌트 XY 평면 = 월드 수평면이다.
	const FVector LockedCS = ComponentToWorld.InverseTransformPosition(FVector(Leg.LockedLocation.X, Leg.LockedLocation.Y, AnimFootWorld.Z));
	InOutEffectorCS.X = FMath::Lerp(InOutEffectorCS.X, LockedCS.X, static_cast<double>(Leg.LockAlpha));
	InOutEffectorCS.Y = FMath::Lerp(InOutEffectorCS.Y, LockedCS.Y, static_cast<double>(Leg.LockAlpha));
	// 몸이 돈 만큼 발을 반대로 돌려 월드 방향을 유지한다.
	OutYawCorrection = Leg.LockAlpha * FRotator::NormalizeAxis(Leg.LockedYaw - ComponentYaw);
}

void FAnimNode_knightLegPlacement::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();

	struct FLegPose
	{
		FCompactPoseBoneIndex Root;
		FCompactPoseBoneIndex Joint;
		FCompactPoseBoneIndex Foot;
		FTransform RootCS;
		FTransform JointCS;
		FTransform FootCS;
		/** 컴포넌트 공간 단위의 지면 높이 차. */
		double Offset;
		FknightLegPlacementLeg* Leg;
	};

	// LOD 등으로 평가를 건너뛰었으면 그동안 몸이 움직였으니 이전 고정 위치를 버린다.
	const bool bResetFootLock = UpdatesSinceEvaluate > 1;
	UpdatesSinceEvaluate = 0;

	TArray<FLegPose, TInlineAllocator<4>> Poses;
	for (FknightLegPlacementLeg& Leg : Legs)
	{
		if (bResetFootLock)
		{
			Leg.bLocked = false;
			Leg.bWaitForLift = false;
			Leg.LockAlpha = 0.f;
		}
		if (!Leg.bChainValid)
		{
			continue;
		}
		const FCompactPoseBoneIndex Root = Leg.RootBone.GetCompactPoseIndex(BoneContainer);
		const FCompactPoseBoneIndex Joint = Leg.JointBone.GetCompactPoseIndex(BoneContainer);
		const FCompactPoseBoneIndex Foot = Leg.FootBone.GetCompactPoseIndex(BoneContainer);
		Poses.Add({ Root, Joint, Foot, Output.Pose.GetComponentSpaceTransform(Root), Output.Pose.GetComponentSpaceTransform(Joint),
			Output.Pose.GetComponentSpaceTransform(Foot), Leg.Offset * WorldToComponent, &Leg });
	}
	if (Poses.IsEmpty())
	{
		return;
	}

	// 1) 골반: 발밑 지면 평면 z = a*x + b*y + c (x, y = 발 위치, z = 높이 차)에 맞춰 내리고 기울인다.
	const FCompactPoseBoneIndex PelvisIndex = PelvisBone.GetCompactPoseIndex(BoneContainer);
	const FTransform PelvisOld = Output.Pose.GetComponentSpaceTransform(PelvisIndex);
	const FVector PelvisLocation = PelvisOld.GetLocation();
	FTransform PelvisNew = PelvisOld;

	bool bPlaneFound = false;
	if (Poses.Num() >= 3)
	{
		// 최소제곱 정규방정식 3x3을 크라메르 공식으로 푼다. 발이 한 줄로 서 있으면 행렬식이 0에 가까워 건너뛴다.
		double Sxx = 0, Sxy = 0, Syy = 0, Sx = 0, Sy = 0, Sxz = 0, Syz = 0, Sz = 0;
		for (const FLegPose& Pose : Poses)
		{
			// 골반 기준 좌표로 풀어 수치 오차를 줄인다.
			const double X = Pose.FootCS.GetLocation().X - PelvisLocation.X;
			const double Y = Pose.FootCS.GetLocation().Y - PelvisLocation.Y;
			Sxx += X * X; Sxy += X * Y; Syy += Y * Y; Sx += X; Sy += Y;
			Sxz += X * Pose.Offset; Syz += Y * Pose.Offset; Sz += Pose.Offset;
		}
		const double N = Poses.Num();
		auto Det3 = [](double A, double B, double C, double D, double E, double F, double G, double H, double I)
		{
			return A * (E * I - F * H) - B * (D * I - F * G) + C * (D * H - E * G);
		};
		const double Det = Det3(Sxx, Sxy, Sx, Sxy, Syy, Sy, Sx, Sy, N);
		if (FMath::Abs(Det) > UE_KINDA_SMALL_NUMBER * FMath::Max(1.0, Sxx * Syy * N))
		{
			const double A = Det3(Sxz, Sxy, Sx, Syz, Syy, Sy, Sz, Sy, N) / Det;
			const double B = Det3(Sxx, Sxz, Sx, Sxy, Syz, Sy, Sx, Sz, N) / Det;
			const double C = Det3(Sxx, Sxy, Sxz, Sxy, Syy, Syz, Sx, Sy, Sz) / Det;
			bPlaneFound = true;

			// 골반 바로 아래의 평면 높이만큼 내린다(골반 기준 좌표라 c가 곧 그 높이).
			PelvisNew.AddToTranslation(FVector(0.0, 0.0, C));

			const FVector Normal = FVector(-A, -B, 1.0).GetSafeNormal();
			FQuat Tilt = FQuat::FindBetweenNormals(FVector::UpVector, Normal);
			FVector Axis;
			double Angle = 0.0;
			Tilt.ToAxisAndAngle(Axis, Angle);
			Angle = FMath::Min(Angle, static_cast<double>(FMath::DegreesToRadians(MaxBodyTilt))) * BodyTiltWeight;
			if (Angle > UE_KINDA_SMALL_NUMBER)
			{
				// 컴포넌트 공간에서 골반 위치를 축으로 돌린다.
				PelvisNew.SetRotation(FQuat(Axis, Angle) * PelvisOld.GetRotation());
			}
		}
	}
	if (!bPlaneFound)
	{
		// 2족이거나 발이 한 줄이면 가장 낮은 발만큼만 내린다. 높은 쪽 다리는 IK로 굽힌다.
		double Lowest = TNumericLimits<double>::Max();
		for (const FLegPose& Pose : Poses)
		{
			Lowest = FMath::Min(Lowest, Pose.Offset);
		}
		PelvisNew.AddToTranslation(FVector(0.0, 0.0, Lowest));
	}
	OutBoneTransforms.Add(FBoneTransform(PelvisIndex, PelvisNew));

	// 2) 다리: 골반이 움직인 만큼 체인을 옮긴 뒤, 발을 원래 높이 + 지면 높이 차로 IK.
	// 자식의 새 컴포넌트 공간 = 자식 * 골반^-1 * 새 골반 (다리 체인은 모두 골반의 자손이다).
	const FTransform PelvisDelta = PelvisOld.Inverse() * PelvisNew;
	const FTransform& ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();
#if ENABLE_ANIM_DRAW_DEBUG
	const bool bDraw = knightLegPlacement::CVarDraw.GetValueOnAnyThread() != 0;
#endif
	for (FLegPose& Pose : Poses)
	{
		const double DesiredFootZ = Pose.FootCS.GetLocation().Z + Pose.Offset;
		FTransform RootCS = Pose.RootCS * PelvisDelta;
		FTransform JointCS = Pose.JointCS * PelvisDelta;
		FTransform FootCS = Pose.FootCS * PelvisDelta;

		FVector Effector = FootCS.GetLocation();
		Effector.Z = DesiredFootZ;

		// 공중이거나 꺼져 있으면 커브를 0으로 봐서 고정을 부드럽게 푼다.
		const float LockCurveValue = bFootLockActive ? Output.Curve.Get(Pose.Leg->LockCurve) : 0.f;
		double YawCorrection = 0.0;
		UpdateFootLock(*Pose.Leg, LockCurveValue, ComponentToWorld, FootCS.GetLocation(), Effector, YawCorrection);
		if (YawCorrection != 0.0)
		{
			FootCS.SetRotation(FQuat(FVector::UpVector, FMath::DegreesToRadians(YawCorrection)) * FootCS.GetRotation());
		}
#if ENABLE_ANIM_DRAW_DEBUG
		if (bDraw && Pose.Leg->LockAlpha > 0.f)
		{
			Output.AnimInstanceProxy->AnimDrawDebugSphere(ComponentToWorld.TransformPosition(Effector), TraceRadius, 8,
				Pose.Leg->bLocked ? FColor::Yellow : FColor::Orange, false, -1.f, 2.f);
		}
#endif
		// 무릎이 원래 굽은 쪽을 유지하도록, 엉덩이-발 중점에서 무릎 방향으로 밀어 낸 점을 무릎 목표로 쓴다.
		const FVector Mid = (RootCS.GetLocation() + FootCS.GetLocation()) * 0.5;
		const FVector JointTarget = JointCS.GetLocation() + (JointCS.GetLocation() - Mid);
		AnimationCore::SolveTwoBoneIK(RootCS, JointCS, FootCS, JointTarget, Effector, false, 1.0, 1.0);

		OutBoneTransforms.Add(FBoneTransform(Pose.Root, RootCS));
		OutBoneTransforms.Add(FBoneTransform(Pose.Joint, JointCS));
		OutBoneTransforms.Add(FBoneTransform(Pose.Foot, FootCS));
	}
	OutBoneTransforms.Sort(FCompareBoneTransformIndex());
}
