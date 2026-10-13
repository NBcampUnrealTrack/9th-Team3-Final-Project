#include "Combat/knightTargetLockComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GenericTeamAgentInterface.h"
#include "HAL/IConsoleManager.h"
#include "Combat/knightCombatLibrary.h"
#include "NativeGameplayTags.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

namespace
{
	// 락온 대상이 멀리 있음. 부스트 돌진 베기(GA_Knight_BoostLunge)의 발동 조건, 약공격 1타·달리기 공격의 차단 조건. (2026-10-12)
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_LockOn_Far, "State.LockOn.Far");
}

namespace knightTargetLockCVars
{
	static int32 DrawLockOn = 0;
	static FAutoConsoleVariableRef CVarDrawLockOn(
		TEXT("knight.Combat.DrawLockOn"), DrawLockOn,
		TEXT("Draw lock-on candidates and the locked point. 0: off, 1: on"), ECVF_Cheat);
}

UknightTargetLockComponent::UknightTargetLockComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// Rotate the controller after the pawn moved, before the camera updates.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	TargetObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	TargetObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));
}

bool UknightTargetLockComponent::ToggleLockOn()
{
	if (IsLockedOn())
	{
		ClearLockOn();
		return false;
	}
	return TryLockOn();
}

bool UknightTargetLockComponent::TryLockOn()
{
	FVector ViewLocation;
	FRotator ViewRotation;
	if (!GetViewPoint(ViewLocation, ViewRotation))
	{
		return false;
	}
	const FVector ViewDirection = ViewRotation.Vector();
	const float CosMaxAngle = FMath::Cos(FMath::DegreesToRadians(MaxLockAngle));

	TArray<FCandidate> Candidates;
	GatherCandidates(Candidates);

	const FCandidate* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (const FCandidate& Candidate : Candidates)
	{
		const FVector PointLocation = Candidate.Point.GetLocation();
		const FVector ToPoint = PointLocation - ViewLocation;
		const float Distance = ToPoint.Size();
		const float CosAngle = Distance > KINDA_SMALL_NUMBER ? FVector::DotProduct(ToPoint / Distance, ViewDirection) : 1.f;
		if (CosAngle < CosMaxAngle || !HasLineOfSight(ViewLocation, PointLocation, Candidate.Actor.Get()))
		{
			continue;
		}
		// Prefer what the player is looking at; distance breaks ties between similar angles.
		const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosAngle, -1.f, 1.f)));
		const float Score = AngleDegrees / MaxLockAngle + 0.5f * Distance / LockOnRange;
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = &Candidate;
		}
	}
	if (!Best)
	{
		return false;
	}
	SetLock(*Best);
	return true;
}

void UknightTargetLockComponent::ClearLockOn()
{
	if (!LockedActor.IsValid() && !LockedComponent.IsValid())
	{
		return;
	}
	LockedActor.Reset();
	LockedComponent.Reset();
	LockedSocket = NAME_None;
	LockedPartTag = FGameplayTag();
	TimeOutOfSight = 0.f;
	SetComponentTickEnabled(false);
	SetLockedOnTag(false);
	SetFarTag(false);
	OnLockOnTargetChanged.Broadcast(nullptr, FGameplayTag());
}

void UknightTargetLockComponent::SwitchTarget(bool bRight)
{
	FVector CurrentLocation;
	const APawn* const Pawn = Cast<APawn>(GetOwner());
	APlayerController* const PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || !GetLockedTargetLocation(CurrentLocation))
	{
		return;
	}
	FVector2D CurrentScreen;
	if (!PC->ProjectWorldLocationToScreen(CurrentLocation, CurrentScreen))
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	GetViewPoint(ViewLocation, ViewRotation);

	TArray<FCandidate> Candidates;
	GatherCandidates(Candidates);

	const FCandidate* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (const FCandidate& Candidate : Candidates)
	{
		if (Candidate.Point.Component == LockedComponent.Get() && Candidate.Point.SocketName == LockedSocket)
		{
			continue;
		}
		const FVector PointLocation = Candidate.Point.GetLocation();
		FVector2D Screen;
		if (!PC->ProjectWorldLocationToScreen(PointLocation, Screen) || !HasLineOfSight(ViewLocation, PointLocation, Candidate.Actor.Get()))
		{
			continue;
		}
		const float DeltaX = Screen.X - CurrentScreen.X;
		if ((bRight && DeltaX <= 0.f) || (!bRight && DeltaX >= 0.f))
		{
			continue;
		}
		// Horizontal distance dominates; vertical offset only separates stacked points.
		const float Score = FMath::Abs(DeltaX) + 0.5f * FMath::Abs(Screen.Y - CurrentScreen.Y);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = &Candidate;
		}
	}
	if (Best)
	{
		SetLock(*Best);
	}
}

bool UknightTargetLockComponent::GetLockedTargetLocation(FVector& OutLocation) const
{
	const USceneComponent* const Component = LockedComponent.Get();
	if (!LockedActor.IsValid() || !Component)
	{
		return false;
	}
	OutLocation = Component->GetSocketLocation(LockedSocket);
	return true;
}

float UknightTargetLockComponent::GetLockedTargetRadius() const
{
	AActor* const Target = LockedActor.Get();
	return Target && Target->GetClass()->ImplementsInterface(UTargetableInterface::StaticClass())
		? ITargetableInterface::Execute_QueryTargetRadius(Target) : 0.f;
}

bool UknightTargetLockComponent::FindSoftTarget(const FVector& Direction, float Range, float MaxAngle, FVector& OutLocation,
	AActor*& OutActor, FGameplayTag& OutPartTag) const
{
	const AActor* const Owner = GetOwner();
	const FVector Forward = FVector(Direction.X, Direction.Y, 0.f).GetSafeNormal();
	if (!Owner || Forward.IsNearlyZero())
	{
		return false;
	}
	const FVector Origin = Owner->GetActorLocation();
	const float CosMaxAngle = FMath::Cos(FMath::DegreesToRadians(MaxAngle));

	TArray<FCandidate> Candidates;
	GatherCandidates(Candidates);

	const FCandidate* Best = nullptr;
	FVector BestLocation = FVector::ZeroVector;
	float BestScore = TNumericLimits<float>::Max();
	for (const FCandidate& Candidate : Candidates)
	{
		AActor* const Actor = Candidate.Actor.Get();
		const FVector PointLocation = Candidate.Point.GetLocation();
		FVector ToPoint = PointLocation - Origin;
		ToPoint.Z = 0.f;
		const float CenterDistance = ToPoint.Size();
		const float Radius = Candidate.Point.PartTag.IsValid() || !Actor ? 0.f : ITargetableInterface::Execute_QueryTargetRadius(Actor);
		const float Distance = FMath::Max(0.f, CenterDistance - Radius);
		if (Distance > Range)
		{
			continue;
		}
		// Inside the target (or right at it) any direction counts.
		const float CosAngle = CenterDistance > KINDA_SMALL_NUMBER ? FVector::DotProduct(ToPoint / CenterDistance, Forward) : 1.f;
		if (CosAngle < CosMaxAngle && Distance > 50.f)
		{
			continue;
		}
		if (!HasLineOfSight(Origin, PointLocation, Actor))
		{
			continue;
		}
		const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosAngle, -1.f, 1.f)));
		const float Score = Distance * (1.f + AngleDegrees / FMath::Max(MaxAngle, 1.f));
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = &Candidate;
			BestLocation = PointLocation;
		}
	}
	if (!Best)
	{
		return false;
	}
	OutLocation = BestLocation;
	OutActor = Best->Actor.Get();
	OutPartTag = Best->Point.PartTag;
	return true;
}

void UknightTargetLockComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* const Target = LockedActor.Get();
	FVector TargetLocation;
	if (!Target || !Target->GetClass()->ImplementsInterface(UTargetableInterface::StaticClass())
		|| !ITargetableInterface::Execute_CanBeTargeted(Target) || !GetLockedTargetLocation(TargetLocation))
	{
		ClearLockOn();
		return;
	}
	if (FVector::DistSquared(GetOwner()->GetActorLocation(), TargetLocation) > FMath::Square(BreakRange))
	{
		ClearLockOn();
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	if (GetViewPoint(ViewLocation, ViewRotation) && !HasLineOfSight(ViewLocation, TargetLocation, LockedActor.Get()))
	{
		TimeOutOfSight += DeltaTime;
		if (TimeOutOfSight > LoseSightGraceTime)
		{
			ClearLockOn();
			return;
		}
	}
	else
	{
		TimeOutOfSight = 0.f;
	}

	if (bRotateControllerToTarget)
	{
		UpdateControlRotation(DeltaTime);
	}

	// 공격이 닿는 지점(대상 표면 앞 FarStopDistance)까지 남은 거리로 멀다/가깝다를 정한다. 공격 워프와 같은 계산. (2026-10-12)
	if (FarTagDistance > 0.f)
	{
		FVector WarpLocation;
		FRotator WarpRotation;
		float Travel = 0.f;
		const bool bHasWarp = UknightCombatLibrary::ComputeAttackWarp(GetOwner(), BreakRange, FarStopDistance, false, 0.f, 0.f,
			WarpLocation, WarpRotation, Travel);
		SetFarTag(bHasWarp && Travel > FarTagDistance);
	}

	if (knightTargetLockCVars::DrawLockOn != 0)
	{
		DrawDebugSphere(GetWorld(), TargetLocation, 40.f, 12, FColor::Red, false, 0.f);
	}
}

void UknightTargetLockComponent::GatherCandidates(TArray<FCandidate>& OutCandidates) const
{
	OutCandidates.Reset();
	const AActor* const Owner = GetOwner();
	UWorld* const World = GetWorld();
	if (!Owner || !World)
	{
		return;
	}

	FCollisionObjectQueryParams ObjectParams;
	for (const TEnumAsByte<EObjectTypeQuery>& ObjectType : TargetObjectTypes)
	{
		ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectType));
	}
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(knightLockOnGather), false, Owner);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Owner->GetActorLocation(), FQuat::Identity, ObjectParams,
		FCollisionShape::MakeSphere(LockOnRange), QueryParams);

	TSet<AActor*> SeenActors;
	TArray<FknightTargetPoint> Points;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* const Actor = Overlap.GetActor();
		if (!Actor || SeenActors.Contains(Actor))
		{
			continue;
		}
		SeenActors.Add(Actor);

		if (!Actor->GetClass()->ImplementsInterface(UTargetableInterface::StaticClass())
			|| !ITargetableInterface::Execute_CanBeTargeted(Actor)
			|| FGenericTeamId::GetAttitude(Owner, Actor) != ETeamAttitude::Hostile)
		{
			continue;
		}
		Points = ITargetableInterface::Execute_QueryTargetPoints(Actor);
		for (const FknightTargetPoint& Point : Points)
		{
			if (Point.Component)
			{
				OutCandidates.Add({ Actor, Point });
			}
		}
	}

	if (knightTargetLockCVars::DrawLockOn != 0)
	{
		for (const FCandidate& Candidate : OutCandidates)
		{
			DrawDebugSphere(World, Candidate.Point.GetLocation(), 25.f, 8, FColor::Yellow, false, 1.f);
		}
	}
}

bool UknightTargetLockComponent::GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	const APawn* const Pawn = Cast<APawn>(GetOwner());
	const AController* const Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return false;
	}
	Controller->GetPlayerViewPoint(OutLocation, OutRotation);
	return true;
}

bool UknightTargetLockComponent::HasLineOfSight(const FVector& ViewLocation, const FVector& PointLocation, const AActor* Target) const
{
	if (!bRequireLineOfSight)
	{
		return true;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(knightLockOnSight), false, GetOwner());
	Params.AddIgnoredActor(Target);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, PointLocation, ECC_Visibility, Params);
}

void UknightTargetLockComponent::SetLock(const FCandidate& Candidate)
{
	const bool bChanged = LockedActor.Get() != Candidate.Actor.Get() || LockedComponent.Get() != Candidate.Point.Component
		|| LockedSocket != Candidate.Point.SocketName;
	LockedActor = Candidate.Actor;
	LockedComponent = Candidate.Point.Component.Get();
	LockedSocket = Candidate.Point.SocketName;
	LockedPartTag = Candidate.Point.PartTag;
	TimeOutOfSight = 0.f;
	SetComponentTickEnabled(true);
	SetLockedOnTag(true);
	if (bChanged)
	{
		UE_LOG(LogknightCombat, Verbose, TEXT("Locked on %s (%s)."), *GetNameSafe(LockedActor.Get()), *LockedPartTag.ToString());
		OnLockOnTargetChanged.Broadcast(LockedActor.Get(), LockedPartTag);
	}
}

void UknightTargetLockComponent::UpdateControlRotation(float DeltaTime) const
{
	const APawn* const Pawn = Cast<APawn>(GetOwner());
	AController* const Controller = Pawn ? Pawn->GetController() : nullptr;
	FVector TargetLocation;
	FVector ViewLocation;
	FRotator ViewRotation;
	if (!Controller || !GetLockedTargetLocation(TargetLocation) || !GetViewPoint(ViewLocation, ViewRotation))
	{
		return;
	}
	FRotator Desired = (TargetLocation - ViewLocation).Rotation();
	Desired.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Desired.Pitch + PitchOffset), MinPitch, MaxPitch);
	Desired.Roll = 0.f;
	const FRotator Current = Controller->GetControlRotation();
	Controller->SetControlRotation(FMath::RInterpTo(Current, Desired, DeltaTime, ControlRotationInterpSpeed));
}

void UknightTargetLockComponent::SetFarTag(bool bFar)
{
	if (bFar == bFarTagged)
	{
		return;
	}
	if (UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		ASC->SetLooseGameplayTagCount(TAG_State_LockOn_Far, bFar ? 1 : 0);
		bFarTagged = bFar;
	}
}

void UknightTargetLockComponent::SetLockedOnTag(bool bLocked) const
{
	if (UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		ASC->SetLooseGameplayTagCount(knightGameplayTags::State_LockedOn, bLocked ? 1 : 0);
	}
}
