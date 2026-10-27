#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterDiveAttack.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Combat/knightCombatLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Monster/knightMonsterAirTurnComponent.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterDebug.h"
#include "Monster/knightMonsterHoverComponent.h"
#include "Monster/knightMonsterMovementComponent.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

namespace knightMonsterDive
{
	constexpr float TickInterval = 1.f / 60.f;
	/** 반복 작업의 횟수 상한. 실제로는 결과 몽타주가 끝날 때 작업을 끝낸다. */
	constexpr int32 MaxTicks = 100000;
}

UknightGameplayAbility_MonsterDiveAttack::UknightGameplayAbility_MonsterDiveAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Selection.MinRange = 800.f;
	Selection.MaxRange = 2500.f;
	// 접는 동안 타깃 쪽으로 돈다.
	Selection.MaxAngle = 180.f;
}

void UknightGameplayAbility_MonsterDiveAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!Montage)
	{
		UE_LOG(LogknightAbility, Error, TEXT("%s has no Montage."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	HitActors.Reset();
	Phase = EPhase::Fold;
	PhaseTime = 0.f;
	bConnected = false;
	bParried = false;
	bFallGetUpStarted = false;
	if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AddLooseGameplayTag(knightGameplayTags::State_Monster_NoFlinch);
		bNoFlinchApplied = true;
	}

	UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, 1.f, FoldSection);
	MontageTask->ReadyForActivation();

	UAbilityTask_Repeat* const Task = UAbilityTask_Repeat::RepeatAction(this, knightMonsterDive::TickInterval, knightMonsterDive::MaxTicks);
	Task->OnPerformAction.AddDynamic(this, &ThisClass::HandleTick);
	TickTask = Task;
	Task->ReadyForActivation();
}

void UknightGameplayAbility_MonsterDiveAttack::HandleTick(int32 ActionNumber)
{
	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	if (!Monster)
	{
		return;
	}
	// 몬스터 시간으로 센다. 패링 히트스톱으로 몸(이동·몽타주)이 멈추는 동안 시계만 흐르면, 돌진이 덜 끝난 채 결과
	// 몽타주가 나오고 남은 돌진 이동에 쓰러지는 몸이 미끄러진다.
	const float DeltaTime = knightMonsterDive::TickInterval * FMath::Max(Monster->CustomTimeDilation, 0.f);
	PhaseTime += DeltaTime;
	if (Phase == EPhase::Fold)
	{
		const UknightMonsterTargetingComponent* const Targeting = Monster->GetTargetingComponent();
		const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
		if (UknightMonsterMovementComponent* const Movement = Monster->GetMonsterMovementComponent(); Movement && Target)
		{
			Movement->TurnTowards(Target->GetActorLocation(), DeltaTime, FoldTurnRate, 2.f);
		}
		if (PhaseTime >= FoldTime)
		{
			StartDive();
		}
		return;
	}
	if (Phase == EPhase::Dive)
	{
		ApplyHits();
		if (bParried || PhaseTime >= DiveDuration)
		{
			FinishDive();
		}
	}
}

void UknightGameplayAbility_MonsterDiveAttack::StartDive()
{
	AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	const UknightMonsterTargetingComponent* const Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
	const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
	Phase = EPhase::Dive;
	PhaseTime = 0.f;

	// 접는 순간의 타깃 위치로 고정한다. 그 뒤에 옆으로 피하면 빗나간다.
	const FVector Start = Monster->GetActorLocation();
	FVector Direction = Monster->GetActorForwardVector().GetSafeNormal2D();
	float Distance = 0.f;
	if (Target)
	{
		const FVector ToTarget = Target->GetActorLocation() - Start;
		Direction = ToTarget.GetSafeNormal2D(UE_SMALL_NUMBER, Direction);
		Distance = FMath::Max(0.f, static_cast<float>(ToTarget.Size2D()) + Overshoot - HeadReach);
	}
	DiveDuration = FMath::Max(MinDiveTime, Distance / DiveSpeed);
	Monster->SetActorRotation(Direction.Rotation());

	if (UAnimInstance* const Anim = Monster->GetMesh() ? Monster->GetMesh()->GetAnimInstance() : nullptr)
	{
		Anim->Montage_JumpToSection(LoopSection, Montage);
	}
	UAbilityTask_ApplyRootMotionConstantForce* const Task = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("Dive"), Direction, Distance / DiveDuration, DiveDuration, false, nullptr,
		ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.f, false);
	DiveTask = Task;
	Task->ReadyForActivation();
	// 같은 시간 동안 공중 높이에서 바닥 높이로 내려온다.
	if (UknightMonsterHoverComponent* const Hover = Monster->FindComponentByClass<UknightMonsterHoverComponent>())
	{
		Hover->SetAirborne(false, DiveDuration);
	}
#if ENABLE_DRAW_DEBUG
	if (knightMonsterDebug::ShouldDrawAttacks())
	{
		DrawDebugSphere(GetWorld(), Start + Direction * (Distance + HeadReach), 100.f, 12, FColor::Yellow, false, DiveDuration + 1.f);
	}
#endif
	UE_LOG(LogknightAI, Log, TEXT("%s: dive %.0f cm over %.2f s."), *GetNameSafe(Monster), Distance, DiveDuration);
}

void UknightGameplayAbility_MonsterDiveAttack::FinishDive()
{
	Phase = EPhase::Outcome;
	ApplyHits();
	// 결과 몽타주는 제자리에서 재생한다. 남은 돌진 이동이 있으면 여기서 끊는다(끊으면 마무리 속도 0이 적용되지 않아 직접 멈춘다).
	if (DiveTask.IsValid() && DiveTask->IsActive())
	{
		DiveTask->EndTask();
		if (ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
		{
			Character->GetCharacterMovement()->StopMovementImmediately();
		}
	}
	DiveTask.Reset();
	if (bParried && !bConnected)
	{
		if (UknightMonsterHoverComponent* const Hover = GetAvatarActorFromActorInfo()->FindComponentByClass<UknightMonsterHoverComponent>())
		{
			Hover->SetAirborne(false, ParriedDropTime);
		}
	}
	UAnimMontage* const Outcome = bConnected ? StuckMontage.Get() : FallMontage.Get();
	UE_LOG(LogknightAI, Log, TEXT("%s: dive %s."), *GetNameSafe(GetAvatarActorFromActorInfo()),
		bConnected ? TEXT("connected (stuck)") : bParried ? TEXT("parried (fall)") : TEXT("missed (fall)"));
	if (!Outcome)
	{
		HandleOutcomeFinished();
		return;
	}
	UAbilityTask_PlayMontageAndWait* const OutcomeTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Outcome,
		1.f, NAME_None, true, 1.f, bConnected ? StuckStartTime : FallStartTime);
	if (!bConnected && FallGetUpMontage)
	{
		OutcomeTask->OnCompleted.AddDynamic(this, &ThisClass::HandleFallBlendOut);
		OutcomeTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleFallBlendOut);
	}
	else
	{
		OutcomeTask->OnCompleted.AddDynamic(this, &ThisClass::HandleOutcomeFinished);
		OutcomeTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleOutcomeFinished);
	}
	OutcomeTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleOutcomeFinished);
	OutcomeTask->OnCancelled.AddDynamic(this, &ThisClass::HandleOutcomeFinished);
	OutcomeTask->ReadyForActivation();
}

void UknightGameplayAbility_MonsterDiveAttack::HandleFallBlendOut()
{
	if (!IsActive() || bFallGetUpStarted)
	{
		return;
	}
	// 쓰러진 몽타주가 빠지는 동안 일어나기가 그 자세에서 블렌드 인한다.
	bFallGetUpStarted = true;
	UAbilityTask_PlayMontageAndWait* const GetUpTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, FallGetUpMontage);
	GetUpTask->OnCompleted.AddDynamic(this, &ThisClass::HandleOutcomeFinished);
	GetUpTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleOutcomeFinished);
	GetUpTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleOutcomeFinished);
	GetUpTask->OnCancelled.AddDynamic(this, &ThisClass::HandleOutcomeFinished);
	GetUpTask->ReadyForActivation();
}

void UknightGameplayAbility_MonsterDiveAttack::HandleOutcomeFinished()
{
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UknightGameplayAbility_MonsterDiveAttack::ApplyHits()
{
	AActor* const Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}
	const USkeletalMeshComponent* const Mesh = GetCurrentActorInfo() ? GetCurrentActorInfo()->SkeletalMeshComponent.Get() : nullptr;
	const FVector Anchor = Mesh && !HitSocket.IsNone() && Mesh->DoesSocketExist(HitSocket) ? Mesh->GetSocketLocation(HitSocket) : Avatar->GetActorLocation();
	const FVector Center = Anchor + Avatar->GetActorQuat().RotateVector(HitOffset);
	const int32 NewHits = UknightCombatLibrary::ApplyAreaMeleeHits(Avatar, Center, FCollisionShape::MakeSphere(HitRadius), HitParams, HitActors,
		[](const AActor&) { return true; },
		[this](AActor& Target, EknightHitOutcome Outcome)
		{
			bConnected |= Outcome == EknightHitOutcome::Hit || Outcome == EknightHitOutcome::Guarded;
			bParried |= Outcome == EknightHitOutcome::Parried;
		});
#if ENABLE_DRAW_DEBUG
	if (knightMonsterDebug::ShouldDrawAttacks())
	{
		DrawDebugSphere(GetWorld(), Center, HitRadius, 16, NewHits == 0 ? FColor::Red : FColor::Magenta, false, knightMonsterDive::TickInterval, 0, 3.f);
	}
#endif
}

void UknightGameplayAbility_MonsterDiveAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (TickTask.IsValid())
	{
		TickTask->EndTask();
	}
	TickTask.Reset();
	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC && bNoFlinchApplied)
	{
		ASC->RemoveLooseGameplayTag(knightGameplayTags::State_Monster_NoFlinch);
	}
	bNoFlinchApplied = false;

	// 공중 턴이 이어지면 다시 공중 높이로 오른다. 격추(다운)·사망이면 공중 턴 컴포넌트가 이미 바닥으로 내렸다.
	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	const UknightMonsterAirTurnComponent* const AirTurn = Monster ? Monster->FindComponentByClass<UknightMonsterAirTurnComponent>() : nullptr;
	UknightMonsterHoverComponent* const Hover = Monster ? Monster->FindComponentByClass<UknightMonsterHoverComponent>() : nullptr;
	if (AirTurn && AirTurn->IsAirborne() && Hover && !Monster->IsDead()
		&& !(ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_HitReact_Knockdown)))
	{
		Hover->SetAirborne(true, RiseTime);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
