#include "Animation/knightAnimNode_LegPlacement.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Volume.h"
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

	/** 고정·걸음 상태를 지운다. 발은 다음 디딤부터 다시 잡힌다. */
	static void ResetFootLock(FknightLegPlacementLeg& Leg)
	{
		Leg.bLocked = false;
		Leg.bWaitForLift = false;
		Leg.bStepping = false;
		Leg.LockAlpha = 0.f;
		Leg.ReleaseLift = 0.0;
	}
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
		// 고정 상태는 여기서 지우지 않는다. 이 함수는 LOD가 바뀔 때마다 다시 불려서, 지우면 붙잡고 있던 발이 그 순간 애니메이션
		// 위치로 튄다. 체인을 못 만든 다리만 아래에서 지운다(평가를 건너뛴 동안의 초기화는 Evaluate가 따로 한다).
		// 발 -> 무릎 -> 허벅지 순서로 부모를 따라 올라가 2본 체인을 만든다.
		const int32 Foot = RefSkeleton.FindBoneIndex(Leg.FootBone.BoneName);
		const int32 Joint = Foot != INDEX_NONE ? RefSkeleton.GetParentIndex(Foot) : INDEX_NONE;
		const int32 Root = Joint != INDEX_NONE ? RefSkeleton.GetParentIndex(Joint) : INDEX_NONE;
		if (Root == INDEX_NONE)
		{
			knightLegPlacement::ResetFootLock(Leg);
			continue;
		}
		Leg.JointBone = FBoneReference(RefSkeleton.GetBoneName(Joint));
		Leg.RootBone = FBoneReference(RefSkeleton.GetBoneName(Root));
		Leg.JointBone.Initialize(RequiredBones);
		Leg.RootBone.Initialize(RequiredBones);
		Leg.bChainValid = Leg.FootBone.IsValidToEvaluate(RequiredBones) && Leg.JointBone.IsValidToEvaluate(RequiredBones)
			&& Leg.RootBone.IsValidToEvaluate(RequiredBones);
		if (!Leg.bChainValid)
		{
			// 이 LOD에서 다리 뼈가 빠졌다. 다시 들어올 때 예전 고정 위치를 쓰지 않게 지운다.
			knightLegPlacement::ResetFootLock(Leg);
		}

		// 기준 포즈에서 허벅지(Root)와 발목의 컴포넌트 공간 거리.
		const TArray<FTransform>& RefPose = RefSkeleton.GetRefBonePose();
		auto RefComponentLocation = [&RefSkeleton, &RefPose](int32 Bone)
		{
			FTransform Result = FTransform::Identity;
			for (; Bone != INDEX_NONE; Bone = RefSkeleton.GetParentIndex(Bone))
			{
				Result = Result * RefPose[Bone];
			}
			return Result.GetLocation();
		};
		Leg.LengthCS = (RefComponentLocation(Foot) - RefComponentLocation(Root)).Size();

		// 발 뼈의 자손(발가락·발톱). 기준 스켈레톤은 부모가 자식보다 앞에 있으니 뒤쪽만 훑는다.
		Leg.ContactBones.Reset();
		for (int32 Bone = Foot + 1; Bone < RefSkeleton.GetNum(); ++Bone)
		{
			if (RefSkeleton.BoneIsChildOf(Bone, Foot))
			{
				FBoneReference& Contact = Leg.ContactBones.Add_GetRef(FBoneReference(RefSkeleton.GetBoneName(Bone)));
				Contact.Initialize(RequiredBones);
			}
		}
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
	GroundSnapAlpha = FMath::FInterpTo(GroundSnapAlpha, bActive ? 1.f : 0.f, DeltaTime, InterpSpeed);

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
			// 밟는 땅이 아닌 WorldStatic은 건너뛰고 그 아래 땅을 찾는다.
			// - WaterBody: 이 맵에서 WorldStatic이라 첫 스윕이 호수 수면에 맞는다.
			// - 볼륨(경기장 경계 BlockingVolume 등): 몸이 캡슐보다 커서 경계 옆 경사에서는 발이 볼륨 안에 들어간다. 스윕이 볼륨 안에서
			//   시작하면 시작점(MaxStepUp 위)을 땅으로 읽어 발과 몸이 떠오른다.
			for (int32 Attempt = 0; Attempt < 16; ++Attempt)
			{
				bHit = World->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity, ObjectParams,
					FCollisionShape::MakeSphere(TraceRadius), QueryParams);
				if (!bHit || !(Cast<AWaterBody>(Hit.GetActor()) || Cast<AVolume>(Hit.GetActor())))
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

FVector FAnimNode_knightLegPlacement::PredictFootWorld(const FVector& AnimFootWorld, const FTransform& ComponentToWorld, float Time) const
{
	// 애니메이션 발은 몸에 붙어 있으니, 몸이 Time초 동안 돌고 움직인 만큼 같이 옮긴다.
	const FVector Pivot = ComponentToWorld.GetLocation();
	const FVector Rotated = FQuat(FVector::UpVector, FMath::DegreesToRadians(ComponentYawRate * Time)).RotateVector(AnimFootWorld - Pivot);
	return Pivot + Rotated + ComponentVelocity * Time;
}

void FAnimNode_knightLegPlacement::BeginFootRelease(FknightLegPlacementLeg& Leg, const FVector& AnimFootWorld, double Lift)
{
	Leg.bLocked = false;
	Leg.bStepping = false;
	Leg.ReleaseLift = Lift;
	const double Speed = FMath::Max(LegLengthWorld(Leg) * FootLockReleaseSpeed, 1.0);
	Leg.ReleaseTime = FMath::Max(FootLockReleaseTime, static_cast<float>(FVector::Dist2D(Leg.LockedLocation, AnimFootWorld) / Speed));
}

void FAnimNode_knightLegPlacement::UpdateFootLockState(FknightLegPlacementLeg& Leg, bool bPlanted, const FVector& AnimFootWorld,
	const FTransform& ComponentToWorld)
{
	if (!bPlanted)
	{
		Leg.bWaitForLift = false;
	}
	const double ComponentYaw = ComponentToWorld.Rotator().Yaw;

	if (Leg.bStepping)
	{
		Leg.StepElapsed += FootLockDeltaTime;
		const float Lead = StepLead * StepDuration;
		if (!bPlanted)
		{
			// 애니메이션이 발을 들거나 다른 동작으로 넘어가면, 지금 위치와 높이에서 걸음을 멈추고 애니메이션에 넘긴다.
			const float Progress = FMath::Clamp(Leg.StepElapsed / StepDuration, 0.f, 1.f);
			const float StepAlpha = FMath::SmoothStep(0.f, 1.f, Progress);
			Leg.LockedLocation = FMath::Lerp(Leg.StepFrom, PredictFootWorld(AnimFootWorld, ComponentToWorld, FMath::Max(0.f, StepDuration - Leg.StepElapsed) + Lead), static_cast<double>(StepAlpha));
			Leg.LockedYaw = Leg.StepFromYaw + StepAlpha * FRotator::NormalizeAxis(ComponentYaw + ComponentYawRate * Lead - Leg.StepFromYaw);
			BeginFootRelease(Leg, AnimFootWorld, FMath::Sin(Progress * UE_PI) * Leg.LengthCS * StepHeightRatio);
		}
		else if (Leg.StepElapsed >= StepDuration)
		{
			// 착지. 몸이 계속 돈다고 보고 조금 앞에 디뎠으니 다음 걸음까지 여유가 생긴다.
			Leg.LockedLocation = PredictFootWorld(AnimFootWorld, ComponentToWorld, Lead);
			Leg.LockedYaw = ComponentYaw + ComponentYawRate * Lead;
			Leg.bStepping = false;
		}
	}
	else if (Leg.bLocked)
	{
		if (!bPlanted)
		{
			BeginFootRelease(Leg, AnimFootWorld, 0.0);
		}
		else if (FVector::Dist2D(Leg.LockedLocation, AnimFootWorld) > LegLengthWorld(Leg) * MaxFootLockStretch)
		{
			// 다리가 닿지 않는 거리(걸음을 못 뗀 경우). 붙잡고 있으면 다리가 펴진 채 끌리니 풀고, 다음 디딤까지 기다린다.
			BeginFootRelease(Leg, AnimFootWorld, 0.0);
			Leg.bWaitForLift = true;
		}
	}
	else if (bPlanted && !Leg.bWaitForLift)
	{
		// 풀리던 중이면 지금 보이는 위치(고정과 애니메이션 사이)에서 다시 잡아야 발이 튀지 않는다.
		Leg.LockedLocation = FMath::Lerp(AnimFootWorld, Leg.LockedLocation, static_cast<double>(Leg.LockAlpha));
		Leg.LockedYaw = ComponentYaw + Leg.LockAlpha * FRotator::NormalizeAxis(Leg.LockedYaw - ComponentYaw);
		Leg.LockAlpha = 1.f;
		Leg.ReleaseLift = 0.0;
		Leg.bLocked = true;
	}

	if (!Leg.bLocked)
	{
		Leg.LockAlpha = Leg.ReleaseTime > 0.f ? FMath::Max(0.f, Leg.LockAlpha - FootLockDeltaTime / Leg.ReleaseTime) : 0.f;
	}
}

void FAnimNode_knightLegPlacement::ApplyFootLock(const FknightLegPlacementLeg& Leg, const FVector& AnimFootWorld, const FTransform& ComponentToWorld,
	FVector& InOutEffectorCS, double& OutYawCorrection) const
{
	if (Leg.LockAlpha <= 0.f)
	{
		return;
	}
	const double ComponentYaw = ComponentToWorld.Rotator().Yaw;
	FVector TargetWorld = Leg.LockedLocation;
	double TargetYaw = Leg.LockedYaw;
	// 걷던 중에 풀렸으면 든 높이도 풀기와 함께 내린다.
	double Lift = Leg.ReleaseLift;
	if (Leg.bStepping)
	{
		// 시작점에서 착지 예상점까지 부드럽게 옮기며 가운데에서 가장 높이 든다. 착지점은 몸을 따라 매 프레임 다시 잡는다.
		const float Progress = FMath::Clamp(Leg.StepElapsed / StepDuration, 0.f, 1.f);
		const float StepAlpha = FMath::SmoothStep(0.f, 1.f, Progress);
		const float Lead = StepLead * StepDuration;
		const FVector Landing = PredictFootWorld(AnimFootWorld, ComponentToWorld, StepDuration - Leg.StepElapsed + Lead);
		TargetWorld = FMath::Lerp(Leg.StepFrom, Landing, static_cast<double>(StepAlpha));
		TargetYaw = Leg.StepFromYaw + StepAlpha * FRotator::NormalizeAxis(ComponentYaw + ComponentYawRate * Lead - Leg.StepFromYaw);
		Lift = FMath::Sin(Progress * UE_PI) * Leg.LengthCS * StepHeightRatio;
	}

	// 높이는 지면 맞추기 몫이라 XY만 붙잡는다. 컴포넌트는 요만 도니 컴포넌트 XY 평면 = 월드 수평면이다.
	const FVector TargetCS = ComponentToWorld.InverseTransformPosition(FVector(TargetWorld.X, TargetWorld.Y, AnimFootWorld.Z));
	InOutEffectorCS.X = FMath::Lerp(InOutEffectorCS.X, TargetCS.X, static_cast<double>(Leg.LockAlpha));
	InOutEffectorCS.Y = FMath::Lerp(InOutEffectorCS.Y, TargetCS.Y, static_cast<double>(Leg.LockAlpha));
	InOutEffectorCS.Z += Leg.bStepping ? Lift : Lift * Leg.LockAlpha;
	// 몸이 돈 만큼 발을 반대로 돌려 월드 방향을 유지한다.
	OutYawCorrection = Leg.LockAlpha * FRotator::NormalizeAxis(TargetYaw - ComponentYaw);
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
		/** 골반을 옮기기 전 애니메이션의 발 높이와, 접지 붙이기로 내릴(음수면 올릴) 높이. */
		double AnimFootZ;
		double GroundSnap;
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
			knightLegPlacement::ResetFootLock(Leg);
		}
		if (!Leg.bChainValid)
		{
			continue;
		}
		const FCompactPoseBoneIndex Root = Leg.RootBone.GetCompactPoseIndex(BoneContainer);
		const FCompactPoseBoneIndex Joint = Leg.JointBone.GetCompactPoseIndex(BoneContainer);
		const FCompactPoseBoneIndex Foot = Leg.FootBone.GetCompactPoseIndex(BoneContainer);
		const FTransform FootCS = Output.Pose.GetComponentSpaceTransform(Foot);

		// 접지 붙이기: 애니메이션이 가정한 땅은 컴포넌트 Z = 0. 접지점이 그 근처면 땅에 붙이고, 높을수록 덜 붙여 든 발은 그대로 둔다.
		double GroundSnap = 0.0;
		const double SnapRange = Leg.LengthCS * GroundSnapRatio;
		if (SnapRange > UE_KINDA_SMALL_NUMBER && GroundSnapAlpha > 0.f)
		{
			double ContactZ = FootCS.GetLocation().Z;
			for (const FBoneReference& Contact : Leg.ContactBones)
			{
				if (Contact.IsValidToEvaluate(BoneContainer))
				{
					ContactZ = FMath::Min(ContactZ, Output.Pose.GetComponentSpaceTransform(Contact.GetCompactPoseIndex(BoneContainer)).GetLocation().Z);
				}
			}
			const double Clearance = ContactZ - Leg.SoleOffset;
			GroundSnap = Clearance * (1.0 - FMath::SmoothStep(SnapRange, SnapRange * 2.0, FMath::Abs(Clearance))) * GroundSnapAlpha;
		}
		Poses.Add({ Root, Joint, Foot, Output.Pose.GetComponentSpaceTransform(Root), Output.Pose.GetComponentSpaceTransform(Joint),
			FootCS, Leg.Offset * WorldToComponent, FootCS.GetLocation().Z, GroundSnap, &Leg });
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

	// 3) 발 고정. 걸음 착지점을 내다보려고 몸의 속도와 회전 속도를 잰다.
	const double ComponentYaw = ComponentToWorld.Rotator().Yaw;
	if (bHasPrevComponent && !bResetFootLock && FootLockDeltaTime > UE_KINDA_SMALL_NUMBER)
	{
		ComponentVelocity = (ComponentToWorld.GetLocation() - PrevComponentLocation) / FootLockDeltaTime;
		ComponentVelocity.Z = 0.0;
		ComponentYawRate = FRotator::NormalizeAxis(ComponentYaw - PrevComponentYaw) / FootLockDeltaTime;
	}
	else
	{
		ComponentVelocity = FVector::ZeroVector;
		ComponentYawRate = 0.0;
	}
	PrevComponentLocation = ComponentToWorld.GetLocation();
	PrevComponentYaw = ComponentYaw;
	bHasPrevComponent = true;

	TArray<FVector, TInlineAllocator<4>> AnimFeetWorld;
	for (FLegPose& Pose : Poses)
	{
		Pose.RootCS = Pose.RootCS * PelvisDelta;
		Pose.JointCS = Pose.JointCS * PelvisDelta;
		Pose.FootCS = Pose.FootCS * PelvisDelta;
		AnimFeetWorld.Add(ComponentToWorld.TransformPosition(Pose.FootCS.GetLocation()));
		// 섞인 모든 포즈가 디딘 상태(커브 ≈ 1)일 때만 고정한다. 몽타주 블렌드가 시작되면 바로 풀기 시작해서
		// 발 이동이 포즈 블렌드와 함께 일어나고, 블렌드 중간 자세에서 고정하지 않는다. 공중이거나 꺼져 있으면 푼다.
		const bool bPlanted = bFootLockActive && Output.Curve.Get(Pose.Leg->LockCurve) > 0.99f;
		UpdateFootLockState(*Pose.Leg, bPlanted, AnimFeetWorld.Last(), ComponentToWorld);
	}

	if (bEnableAutoStep && bFootLockActive && ComponentVelocity.SizeSquared2D() <= FMath::Square(MaxAutoStepSpeed))
	{
		// 다리 길이 대비 가장 많이 끌린 발부터 걸음을 뗀다. 같이 드는 발은 대각선이어야 몸이 버틴다.
		auto Drift = [&](int32 Index)
		{
			const FknightLegPlacementLeg& Leg = *Poses[Index].Leg;
			return FVector::Dist2D(Leg.LockedLocation, AnimFeetWorld[Index]) / FMath::Max(LegLengthWorld(Leg), 1.0);
		};
		auto IsDiagonal = [&](int32 A, int32 B)
		{
			const FVector PA = Poses[A].FootCS.GetLocation() - PelvisNew.GetLocation();
			const FVector PB = Poses[B].FootCS.GetLocation() - PelvisNew.GetLocation();
			return PA.X * PB.X < 0.0 && PA.Y * PB.Y < 0.0;
		};
		TArray<int32, TInlineAllocator<4>> Stepping;
		TArray<int32, TInlineAllocator<4>> Candidates;
		for (int32 Index = 0; Index < Poses.Num(); ++Index)
		{
			const FknightLegPlacementLeg& Leg = *Poses[Index].Leg;
			if (Leg.bStepping)
			{
				Stepping.Add(Index);
			}
			else if (Leg.bLocked && Drift(Index) > StepTriggerRatio)
			{
				Candidates.Add(Index);
			}
		}
		Candidates.Sort([&](int32 A, int32 B) { return Drift(A) > Drift(B); });
		for (const int32 Index : Candidates)
		{
			if (Stepping.Num() >= MaxSimultaneousSteps
				|| Stepping.ContainsByPredicate([&](int32 Other) { return !IsDiagonal(Index, Other); }))
			{
				continue;
			}
			FknightLegPlacementLeg& Leg = *Poses[Index].Leg;
			Leg.bStepping = true;
			Leg.StepElapsed = 0.f;
			Leg.StepFrom = Leg.LockedLocation;
			Leg.StepFromYaw = Leg.LockedYaw;
			Stepping.Add(Index);
		}
	}

	for (int32 PoseIndex = 0; PoseIndex < Poses.Num(); ++PoseIndex)
	{
		FLegPose& Pose = Poses[PoseIndex];
		// 목표 = 애니메이션 높이 + 지면 높이 차. 골반이 옮긴 발 높이에는 이미 지면 평면 몫이 들어 있어 기준으로 쓰면 두 번 더해진다.
		const double DesiredFootZ = Pose.AnimFootZ - Pose.GroundSnap + Pose.Offset;
		FTransform RootCS = Pose.RootCS;
		FTransform JointCS = Pose.JointCS;
		FTransform FootCS = Pose.FootCS;

		FVector Effector = FootCS.GetLocation();
		Effector.Z = DesiredFootZ;

		double YawCorrection = 0.0;
		ApplyFootLock(*Pose.Leg, AnimFeetWorld[PoseIndex], ComponentToWorld, Effector, YawCorrection);
		if (YawCorrection != 0.0)
		{
			FootCS.SetRotation(FQuat(FVector::UpVector, FMath::DegreesToRadians(YawCorrection)) * FootCS.GetRotation());
		}
#if ENABLE_ANIM_DRAW_DEBUG
		if (bDraw && Pose.Leg->LockAlpha > 0.f)
		{
			Output.AnimInstanceProxy->AnimDrawDebugSphere(ComponentToWorld.TransformPosition(Effector), TraceRadius, 8,
				Pose.Leg->bStepping ? FColor::Cyan : Pose.Leg->bLocked ? FColor::Yellow : FColor::Orange, false, -1.f, 2.f);
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
