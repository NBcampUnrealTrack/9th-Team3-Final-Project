#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterLeapAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Combat/knightCombatLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "Monster/knightMonsterDebug.h"
#include "knight.h"

namespace knightMonsterLeap
{
	constexpr float TickInterval = 1.f / 30.f;
	/** 반복 작업의 횟수 상한. 실제로는 몽타주가 끝날 때 작업을 끝낸다. */
	constexpr int32 MaxTicks = 100000;
}

UknightGameplayAbility_MonsterLeapAttack::UknightGameplayAbility_MonsterLeapAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Selection.MinRange = 2500.f;
	Selection.MaxRange = 5000.f;
	Selection.MaxAngle = 30.f;
}

void UknightGameplayAbility_MonsterLeapAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	if (!Montage || !Monster)
	{
		UE_LOG(LogknightAbility, Error, TEXT("%s needs a Montage and a monster avatar."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	HitActors.Reset();
	bAirborne = false;
	bMontageEnded = false;
	LeapScale = 1.f;
	MeshScale = Monster->GetMesh() ? Monster->GetMesh()->GetComponentScale().X : 1.f;

	// 가까운 타깃이면 달리기 앞부분을 건너뛴다. 남은 차이와 타깃의 움직임은 도약 이동 배율이 맞춘다.
	const UknightMonsterTargetingComponent* const Targeting = Monster->GetTargetingComponent();
	const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
	const float Distance = Target ? FVector::Dist2D(Monster->GetActorLocation(), Target->GetActorLocation()) : 0.f;
	const float StartTime = Target ? FindStartTime(Distance) : FMath::Min(MaxStartTime, LeapStartTime);
	if (Target)
	{
		UpdateLeapScale(*Target, StartTime);
	}
	UE_LOG(LogknightAI, Verbose, TEXT("%s: %s leap at %.0f cm, starting at %.2fs (scale %.2f)."), *GetNameSafe(Monster), *GetName(), Distance, StartTime, LeapScale);

	RushEffects.Start(*Monster);

	UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage,
		1.f, NAME_None, true, 1.f, StartTime);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->ReadyForActivation();

	UAbilityTask_Repeat* const Task = UAbilityTask_Repeat::RepeatAction(this, knightMonsterLeap::TickInterval, knightMonsterLeap::MaxTicks);
	Task->OnPerformAction.AddDynamic(this, &ThisClass::HandleTick);
	TickTask = Task;
	Task->ReadyForActivation();
}

void UknightGameplayAbility_MonsterLeapAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	SetAirborne(false);
	RushEffects.Stop();
	if (ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		Character->SetAnimRootMotionTranslationScale(1.f);
	}
	HitActors.Reset();
	TickTask.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

float UknightGameplayAbility_MonsterLeapAttack::GetTravel(float From, float To) const
{
	return From >= To ? 0.f : Montage->ExtractRootMotionFromTrackRange(From, To, FAnimExtractContext()).GetTranslation().Size2D() * MeshScale;
}

float UknightGameplayAbility_MonsterLeapAttack::FindStartTime(float Distance) const
{
	// 시작 시각이 늦을수록 BiteTime까지의 이동이 짧다. 이분 탐색으로 이동 + BiteReach가 Distance가 되는 시각을 찾는다.
	const float Needed = Distance - BiteReach;
	float Low = 0.f;
	float High = FMath::Min(MaxStartTime, LeapStartTime);
	if (GetTravel(Low, BiteTime) <= Needed)
	{
		return Low;
	}
	if (GetTravel(High, BiteTime) >= Needed)
	{
		return High;
	}
	for (int32 Step = 0; Step < 12; ++Step)
	{
		const float Mid = 0.5f * (Low + High);
		(GetTravel(Mid, BiteTime) > Needed ? Low : High) = Mid;
	}
	return High;
}

void UknightGameplayAbility_MonsterLeapAttack::UpdateLeapScale(const AActor& Target, float Position)
{
	const AActor* const Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}
	// 도약 전 남은 달리기는 원래 속도로 가므로 빼고, 남은 도약 이동이 나머지를 채우도록 배율을 정한다.
	const float Distance = FVector::Dist2D(Avatar->GetActorLocation(), Target.GetActorLocation());
	const float Run = GetTravel(Position, LeapStartTime);
	const float Leap = GetTravel(FMath::Max(Position, LeapStartTime), BiteTime);
	if (Leap > KINDA_SMALL_NUMBER)
	{
		LeapScale = FMath::Clamp((Distance - BiteReach - Run) / Leap, MinLeapScale, MaxLeapScale);
	}
}

void UknightGameplayAbility_MonsterLeapAttack::HandleTick(int32 ActionNumber)
{
	AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	UAnimInstance* const AnimInstance = GetCurrentActorInfo() ? GetCurrentActorInfo()->GetAnimInstance() : nullptr;
	if (!Monster || !AnimInstance || bMontageEnded)
	{
		if (TickTask.IsValid())
		{
			TickTask->EndTask();
		}
		return;
	}

	const float DeltaTime = GetMonsterTimeStep(knightMonsterLeap::TickInterval);
	const float Position = AnimInstance->Montage_GetPosition(Montage);
	const UknightMonsterTargetingComponent* const Targeting = Monster->GetTargetingComponent();
	const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;

	// 도약 방향이 고정될 때까지 몸을 타깃 쪽으로 튼다.
	if (Target && Position <= TrackTargetUntil && RunSteerRate > 0.f)
	{
		const FRotator Current = Monster->GetActorRotation();
		const float DesiredYaw = (Target->GetActorLocation() - Monster->GetActorLocation()).Rotation().Yaw;
		const float Step = FMath::Clamp(FMath::FindDeltaAngleDegrees(Current.Yaw, DesiredYaw), -RunSteerRate * DeltaTime, RunSteerRate * DeltaTime);
		Monster->SetActorRotation(FRotator(Current.Pitch, Current.Yaw + Step, Current.Roll));
	}
	if (Target && Position <= TrackTargetUntil)
	{
		UpdateLeapScale(*Target, Position);
	}
	Monster->SetAnimRootMotionTranslationScale(Position >= LeapStartTime ? LeapScale : 1.f);
	SetAirborne(Position >= TakeoffTime && Position < LandTime);
	if (Position >= LandTime)
	{
		RushEffects.Stop();
	}
	RushEffects.Tick(*Monster, DeltaTime, !bAirborne);
	if (Position >= HitStartTime && Position <= HitEndTime)
	{
		ApplyHits();
	}

#if ENABLE_DRAW_DEBUG
	if (knightMonsterDebug::ShouldDrawAttacks() && Position <= BiteTime)
	{
		const float Ahead = GetTravel(Position, LeapStartTime) + GetTravel(FMath::Max(Position, LeapStartTime), BiteTime) * LeapScale + BiteReach;
		const FVector BitePoint = Monster->GetActorLocation() + Monster->GetActorForwardVector().GetSafeNormal2D() * Ahead;
		DrawDebugSphere(GetWorld(), BitePoint, 50.f, 8, FColor::Yellow, false, knightMonsterLeap::TickInterval, 0, 3.f);
	}
#endif
}

void UknightGameplayAbility_MonsterLeapAttack::SetAirborne(bool bInAirborne)
{
	if (bAirborne == bInAirborne)
	{
		return;
	}
	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	UCharacterMovementComponent* const Movement = Monster ? Monster->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return;
	}
	bAirborne = bInAirborne;
	// 걷기 모드는 루트 모션의 Z를 버린다. 공중 구간만 비행 모드로 높이를 따르고, 끝나면 떨어뜨려 바닥에 붙인다.
	Movement->SetMovementMode(bAirborne ? MOVE_Flying : MOVE_Falling);
	UE_LOG(LogknightAI, Verbose, TEXT("%s: %s %s."), *GetNameSafe(Monster), *GetName(), bAirborne ? TEXT("took off") : TEXT("landed"));
}

void UknightGameplayAbility_MonsterLeapAttack::ApplyHits()
{
	AActor* const Avatar = GetAvatarActorFromActorInfo();
	UWorld* const World = GetWorld();
	if (!Avatar || !World)
	{
		return;
	}

	const USkeletalMeshComponent* const Mesh = GetCurrentActorInfo() ? GetCurrentActorInfo()->SkeletalMeshComponent.Get() : nullptr;
	const FVector Anchor = Mesh && !HitSocket.IsNone() && Mesh->DoesSocketExist(HitSocket) ? Mesh->GetSocketLocation(HitSocket) : Avatar->GetActorLocation();
	const FVector Center = Anchor + Avatar->GetActorQuat().RotateVector(HitOffset);
	const int32 NewHits = UknightCombatLibrary::ApplyAreaMeleeHits(Avatar, Center, FCollisionShape::MakeSphere(HitRadius), HitParams, HitActors);

#if ENABLE_DRAW_DEBUG
	if (knightMonsterDebug::ShouldDrawAttacks())
	{
		DrawDebugSphere(World, Center, HitRadius, 16, NewHits == 0 ? FColor::Red : FColor::Magenta, false, knightMonsterLeap::TickInterval, 0, 3.f);
	}
#endif
}

void UknightGameplayAbility_MonsterLeapAttack::HandleMontageEnded()
{
	if (bMontageEnded)
	{
		return;
	}
	bMontageEnded = true;
	SetAirborne(false);
	RushEffects.Stop();
	// 후딜 동안 다른 루트 모션이 끼어들어도 도약 배율이 남지 않게 바로 되돌린다.
	if (ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		Character->SetAnimRootMotionTranslationScale(1.f);
	}
	UAbilityTask_WaitDelay* const RecoveryTask = UAbilityTask_WaitDelay::WaitDelay(this, RecoveryTime);
	RecoveryTask->OnFinish.AddDynamic(this, &ThisClass::HandleRecoveryFinished);
	RecoveryTask->ReadyForActivation();
}

void UknightGameplayAbility_MonsterLeapAttack::HandleMontageInterrupted()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

void UknightGameplayAbility_MonsterLeapAttack::HandleRecoveryFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}
