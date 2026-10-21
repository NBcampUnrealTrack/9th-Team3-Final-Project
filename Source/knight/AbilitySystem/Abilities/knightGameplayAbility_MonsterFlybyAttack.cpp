#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterFlybyAttack.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Animation/AnimMontage.h"
#include "Combat/knightCombatLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/RootMotionSource.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterDebug.h"
#include "Monster/knightMonsterMovementComponent.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

namespace knightMonsterFlyby
{
	constexpr float TickInterval = 1.f / 60.f;
	constexpr int32 MaxTicks = 100000;
}

UknightGameplayAbility_MonsterFlybyAttack::UknightGameplayAbility_MonsterFlybyAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Selection.MinRange = 800.f;
	Selection.MaxRange = 3000.f;
	Selection.MaxAngle = 180.f;
	HitParams.bCanBeParried = false;
}

void UknightGameplayAbility_MonsterFlybyAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	bPassing = false;
	PhaseTime = 0.f;
	HitActors.Reset();
	SetWarning(true);
	if (WarningMontage)
	{
		UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, WarningMontage);
		MontageTask->ReadyForActivation();
		PlayingWarning = WarningMontage;
	}
	UAbilityTask_Repeat* const Task = UAbilityTask_Repeat::RepeatAction(this, knightMonsterFlyby::TickInterval, knightMonsterFlyby::MaxTicks);
	Task->OnPerformAction.AddDynamic(this, &ThisClass::HandleTick);
	TickTask = Task;
	Task->ReadyForActivation();
}

void UknightGameplayAbility_MonsterFlybyAttack::HandleTick(int32 ActionNumber)
{
	PhaseTime += knightMonsterFlyby::TickInterval;
	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	if (!Monster)
	{
		return;
	}
	if (!bPassing)
	{
		const UknightMonsterTargetingComponent* const Targeting = Monster->GetTargetingComponent();
		const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
		UknightMonsterMovementComponent* const Movement = Monster->GetMonsterMovementComponent();
		if (Movement && Target)
		{
			Movement->TurnTowards(Target->GetActorLocation(), knightMonsterFlyby::TickInterval, TurnRate, 2.f);
		}
#if ENABLE_DRAW_DEBUG
		if (knightMonsterDebug::ShouldDrawAttacks() && Target)
		{
			const FVector From = Monster->GetActorLocation();
			const FVector Dir = (Target->GetActorLocation() - From).GetSafeNormal2D();
			const FVector To = Target->GetActorLocation() + Dir * PassOvershoot;
			DrawDebugLine(GetWorld(), From, To, FColor::Orange, false, knightMonsterFlyby::TickInterval, 0, HalfWidth * 0.2f);
		}
#endif
		if (PhaseTime >= WarningTime)
		{
			StartPass();
		}
		return;
	}
	ApplyHits();
	if (PhaseTime >= PassDuration)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UknightGameplayAbility_MonsterFlybyAttack::StartPass()
{
	AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	const UknightMonsterTargetingComponent* const Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
	const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
	SetWarning(false);
	bPassing = true;
	PhaseTime = 0.f;
	if (PlayingWarning.IsValid())
	{
		MontageStop(0.3f);
		PlayingWarning.Reset();
	}

	// 경고가 끝나는 순간의 타깃 위치로 방향·거리를 고정한다. 이후 타깃이 움직여도 일자로 지나간다.
	FVector Direction = Monster->GetActorForwardVector().GetSafeNormal2D();
	float Travel = PassOvershoot;
	if (Target)
	{
		const FVector ToTarget = Target->GetActorLocation() - Monster->GetActorLocation();
		Direction = ToTarget.GetSafeNormal2D(UE_SMALL_NUMBER, Direction);
		Travel += ToTarget.Size2D();
	}
	Monster->SetActorRotation(Direction.Rotation());
	PassDuration = Travel / PassSpeed;
	UAbilityTask_ApplyRootMotionConstantForce* const PassTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("Flyby"), Direction, PassSpeed, PassDuration, false, nullptr, ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.f, false);
	PassTask->ReadyForActivation();
	UE_LOG(LogknightAI, Log, TEXT("%s: flyby passes %.0f cm in %.2f s."), *GetNameSafe(Monster), Travel, PassDuration);
}

void UknightGameplayAbility_MonsterFlybyAttack::ApplyHits()
{
	ACharacter* const Monster = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const USkeletalMeshComponent* const Mesh = Monster ? Monster->GetMesh() : nullptr;
	if (!Monster)
	{
		return;
	}
	// 가슴 바로 아래 바닥(캡슐 바닥 높이)을 중심으로 한다. 공중에 뜬 메시 높이와 상관없이 바닥의 대상이 맞는다.
	const FVector Chest = Mesh && Mesh->DoesSocketExist(BeamBone) ? Mesh->GetSocketLocation(BeamBone) : Monster->GetActorLocation();
	const UCapsuleComponent* const Capsule = Monster->GetCapsuleComponent();
	const float GroundZ = Monster->GetActorLocation().Z - (Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.f);
	const FVector Center(Chest.X, Chest.Y, GroundZ);
	const int32 NewHits = UknightCombatLibrary::ApplyAreaMeleeHits(Monster, Center, FCollisionShape::MakeSphere(HalfWidth), HitParams, HitActors);
#if ENABLE_DRAW_DEBUG
	if (knightMonsterDebug::ShouldDrawAttacks())
	{
		DrawDebugSphere(GetWorld(), Center, HalfWidth, 12, NewHits == 0 ? FColor::Red : FColor::Magenta, false, 0.5f);
	}
#endif
}

void UknightGameplayAbility_MonsterFlybyAttack::SetWarning(bool bOn)
{
	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || bOn == bWarningApplied)
	{
		return;
	}
	if (bOn)
	{
		ASC->AddLooseGameplayTag(knightGameplayTags::State_Monster_Warning);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(knightGameplayTags::State_Monster_Warning);
	}
	bWarningApplied = bOn;
}

void UknightGameplayAbility_MonsterFlybyAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (TickTask.IsValid())
	{
		TickTask->EndTask();
	}
	TickTask.Reset();
	SetWarning(false);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
