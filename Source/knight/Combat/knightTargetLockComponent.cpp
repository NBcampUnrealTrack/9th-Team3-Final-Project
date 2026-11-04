#include "Combat/knightTargetLockComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GenericTeamAgentInterface.h"
#include "HAL/IConsoleManager.h"
#include "Combat/knightCombatLibrary.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterDefinition.h"
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

bool UknightTargetLockComponent::PressLockOn()
{
	// 기본은 켜기/끄기. 가장 가까운 부위로 다시 잡기는 bReLockNearestOnPress일 때만.
	if (!bReLockNearestOnPress)
	{
		return ToggleLockOn();
	}
	const UWorld* const World = GetWorld();
	const double Now = World ? World->GetRealTimeSeconds() : 0.0;
	const bool bDoublePress = LastLockOnPressTime >= 0.0 && Now - LastLockOnPressTime <= LockOnDoublePressTime;
	LastLockOnPressTime = Now;

	if (!IsLockedOn())
	{
		// 같은 부위를 눌러 풀자마자 한 번 더 누른 것은 두 번 누르기(해제)의 두 번째라 다시 잡지 않는다.
		if (bDoublePress && bLastLockOnPressReleased)
		{
			bLastLockOnPressReleased = false;
			return false;
		}
		bLastLockOnPressReleased = false;
		const bool bLocked = TryLockOn();
		UE_LOG(LogknightCombat, Log, TEXT("%s: lock-on press -> %s %s."), *GetNameSafe(GetOwner()),
			bLocked ? *GetNameSafe(LockedActor.Get()) : TEXT("no target in view"), bLocked ? *LockedPartTag.ToString() : TEXT(""));
		return bLocked;
	}

	// 락온 중: 두 번 누르기면 해제, 아니면 플레이어와 가장 가까운 부위로. 그게 지금 부위면(또는 앞쪽에 아무것도 없으면) 해제.
	FCandidate Nearest;
	const bool bFoundNearest = !bDoublePress && FindNearestPointInView(Nearest);
	const bool bSamePoint = bFoundNearest && Nearest.Point.Component == LockedComponent.Get() && Nearest.Point.SocketName == LockedSocket;
	if (!bFoundNearest || bSamePoint)
	{
		UE_LOG(LogknightCombat, Log, TEXT("%s: lock-on press -> released (%s)."), *GetNameSafe(GetOwner()),
			bDoublePress ? TEXT("double press") : (bSamePoint ? TEXT("same part is nearest") : TEXT("nothing in view")));
		ClearLockOn();
		bLastLockOnPressReleased = true;
		return false;
	}
	SetLock(Nearest);
	bLastLockOnPressReleased = false;
	UE_LOG(LogknightCombat, Log, TEXT("%s: lock-on press -> nearest %s %s."), *GetNameSafe(GetOwner()), *GetNameSafe(LockedActor.Get()), *LockedPartTag.ToString());
	return true;
}

bool UknightTargetLockComponent::TryLockOn()
{
	if (IsLockOnBlocked())
	{
		return false;
	}
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
	ManualPartPinEndTime = -1.0;
	FCandidate InitialLock = *Best;
	if (bAutoSelectNearestPart && Best->Actor.IsValid())
	{
		SelectBestPartOnActor(Best->Actor.Get(), false, InitialLock);
	}
	SetLock(InitialLock);
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
	bHasSmoothedCameraFocus = false;
	LastAutoPartSwitchTime = -1.0;
	ManualPartPinEndTime = -1.0;
	SetComponentTickEnabled(false);
	SetLockedOnTag(false);
	SetFarTag(false);
	OnLockOnTargetChanged.Broadcast(nullptr, FGameplayTag());
}

void UknightTargetLockComponent::SwitchTarget(bool bRight)
{
	AActor* const Target = LockedActor.Get();
	if (bCyclePartsInOrder && Target && Target->GetClass()->ImplementsInterface(UTargetableInterface::StaticClass()))
	{
		const TArray<FknightTargetPoint> Points = ITargetableInterface::Execute_QueryTargetPoints(Target);
		TArray<FknightTargetPoint> ValidPoints;
		ValidPoints.Reserve(Points.Num());
		int32 CurrentIndex = INDEX_NONE;
		for (const FknightTargetPoint& Point : Points)
		{
			if (!Point.Component)
			{
				continue;
			}
			if (Point.Component == LockedComponent.Get() && Point.SocketName == LockedSocket)
			{
				CurrentIndex = ValidPoints.Num();
			}
			ValidPoints.Add(Point);
		}
		if (ValidPoints.Num() > 1)
		{
			if (CurrentIndex == INDEX_NONE)
			{
				CurrentIndex = 0;
			}
			const int32 Step = bRight ? 1 : -1;
			const int32 NextIndex = (CurrentIndex + Step + ValidPoints.Num()) % ValidPoints.Num();
			if (const UWorld* const World = GetWorld(); World && ManualPartPinDuration > 0.f)
			{
				ManualPartPinEndTime = World->GetRealTimeSeconds() + ManualPartPinDuration;
			}
			SetLock({ Target, ValidPoints[NextIndex] });
			UE_LOG(LogknightCombat, Log, TEXT("%s: lock-on wheel cycle (%s) -> %s %s (%d/%d)."),
				*GetNameSafe(GetOwner()), bRight ? TEXT("next") : TEXT("prev"),
				*GetNameSafe(Target), *ValidPoints[NextIndex].PartTag.ToString(), NextIndex + 1, ValidPoints.Num());
			return;
		}
	}

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
	const FCandidate* WrapBest = nullptr;
	float WrapExtremeX = bRight ? TNumericLimits<float>::Max() : TNumericLimits<float>::Lowest();
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
		if ((bRight && DeltaX > 0.f) || (!bRight && DeltaX < 0.f))
		{
			// Horizontal distance dominates; vertical offset only separates stacked points.
			const float Score = FMath::Abs(DeltaX) + 0.5f * FMath::Abs(Screen.Y - CurrentScreen.Y);
			if (Score < BestScore)
			{
				BestScore = Score;
				Best = &Candidate;
			}
		}
		else if ((bRight && Screen.X < WrapExtremeX) || (!bRight && Screen.X > WrapExtremeX))
		{
			WrapExtremeX = Screen.X;
			WrapBest = &Candidate;
		}
	}
	const FCandidate* const Chosen = Best ? Best : WrapBest;
	if (Chosen)
	{
		if (const UWorld* const World = GetWorld(); World && ManualPartPinDuration > 0.f)
		{
			ManualPartPinEndTime = World->GetRealTimeSeconds() + ManualPartPinDuration;
		}
		SetLock(*Chosen);
	}
}

bool UknightTargetLockComponent::SwitchTargetInDirection(FVector2D ScreenDirection)
{
	ScreenDirection = ScreenDirection.GetSafeNormal();
	FVector CurrentLocation;
	const APawn* const Pawn = Cast<APawn>(GetOwner());
	APlayerController* const PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (ScreenDirection.IsNearlyZero() || !PC || !GetLockedTargetLocation(CurrentLocation))
	{
		return false;
	}
	FVector2D CurrentScreen;
	if (!PC->ProjectWorldLocationToScreen(CurrentLocation, CurrentScreen))
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	GetViewPoint(ViewLocation, ViewRotation);

	TArray<FCandidate> Candidates;
	GatherCandidates(Candidates);

	const float CosMaxAngle = FMath::Cos(FMath::DegreesToRadians(SwitchMaxAngle));
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
		const FVector2D Delta = Screen - CurrentScreen;
		const float Distance = Delta.Size();
		// 화면에서 거의 겹친 지점은 방향을 정할 수 없어 뺀다.
		if (Distance < 1.f)
		{
			continue;
		}
		const float CosAngle = FVector2D::DotProduct(Delta / Distance, ScreenDirection);
		if (CosAngle < CosMaxAngle)
		{
			continue;
		}
		// 입력 방향에 가까울수록, 화면에서 가까울수록 먼저 고른다.
		const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosAngle, -1.f, 1.f)));
		const float Score = Distance * (1.f + AngleDegrees / SwitchMaxAngle);
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
	if (const UWorld* const World = GetWorld(); World && ManualPartPinDuration > 0.f)
	{
		ManualPartPinEndTime = World->GetRealTimeSeconds() + ManualPartPinDuration;
	}
	SetLock(*Best);
	return true;
}

void UknightTargetLockComponent::AddSwitchLookInput(const FVector2D& LookInput, bool bFromStick)
{
	const UWorld* const World = GetWorld();
	if (!bSwitchByLookInput || !World)
	{
		return;
	}

	if (bFromStick)
	{
		// 스틱 입력은 데드존 안에서는 오지 않는다. 한 프레임이라도 끊겼거나 충분히 돌아왔으면 가운데로 돌아온 것으로 본다.
		const float Tilt = LookInput.Size();
		if (GFrameCounter > LastSwitchStickFrame + 1 || Tilt < SwitchStickRearmThreshold)
		{
			bSwitchStickArmed = true;
		}
		LastSwitchStickFrame = GFrameCounter;
		if (!IsLockedOn())
		{
			// 락온 전부터 기울이던 스틱은 한 번 되돌리기 전까지 전환으로 치지 않는다.
			bSwitchStickArmed = false;
			return;
		}
		if (bSwitchStickArmed && Tilt >= SwitchStickThreshold)
		{
			bSwitchStickArmed = false;
			const bool bSwitched = SwitchTargetInDirection(LookInput);
			UE_LOG(LogknightCombat, Log, TEXT("%s: lock-on switch by stick (%.2f, %.2f) -> %s %s."), *GetNameSafe(GetOwner()), LookInput.X, LookInput.Y,
				bSwitched ? *GetNameSafe(LockedActor.Get()) : TEXT("no target in that direction"), bSwitched ? *LockedPartTag.ToString() : TEXT(""));
		}
		return;
	}

	// 마우스 이동 전환은 기본 끔(bSwitchByMouseMove).
	if (!bSwitchByMouseMove)
	{
		return;
	}

	// 마우스: 실제 시간 기준(히트스탑·슬로모션과 상관없이 손 움직임 그대로).
	const double Now = World->GetRealTimeSeconds();
	if (!IsLockedOn())
	{
		// 락온 전부터 움직이던 마우스는 락온 직후 SwitchMouseCooldown 동안 전환으로 치지 않는다.
		SwitchMouseCooldownEndTime = Now + SwitchMouseCooldown;
		SwitchMouseStartTime = -1.0;
		return;
	}
	if (Now < SwitchMouseCooldownEndTime)
	{
		SwitchMouseStartTime = -1.0;
		return;
	}
	// SwitchMouseWindow가 지나면 새로 모은다 → 천천히 미는 움직임은 문턱에 못 미친다.
	if (SwitchMouseStartTime < 0.0 || Now - SwitchMouseStartTime > SwitchMouseWindow)
	{
		SwitchMouseSum = FVector2D::ZeroVector;
		SwitchMouseStartTime = Now;
	}
	SwitchMouseSum += LookInput;
	if (SwitchMouseSum.SizeSquared() < FMath::Square(SwitchMouseThreshold))
	{
		return;
	}
	const bool bSwitched = SwitchTargetInDirection(SwitchMouseSum);
	UE_LOG(LogknightCombat, Log, TEXT("%s: lock-on switch by mouse (%.1f, %.1f) -> %s %s."), *GetNameSafe(GetOwner()), SwitchMouseSum.X, SwitchMouseSum.Y,
		bSwitched ? *GetNameSafe(LockedActor.Get()) : TEXT("no target in that direction"), bSwitched ? *LockedPartTag.ToString() : TEXT(""));
	// 대상이 없었어도 쉬어야 매 프레임 다시 찾지 않는다.
	SwitchMouseStartTime = -1.0;
	SwitchMouseCooldownEndTime = Now + SwitchMouseCooldown;
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

bool UknightTargetLockComponent::GetLockedCameraLocation(FVector& OutLocation) const
{
	AActor* const Target = LockedActor.Get();
	if (!Target)
	{
		return false;
	}
	if (!bLockCameraToTargetCenter)
	{
		return GetLockedTargetLocation(OutLocation);
	}
	OutLocation = bHasSmoothedCameraFocus ? SmoothedCameraFocusLocation : ComputeTargetCenterLocation(Target);
	return true;
}

FVector UknightTargetLockComponent::ComputeTargetCenterLocation(const AActor* Target) const
{
	if (!Target)
	{
		return FVector::ZeroVector;
	}
	const FVector RootLocation = Target->GetActorLocation();
	const ACharacter* const Character = Cast<ACharacter>(Target);
	const USkeletalMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return RootLocation;
	}

	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(Target);
	const UknightMonsterDefinition* const Definition = Monster ? Monster->GetMonsterDefinition() : nullptr;
	FName CoreSocket = Definition && !Definition->FallbackLockOnSocket.IsNone()
		? Definition->FallbackLockOnSocket : FName(TEXT("spine_03"));
	if (!Mesh->DoesSocketExist(CoreSocket))
	{
		CoreSocket = Mesh->DoesSocketExist(TEXT("pelvis")) ? FName(TEXT("pelvis")) : NAME_None;
	}
	// 캡슐 중심을 기본 앵커로 두고 몸통 코어(spine_03/pelvis) 변위를 제한적으로 반영해,
	// 보스가 머리·꼬리를 크게 휘두르거나 부위가 바뀔 때 카메라가 흔들리지 않으면서도 다운·비행 높이는 자연스럽게 따라간다.
	FVector CenterLocation = RootLocation;
	if (!CoreSocket.IsNone())
	{
		const FVector CoreLocation = Mesh->GetSocketLocation(CoreSocket);
		const FVector Offset = CoreLocation - RootLocation;
		const FVector HorizontalOffset = FVector(Offset.X, Offset.Y, 0.f).GetClampedToMaxSize(120.f);
		const float VerticalOffset = FMath::Clamp(Offset.Z, -100.f, 140.f);
		CenterLocation = RootLocation + HorizontalOffset * 0.3f + FVector(0.f, 0.f, VerticalOffset * 0.35f);
	}

	// (75:25 하이브리드 안정화 카메라) 보스 중심(75%)을 축으로 삼고 내가 고른 부위(25%) 쪽으로 시선을 부드럽게 기울인다.
	if (CameraPartLocationWeight > 0.f && Target == LockedActor.Get())
	{
		FVector PartLocation = FVector::ZeroVector;
		if (GetLockedTargetLocation(PartLocation))
		{
			CenterLocation = FMath::Lerp(CenterLocation, PartLocation, FMath::Clamp(CameraPartLocationWeight, 0.f, 1.f));
		}
	}
	return CenterLocation;
}

bool UknightTargetLockComponent::SelectBestPartOnActor(AActor* Target, bool bUseHysteresis, FCandidate& OutCandidate) const
{
	const AActor* const Owner = GetOwner();
	if (!Owner || !Target || !Target->GetClass()->ImplementsInterface(UTargetableInterface::StaticClass()))
	{
		return false;
	}

	const TArray<FknightTargetPoint> Points = ITargetableInterface::Execute_QueryTargetPoints(Target);
	if (Points.IsEmpty())
	{
		return false;
	}

	const FVector OwnerLocation = Owner->GetActorLocation();
	FVector PreferredForward = Owner->GetActorForwardVector().GetSafeNormal2D();
	if (const APawn* const Pawn = Cast<APawn>(Owner))
	{
		const FVector MoveInput = Pawn->GetLastMovementInputVector().GetSafeNormal2D();
		if (!MoveInput.IsNearlyZero())
		{
			PreferredForward = (PreferredForward + MoveInput).GetSafeNormal2D();
		}
	}

	const FknightTargetPoint* BestPoint = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (const FknightTargetPoint& Point : Points)
	{
		if (!Point.Component)
		{
			continue;
		}
		const FVector PointLocation = Point.GetLocation();
		const float Dist2D = FVector::Dist2D(OwnerLocation, PointLocation);
		const float DeltaZ = FMath::Abs(PointLocation.Z - OwnerLocation.Z);
		// 플레이어 수평 위치를 최우선으로 보되, 높이 차이와 바라보는/이동하는 방향에 가산점을 준다.
		float Score = Dist2D + DeltaZ * 0.38f;

		const FVector Dir2D = (PointLocation - OwnerLocation).GetSafeNormal2D();
		if (!Dir2D.IsNearlyZero() && !PreferredForward.IsNearlyZero())
		{
			Score -= FVector::DotProduct(PreferredForward, Dir2D) * 45.f;
		}

		const bool bIsCurrentPoint = Point.Component == LockedComponent.Get() && Point.SocketName == LockedSocket;
		if (bUseHysteresis && bIsCurrentPoint)
		{
			Score -= AutoPartSwitchHysteresis;
		}

		if (Score < BestScore)
		{
			BestScore = Score;
			BestPoint = &Point;
		}
	}

	if (!BestPoint)
	{
		return false;
	}
	OutCandidate.Actor = Target;
	OutCandidate.Point = *BestPoint;
	return true;
}

void UknightTargetLockComponent::RefreshAutoNearestPart(bool bIgnoreCooldown)
{
	AActor* const Target = LockedActor.Get();
	const UWorld* const World = GetWorld();
	if (!bAutoSelectNearestPart || !Target || !World)
	{
		return;
	}
	const double Now = World->GetRealTimeSeconds();
	if (ManualPartPinEndTime > 0.0 && Now < ManualPartPinEndTime)
	{
		return;
	}
	if (!bIgnoreCooldown && LastAutoPartSwitchTime >= 0.0 && Now - LastAutoPartSwitchTime < AutoPartSwitchMinInterval)
	{
		return;
	}

	FCandidate BestCandidate;
	if (!SelectBestPartOnActor(Target, !bIgnoreCooldown, BestCandidate))
	{
		return;
	}
	if (BestCandidate.Point.Component != LockedComponent.Get() || BestCandidate.Point.SocketName != LockedSocket)
	{
		LastAutoPartSwitchTime = Now;
		SetLock(BestCandidate);
	}
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
	if (!Target || IsLockOnBlocked() || !Target->GetClass()->ImplementsInterface(UTargetableInterface::StaticClass())
		|| !ITargetableInterface::Execute_CanBeTargeted(Target) || !GetLockedTargetLocation(TargetLocation))
	{
		ClearLockOn();
		return;
	}
	// 잠근 지점이 락온 지점 목록에서 빠졌으면(잘려 나간 꼬리 등) 같은 대상의 가장 가까운 지점으로 옮긴다.
	if (!KeepLockedPointAvailable(Target, TargetLocation))
	{
		ClearLockOn();
		return;
	}

	// (방안 1) 플레이어 위치 변화에 따라 가장 가까운 부위로 자동 타겟팅 갱신.
	if (bAutoSelectNearestPart)
	{
		RefreshAutoNearestPart(false);
		GetLockedTargetLocation(TargetLocation);
	}

	const FVector DesiredCameraCenter = ComputeTargetCenterLocation(Target);
	SmoothedCameraFocusLocation = bHasSmoothedCameraFocus && CameraFocusInterpSpeed > 0.f
		? FMath::VInterpTo(SmoothedCameraFocusLocation, DesiredCameraCenter, DeltaTime, CameraFocusInterpSpeed)
		: DesiredCameraCenter;
	bHasSmoothedCameraFocus = true;

	const FVector CameraFocusLocation = bLockCameraToTargetCenter ? SmoothedCameraFocusLocation : TargetLocation;
	if (FVector::DistSquared(GetOwner()->GetActorLocation(), CameraFocusLocation) > FMath::Square(BreakRange)
		&& FVector::DistSquared(GetOwner()->GetActorLocation(), TargetLocation) > FMath::Square(BreakRange))
	{
		ClearLockOn();
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	const bool bHasLOS = !GetViewPoint(ViewLocation, ViewRotation)
		|| HasLineOfSight(ViewLocation, CameraFocusLocation, LockedActor.Get())
		|| HasLineOfSight(ViewLocation, TargetLocation, LockedActor.Get());
	if (!bHasLOS)
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

bool UknightTargetLockComponent::FindNearestPointInView(FCandidate& OutCandidate) const
{
	const AActor* const Owner = GetOwner();
	FVector ViewLocation;
	FRotator ViewRotation;
	if (!Owner || !GetViewPoint(ViewLocation, ViewRotation))
	{
		return false;
	}
	const FVector ViewDirection = ViewRotation.Vector();
	const float CosMaxAngle = FMath::Cos(FMath::DegreesToRadians(MaxLockAngle));
	const FVector OwnerLocation = Owner->GetActorLocation();

	TArray<FCandidate> Candidates;
	GatherCandidates(Candidates);

	const FCandidate* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (const FCandidate& Candidate : Candidates)
	{
		const FVector PointLocation = Candidate.Point.GetLocation();
		const FVector ToPoint = PointLocation - ViewLocation;
		const float ViewDistance = ToPoint.Size();
		const float CosAngle = ViewDistance > KINDA_SMALL_NUMBER ? FVector::DotProduct(ToPoint / ViewDistance, ViewDirection) : 1.f;
		if (CosAngle < CosMaxAngle)
		{
			continue;
		}
		// 거리는 카메라가 아니라 플레이어 기준. 시야 검사(트레이스)는 더 가까운 후보일 때만 한다.
		const float DistSq = FVector::DistSquared(OwnerLocation, PointLocation);
		if (DistSq >= BestDistSq || !HasLineOfSight(ViewLocation, PointLocation, Candidate.Actor.Get()))
		{
			continue;
		}
		BestDistSq = DistSq;
		Best = &Candidate;
	}
	if (!Best)
	{
		return false;
	}
	OutCandidate = *Best;
	return true;
}

void UknightTargetLockComponent::SetLock(const FCandidate& Candidate)
{
	const bool bActorChanged = LockedActor.Get() != Candidate.Actor.Get();
	const bool bChanged = bActorChanged || LockedComponent.Get() != Candidate.Point.Component
		|| LockedSocket != Candidate.Point.SocketName;
	LockedActor = Candidate.Actor;
	LockedComponent = Candidate.Point.Component.Get();
	LockedSocket = Candidate.Point.SocketName;
	LockedPartTag = Candidate.Point.PartTag;
	TimeOutOfSight = 0.f;
	if (bActorChanged && LockedActor.IsValid())
	{
		SmoothedCameraFocusLocation = ComputeTargetCenterLocation(LockedActor.Get());
		bHasSmoothedCameraFocus = true;
	}
	SetComponentTickEnabled(true);
	SetLockedOnTag(true);
	if (bChanged)
	{
		UE_LOG(LogknightCombat, Verbose, TEXT("Locked on %s (%s)."), *GetNameSafe(LockedActor.Get()), *LockedPartTag.ToString());
		OnLockOnTargetChanged.Broadcast(LockedActor.Get(), LockedPartTag);
	}
}

bool UknightTargetLockComponent::KeepLockedPointAvailable(AActor* Target, FVector& InOutLocation)
{
	// 몬스터는 부위가 잘려 숨겨지면 그 지점을 목록에서 뺀다(UknightMonsterPartComponent::GetTargetPoints).
	const TArray<FknightTargetPoint> Points = ITargetableInterface::Execute_QueryTargetPoints(Target);
	const FknightTargetPoint* Nearest = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();
	for (const FknightTargetPoint& Point : Points)
	{
		if (!Point.Component)
		{
			continue;
		}
		if (Point.Component == LockedComponent.Get() && Point.SocketName == LockedSocket)
		{
			return true;
		}
		const float DistSq = FVector::DistSquared(Point.GetLocation(), InOutLocation);
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = &Point;
		}
	}
	if (!Nearest)
	{
		return false;
	}
	UE_LOG(LogknightCombat, Log, TEXT("%s: lock-on point %s is gone, moved to %s."), *GetNameSafe(GetOwner()), *LockedPartTag.ToString(), *Nearest->PartTag.ToString());
	InOutLocation = Nearest->GetLocation();
	SetLock({ Target, *Nearest });
	return true;
}

void UknightTargetLockComponent::UpdateControlRotation(float DeltaTime) const
{
	const APawn* const Pawn = Cast<APawn>(GetOwner());
	AController* const Controller = Pawn ? Pawn->GetController() : nullptr;
	FVector TargetLocation;
	FVector ViewLocation;
	FRotator ViewRotation;
	if (!Controller || !GetLockedCameraLocation(TargetLocation) || !GetViewPoint(ViewLocation, ViewRotation))
	{
		return;
	}
	// Measure pitch from the character's chest pivot rather than the orbiting camera location.
	// Measuring from ViewLocation creates a positive feedback loop (looking down raises the boom, which looks
	// further down into a top-view; looking up lowers the boom into the floor/water, which looks further up).
	const FVector LookOrigin = Pawn->GetActorLocation() + FVector(0.f, 0.f, 65.f);
	const float HorizontalDist = FMath::Max(FVector::Dist2D(TargetLocation, LookOrigin), 480.f);
	const float RawPitch = FMath::RadiansToDegrees(FMath::Atan2((TargetLocation.Z - LookOrigin.Z) * 0.65f, HorizontalDist));

	FRotator Desired = (TargetLocation - LookOrigin).Rotation();
	Desired.Pitch = FMath::Clamp(FRotator::NormalizeAxis(RawPitch + PitchOffset), MinPitch, MaxPitch);
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

bool UknightTargetLockComponent::IsLockOnBlocked() const
{
	const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	return ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_LockOn_Blocked);
}

void UknightTargetLockComponent::SetLockedOnTag(bool bLocked) const
{
	if (UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		ASC->SetLooseGameplayTagCount(knightGameplayTags::State_LockedOn, bLocked ? 1 : 0);
	}
}
