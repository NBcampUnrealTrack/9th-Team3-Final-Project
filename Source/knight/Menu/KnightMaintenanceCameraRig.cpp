#include "Menu/KnightMaintenanceCameraRig.h"
#include "Camera/CameraActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "System/knightGameplayTags.h"

namespace
{
	/** 임계 감쇠 스프링(반감기). 오버슈트 없음, 목표가 바뀌어도 속도 유지. */
	void SpringTo(FVector& Value, FVector& Velocity, const FVector& Goal, float HalfLife, float DeltaSeconds)
	{
		if (HalfLife <= KINDA_SMALL_NUMBER)
		{
			Velocity = FVector::ZeroVector;
			Value = Goal;
			return;
		}
		const float Y = 2.f * UE_LN2 / HalfLife;
		const FVector J0 = Value - Goal;
		const FVector J1 = Velocity + J0 * Y;
		const float Decay = FMath::Exp(-Y * DeltaSeconds);
		Value = Goal + Decay * (J0 + J1 * DeltaSeconds);
		Velocity = Decay * (Velocity - J1 * Y * DeltaSeconds);
	}

	FKnightMaintenanceShot MakeShot(FGameplayTag Slot, FName Bone, float Distance, float HeightOffset, float Yaw, float Pitch)
	{
		FKnightMaintenanceShot Shot;
		Shot.Slot = Slot;
		Shot.Bone = Bone;
		Shot.Distance = Distance;
		Shot.HeightOffset = HeightOffset;
		Shot.Yaw = Yaw;
		Shot.Pitch = Pitch;
		return Shot;
	}
}

AKnightMaintenanceCameraRig::AKnightMaintenanceCameraRig()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	Shots = {
		MakeShot(knightGameplayTags::Equipment_Slot_Head, TEXT("head"), 380.f, 0.f, 0.f, 2.f),
		MakeShot(knightGameplayTags::Equipment_Slot_Torso, TEXT("spine_03"), 440.f, 0.f, 0.f, 0.f),
		MakeShot(knightGameplayTags::Equipment_Slot_Arms, TEXT("lowerarm_l"), 480.f, 0.f, -12.f, 4.f),
		MakeShot(knightGameplayTags::Equipment_Slot_Legs, TEXT("pelvis"), 680.f, -230.f, 0.f, 0.f),
		MakeShot(knightGameplayTags::Equipment_Slot_Weapon, TEXT("hand_r"), 440.f, 0.f, 12.f, 4.f) };
}

AKnightMaintenanceCameraRig::FView AKnightMaintenanceCameraRig::MakeView(const FVector& Location, const FRotator& Rotation, float FocusDistance)
{
	FView View;
	View.Location = Location;
	View.Focus = Location + Rotation.Vector() * FocusDistance;
	return View;
}

float AKnightMaintenanceCameraRig::FocusDistanceFrom(const FVector& Location) const
{
	// 바라보는 점을 메카 거리에 둬서 시점 전환 시 메카를 중심으로 돈다.
	return FocusActor ? FMath::Max(FVector::Dist2D(Location, FocusActor->GetActorLocation()), 100.f) : 500.f;
}

AKnightMaintenanceCameraRig::FView AKnightMaintenanceCameraRig::OverviewView() const
{
	return MakeView(MaintenanceWorldLocation, MaintenanceWorldRotation, FocusDistanceFrom(MaintenanceWorldLocation));
}

bool AKnightMaintenanceCameraRig::ShotView(FGameplayTag Slot, FView& OutView) const
{
	const USkeletalMeshComponent* Mesh = FocusActor ? FocusActor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	const FKnightMaintenanceShot* Shot = Shots.FindByPredicate([Slot](const FKnightMaintenanceShot& Candidate) { return Candidate.Slot == Slot; });
	if (!Mesh || !Shot || Mesh->GetBoneIndex(Shot->Bone) == INDEX_NONE) return false;
	const FVector Target = Mesh->GetBoneLocation(Shot->Bone) + FVector(0.f, 0.f, Shot->HeightOffset);
	const FVector Direction = FRotator(0.f, MaintenanceWorldRotation.Yaw + Shot->Yaw, 0.f).Vector();
	const float PitchRadians = FMath::DegreesToRadians(Shot->Pitch);
	OutView.Location = Target - Direction * Shot->Distance * FMath::Cos(PitchRadians) + FVector(0.f, 0.f, Shot->Distance * FMath::Sin(PitchRadians));
	OutView.Focus = Target;
	return true;
}

void AKnightMaintenanceCameraRig::SetMaintenanceView(bool bInOpen)
{
	if (!ControlledCamera || bInOpen == bOpen) return;
	if (bInOpen && !bHasOriginal)
	{
		const FVector Location = ControlledCamera->GetActorLocation();
		Original = MakeView(Location, ControlledCamera->GetActorRotation(), FocusDistanceFrom(Location));
		Smoothed = PathPoint = Original;
		PathVelocity = SmoothedVelocity = SmoothedFocusVelocity = FVector::ZeroVector;
		bHasOriginal = true;
	}
	if (!bHasOriginal) return;
	bOpen = bInOpen;
	bAtShot = false;
	StartMove(bOpen ? OverviewView() : Original, false);
}

void AKnightMaintenanceCameraRig::FocusSlot(FGameplayTag Slot)
{
	if (!bOpen) return;
	FView Target;
	if (Slot.IsValid() && ShotView(Slot, Target))
	{
		// 호 이동은 클로즈업끼리만.
		StartMove(Target, bAtShot);
		bAtShot = true;
		return;
	}
	if (bAtShot) StartMove(OverviewView(), false);
	bAtShot = false;
}

void AKnightMaintenanceCameraRig::StartMove(const FView& Target, bool bArc)
{
	PathStart = bMoving ? PathPoint : Smoothed;
	PathEnd = Target;
	const float Travel = FMath::Max(FVector::Dist(PathStart.Location, PathEnd.Location), 0.5f * FVector::Dist(PathStart.Focus, PathEnd.Focus));
	Duration = FMath::Lerp(MinTransitionSeconds, FMath::Max(MaxTransitionSeconds, MinTransitionSeconds),
		FMath::Clamp(Travel / FMath::Max(FullTransitionDistance, 1.f), 0.f, 1.f));
	// 이동 중이면 현재 속도로 시작.
	StartSlope = bMoving && Travel > 1.f ? FMath::Clamp(PathVelocity.Size() * Duration / Travel, 0.f, 2.f) : 0.f;
	ArcOffset = FVector::ZeroVector;
	if (bArc)
	{
		const FVector Forward = ((PathStart.Focus - PathStart.Location).GetSafeNormal() + (PathEnd.Focus - PathEnd.Location).GetSafeNormal()).GetSafeNormal();
		ArcOffset = -Forward * FMath::Min(Travel * ArcStrength, MaxArcPullBack);
	}
	Elapsed = 0.f;
	bMoving = true;
	SetActorTickEnabled(true);
}

AKnightMaintenanceCameraRig::FView AKnightMaintenanceCameraRig::EvaluatePath(float Alpha) const
{
	// 3차 이징: 시작 속도 StartSlope, 끝 속도 0.
	const float K = StartSlope;
	const float S = K * Alpha + (3.f - 2.f * K) * Alpha * Alpha + (K - 2.f) * Alpha * Alpha * Alpha;
	FView View;
	View.Location = FMath::Lerp(PathStart.Location, PathEnd.Location, S) + ArcOffset * FMath::Sin(PI * FMath::Clamp(S, 0.f, 1.f));
	View.Focus = FMath::Lerp(PathStart.Focus, PathEnd.Focus, S);
	return View;
}

void AKnightMaintenanceCameraRig::ApplyToCamera(const FView& View)
{
	ControlledCamera->SetActorLocationAndRotation(View.Location, (View.Focus - View.Location).Rotation());
}

void AKnightMaintenanceCameraRig::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bMoving || !ControlledCamera || DeltaSeconds <= 0.f)
	{
		if (!ControlledCamera) bMoving = false;
		if (!bMoving) SetActorTickEnabled(false);
		return;
	}
	Elapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(Elapsed / FMath::Max(Duration, 0.01f), 0.f, 1.f);
	const FVector PreviousLocation = PathPoint.Location;
	PathPoint = EvaluatePath(Alpha);
	PathVelocity = (PathPoint.Location - PreviousLocation) / DeltaSeconds;
	SpringTo(Smoothed.Location, SmoothedVelocity, PathPoint.Location, SmoothingHalfLife, DeltaSeconds);
	SpringTo(Smoothed.Focus, SmoothedFocusVelocity, PathPoint.Focus, SmoothingHalfLife, DeltaSeconds);
	const bool bSettled = Alpha >= 1.f && FVector::DistSquared(Smoothed.Location, PathEnd.Location) < 0.25f
		&& FVector::DistSquared(Smoothed.Focus, PathEnd.Focus) < 0.25f;
	if (bSettled)
	{
		Smoothed = PathPoint = PathEnd;
		PathVelocity = SmoothedVelocity = SmoothedFocusVelocity = FVector::ZeroVector;
		bMoving = false;
		SetActorTickEnabled(false);
		if (!bOpen) bHasOriginal = false;
	}
	ApplyToCamera(Smoothed);
}
