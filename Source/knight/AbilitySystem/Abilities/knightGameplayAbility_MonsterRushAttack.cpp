#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterRushAttack.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Animation/AnimMontage.h"
#include "Combat/knightCombatLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/RootMotionSource.h"
#include "Monster/knightMonsterAirTurnComponent.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterDebug.h"
#include "Monster/knightMonsterHoverComponent.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

namespace knightMonsterRush
{
	constexpr float TickInterval = 1.f / 60.f;
	constexpr int32 MaxTicks = 100000;
}

UknightGameplayAbility_MonsterRushAttack::UknightGameplayAbility_MonsterRushAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Selection.MinRange = 800.f;
	Selection.MaxRange = 2000.f;
	Selection.MaxAngle = 180.f;
}

void UknightGameplayAbility_MonsterRushAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (Steps.IsEmpty())
	{
		UE_LOG(LogknightAbility, Error, TEXT("%s has no Steps."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	StepIndex = 0;
	SetNoFlinch(true);
	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	if (UknightMonsterHoverComponent* const Hover = Monster ? Monster->FindComponentByClass<UknightMonsterHoverComponent>() : nullptr)
	{
		// 바닥의 타깃에 닿도록 몰아치는 동안 낮게 난다.
		Hover->SetAirborne(false, DescendTime);
	}
	StartReposition();
	UAbilityTask_Repeat* const Task = UAbilityTask_Repeat::RepeatAction(this, knightMonsterRush::TickInterval, knightMonsterRush::MaxTicks);
	Task->OnPerformAction.AddDynamic(this, &ThisClass::HandleTick);
	TickTask = Task;
	Task->ReadyForActivation();
}

void UknightGameplayAbility_MonsterRushAttack::HandleTick(int32 ActionNumber)
{
	// 몬스터 시간으로 센다. 타격 히트스톱으로 몽타주가 멈춘 동안 시계만 흐르면 다음 걸음이 몸보다 먼저 나간다.
	PhaseTime += GetMonsterTimeStep(knightMonsterRush::TickInterval);
	AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	if (!Monster || !Steps.IsValidIndex(StepIndex))
	{
		return;
	}
	const FknightMonsterRushStep& Step = Steps[StepIndex];
	switch (Phase)
	{
	case EPhase::Reposition:
	{
		const float Alpha = PhaseDuration > 0.f ? FMath::Clamp(PhaseTime / PhaseDuration, 0.f, 1.f) : 1.f;
		const float Yaw = StartYaw + FMath::FindDeltaAngleDegrees(StartYaw, TargetYaw) * FMath::SmoothStep(0.f, 1.f, Alpha);
		Monster->SetActorRotation(FRotator(0.f, Yaw, 0.f));
		if (Alpha >= 1.f)
		{
			StartStep();
		}
		break;
	}
	case EPhase::Step:
		if (StepTurnYaw != 0.f && Step.ContactTime > 0.f && PhaseTime <= Step.ContactTime + knightMonsterRush::TickInterval)
		{
			const float Alpha = FMath::Clamp(PhaseTime / Step.ContactTime, 0.f, 1.f);
			Monster->SetActorRotation(FRotator(0.f, StepStartYaw + StepTurnYaw * FMath::SmoothStep(0.f, 1.f, Alpha), 0.f));
		}
		if (PhaseTime >= Step.HitStartTime && PhaseTime <= Step.HitEndTime)
		{
			ApplyHits(Step);
		}
		if (PhaseTime >= PhaseDuration)
		{
			if (++StepIndex < Steps.Num())
			{
				StartReposition();
			}
			else
			{
				EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
			}
		}
		break;
	}
}

void UknightGameplayAbility_MonsterRushAttack::StartReposition()
{
	AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	const UknightMonsterTargetingComponent* const Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
	const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
	const USkeletalMeshComponent* const Mesh = Monster ? Monster->GetMesh() : nullptr;
	Phase = EPhase::Reposition;
	PhaseTime = 0.f;
	PhaseDuration = 0.f;
	StartYaw = Monster ? Monster->GetActorRotation().Yaw : 0.f;
	TargetYaw = StartYaw;
	if (!Target || !Mesh)
	{
		return;
	}
	// 접촉 지점을 몸(캡슐) 기준으로 바꾼다. 메시의 상대 회전과 크기를 그대로 쓴다.
	const FknightMonsterRushStep& Step = Steps[StepIndex];
	const FVector Scale = Mesh->GetComponentScale();
	const FVector Local = Mesh->GetRelativeRotation().RotateVector(FVector(Step.ContactPoint.X * Scale.X, Step.ContactPoint.Y * Scale.Y, 0.f));
	const float Reach = Local.Size2D();
	const float ContactYaw = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));

	// 타깃 둘레(반지름 Reach)에서 지금 자리와 가장 가까운 곳으로 간다. 그 자리에서 접촉 지점이 타깃에 오게 돈다.
	const FVector Current = Monster->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	const FVector ToTarget = (TargetLocation - Current).GetSafeNormal2D(UE_SMALL_NUMBER, Monster->GetActorForwardVector());
	FVector Destination = TargetLocation - ToTarget * Reach;
	Destination.Z = Current.Z;
	TargetYaw = ToTarget.Rotation().Yaw - ContactYaw;
	StepTurnYaw = 0.f;
	if (Step.bFaceTargetAtStart)
	{
		// 자리는 같고(타깃에서 Reach 거리), 접촉 각도만큼은 동작 중에 돈다.
		TargetYaw = ToTarget.Rotation().Yaw;
		StepTurnYaw = -ContactYaw;
	}

	const FVector Move = Destination - Current;
	const float Distance = Move.Size2D();
	PhaseDuration = FMath::Clamp(Distance / RepositionSpeed, RepositionMinTime, FMath::Max(RepositionMinTime, RepositionMaxTime));
	if (Distance > 1.f)
	{
		UAbilityTask_ApplyRootMotionConstantForce* const MoveTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
			this, TEXT("RushReposition"), Move.GetSafeNormal2D(), Distance / PhaseDuration, PhaseDuration, false, nullptr,
			ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.f, false);
		MoveTask->ReadyForActivation();
	}
	UE_LOG(LogknightAI, Log, TEXT("%s: rush step %d repositions %.0f cm in %.2f s (contact reach %.0f cm, %.0f deg)."),
		*GetNameSafe(Monster), StepIndex + 1, Distance, PhaseDuration, Reach, ContactYaw);
}

void UknightGameplayAbility_MonsterRushAttack::StartStep()
{
	const FknightMonsterRushStep& Step = Steps[StepIndex];
	Phase = EPhase::Step;
	PhaseTime = 0.f;
	HitActors.Reset();
	const AActor* const Avatar = GetAvatarActorFromActorInfo();
	StepStartYaw = Avatar ? Avatar->GetActorRotation().Yaw : 0.f;
	const float Length = Step.Montage ? Step.Montage->GetPlayLength() : Step.HitEndTime;
	PhaseDuration = Step.EndTime > 0.f ? FMath::Min(Step.EndTime, Length) : Length;
	if (Step.Montage)
	{
		UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Step.Montage);
		MontageTask->ReadyForActivation();
	}
}

void UknightGameplayAbility_MonsterRushAttack::ApplyHits(const FknightMonsterRushStep& Step)
{
	AActor* const Avatar = GetAvatarActorFromActorInfo();
	const USkeletalMeshComponent* const Mesh = GetCurrentActorInfo() ? GetCurrentActorInfo()->SkeletalMeshComponent.Get() : nullptr;
	if (!Avatar)
	{
		return;
	}
	const FVector Center = Mesh && Mesh->DoesSocketExist(Step.HitBone) ? Mesh->GetSocketLocation(Step.HitBone) : Avatar->GetActorLocation();
	const int32 NewHits = UknightCombatLibrary::ApplyAreaMeleeHits(Avatar, Center, FCollisionShape::MakeSphere(Step.HitRadius), HitParams, HitActors);
#if ENABLE_DRAW_DEBUG
	if (knightMonsterDebug::ShouldDrawAttacks())
	{
		DrawDebugSphere(GetWorld(), Center, Step.HitRadius, 16, NewHits == 0 ? FColor::Red : FColor::Magenta, false, knightMonsterRush::TickInterval, 0, 3.f);
	}
#endif
}

void UknightGameplayAbility_MonsterRushAttack::SetNoFlinch(bool bOn)
{
	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || bOn == bNoFlinchApplied)
	{
		return;
	}
	if (bOn)
	{
		ASC->AddLooseGameplayTag(knightGameplayTags::State_Monster_NoFlinch);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(knightGameplayTags::State_Monster_NoFlinch);
	}
	bNoFlinchApplied = bOn;
}

void UknightGameplayAbility_MonsterRushAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (TickTask.IsValid())
	{
		TickTask->EndTask();
	}
	TickTask.Reset();
	SetNoFlinch(false);

	// 다시 떠오른다. 격추(다운)됐거나 공중 턴이 끝났으면 그쪽이 높이를 정한다.
	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	const UknightMonsterAirTurnComponent* const AirTurn = Monster ? Monster->FindComponentByClass<UknightMonsterAirTurnComponent>() : nullptr;
	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	UknightMonsterHoverComponent* const Hover = Monster ? Monster->FindComponentByClass<UknightMonsterHoverComponent>() : nullptr;
	if (Hover && AirTurn && AirTurn->IsAirborne() && !Monster->IsDead()
		&& !(ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_HitReact_Knockdown)))
	{
		Hover->SetAirborne(true, RiseTime);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
