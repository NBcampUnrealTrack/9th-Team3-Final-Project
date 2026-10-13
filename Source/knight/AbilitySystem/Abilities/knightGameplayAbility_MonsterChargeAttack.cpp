#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterChargeAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Combat/knightCombatLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "GenericTeamAgentInterface.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/CombatInterface.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterMovementComponent.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "knight.h"

namespace knightMonsterCharge
{
	constexpr float TickInterval = 1.f / 30.f;
	/** 반복 작업의 횟수 상한. 실제로는 몽타주가 끝날 때 작업을 끝낸다. */
	constexpr int32 MaxTicks = 100000;

	bool ShouldDraw()
	{
		static const IConsoleVariable* const CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("knight.Monster.DrawAttacks"));
		return CVar && CVar->GetInt() != 0;
	}
}

UknightGameplayAbility_MonsterChargeAttack::UknightGameplayAbility_MonsterChargeAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Selection.MinRange = 1200.f;
	Selection.MaxRange = 3500.f;
	Selection.MaxAngle = 30.f;
}

void UknightGameplayAbility_MonsterChargeAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
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
	LoopTime = 0.f;
	SlowTime = 0.f;
	bStopping = false;
	bMontageEnded = false;
	bHitTarget = false;
	KnockbackRemaining = 0.f;
	LastLocation = GetAvatarActorFromActorInfo()->GetActorLocation();

	UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, 1.f, StartSection);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->ReadyForActivation();

	UAbilityTask_Repeat* const Task = UAbilityTask_Repeat::RepeatAction(this, knightMonsterCharge::TickInterval, knightMonsterCharge::MaxTicks);
	Task->OnPerformAction.AddDynamic(this, &ThisClass::HandleTick);
	TickTask = Task;
	Task->ReadyForActivation();
}

void UknightGameplayAbility_MonsterChargeAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	HitActors.Reset();
	TickTask.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UknightGameplayAbility_MonsterChargeAttack::HandleTick(int32 ActionNumber)
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

	const float DeltaTime = knightMonsterCharge::TickInterval;
	TickKnockback(DeltaTime);
	const FName Section = AnimInstance->Montage_GetCurrentSection(Montage);
	const float Position = AnimInstance->Montage_GetPosition(Montage);
	float SectionStart = 0.f;
	float SectionEnd = 0.f;
	Montage->GetSectionStartAndEndTime(Montage->GetSectionIndex(Section), SectionStart, SectionEnd);
	const float SectionTime = Position - SectionStart;

	const UknightMonsterTargetingComponent* const Targeting = Monster->GetTargetingComponent();
	const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;

	// 출발하는 동안에는 가속 없이 바로 돌아 타깃을 겨눈다(등 뒤의 타깃도 0.93초 안에 거의 정면이 된다).
	// 달리기 시작하면 조금씩만 튼다. 타깃을 맞힌 뒤에는 날아간 타깃을 쫓지 않고 직진해 지나친다.
	if (Target && Section == StartSection && StartSteerRate > 0.f)
	{
		const FRotator Current = Monster->GetActorRotation();
		const float DesiredYaw = (Target->GetActorLocation() - Monster->GetActorLocation()).Rotation().Yaw;
		const float Step = FMath::Clamp(FMath::FindDeltaAngleDegrees(Current.Yaw, DesiredYaw), -StartSteerRate * DeltaTime, StartSteerRate * DeltaTime);
		Monster->SetActorRotation(FRotator(Current.Pitch, Current.Yaw + Step, Current.Roll));
	}
	else if (Target && Section == LoopSection && SteerRate > 0.f && !bHitTarget)
	{
		if (UknightMonsterMovementComponent* const Movement = Monster->GetMonsterMovementComponent())
		{
			Movement->TurnTowards(Target->GetActorLocation(), DeltaTime, SteerRate, 0.f);
		}
	}

	const bool bHitOpen = (Section == StartSection && SectionTime >= HitStartTime) || Section == LoopSection
		|| (Section == StopSection && SectionTime <= StopHitTime);
	if (bHitOpen)
	{
		ApplyHits();
	}

	const FVector Location = Monster->GetActorLocation();
	const float Speed = FVector::Dist2D(Location, LastLocation) / DeltaTime;
	LastLocation = Location;

	if (Section != LoopSection || bStopping)
	{
		return;
	}
	LoopTime += DeltaTime;
	SlowTime = Speed < BlockedSpeed ? SlowTime + DeltaTime : 0.f;
	if (SlowTime >= BlockedTime)
	{
		BeginStop(true, TEXT("blocked"));
		return;
	}
	if (!Target)
	{
		BeginStop(false, TEXT("lost target"));
		return;
	}
	const FVector ToTarget = Target->GetActorLocation() - Location;
	const float TargetAhead = FVector::DotProduct(FVector(ToTarget.X, ToTarget.Y, 0.f), Monster->GetActorForwardVector().GetSafeNormal2D());
	if (TargetAhead < -PassDistance)
	{
		BeginStop(false, TEXT("passed target"));
	}
	else if (LoopTime >= MaxLoopTime)
	{
		BeginStop(false, TEXT("max time"));
	}
}

void UknightGameplayAbility_MonsterChargeAttack::BeginStop(bool bImmediate, const TCHAR* Reason)
{
	UAnimInstance* const AnimInstance = GetCurrentActorInfo() ? GetCurrentActorInfo()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}
	bStopping = true;
	// 보통은 지금 Loop 한 바퀴를 마치고 Stop으로 넘어가 발 박자가 끊기지 않게 한다. 막혔을 때만 바로 넘어간다.
	if (bImmediate)
	{
		AnimInstance->Montage_JumpToSection(StopSection, Montage);
	}
	else
	{
		AnimInstance->Montage_SetNextSection(LoopSection, StopSection, Montage);
	}
	UE_LOG(LogknightAI, Verbose, TEXT("%s: %s stopping (%s) after %.2fs of loop."), *GetNameSafe(GetAvatarActorFromActorInfo()), *GetName(), Reason, LoopTime);
}

void UknightGameplayAbility_MonsterChargeAttack::ApplyHits()
{
	AActor* const Avatar = GetAvatarActorFromActorInfo();
	UWorld* const World = GetWorld();
	if (!Avatar || !World)
	{
		return;
	}

	// 돌진은 머리가 먼저 닿는다. 머리 본을 따라가야 정면의 플레이어가 메시에 밀려나기 전에 맞는다.
	const USkeletalMeshComponent* const Mesh = GetCurrentActorInfo() ? GetCurrentActorInfo()->SkeletalMeshComponent.Get() : nullptr;
	const FVector Anchor = Mesh && !HitSocket.IsNone() && Mesh->DoesSocketExist(HitSocket) ? Mesh->GetSocketLocation(HitSocket) : Avatar->GetActorLocation();
	const FVector Center = Anchor + Avatar->GetActorQuat().RotateVector(HitOffset);
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(knightMonsterCharge), false, Avatar);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(HitRadius), QueryParams);

	int32 NewHits = 0;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* const Target = Overlap.GetActor();
		UPrimitiveComponent* const Component = Overlap.GetComponent();
		if (!Target || !Component || HitActors.Contains(Target)
			|| !Target->GetClass()->ImplementsInterface(UCombatInterface::StaticClass())
			|| !ICombatInterface::Execute_CanReceiveHitBP(Target, Avatar)
			|| FGenericTeamId::GetAttitude(Avatar, Target) != ETeamAttitude::Hostile)
		{
			continue;
		}
		HitActors.Add(Target);
		++NewHits;

		FVector ImpactPoint = Component->GetComponentLocation();
		FVector ClosestPoint;
		if (Component->GetClosestPointOnCollision(Center, ClosestPoint) >= 0.f)
		{
			ImpactPoint = ClosestPoint;
		}
		FHitResult Hit(Target, Component, ImpactPoint, (Center - ImpactPoint).GetSafeNormal());
		Hit.bBlockingHit = true;
		Hit.TraceStart = Center;
		Hit.TraceEnd = ImpactPoint;
		const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(Avatar);
		const UknightMonsterTargetingComponent* const Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
		bHitTarget |= Targeting && Targeting->GetCurrentTarget() == Target;
		const EknightHitOutcome Outcome = UknightCombatLibrary::ApplyMeleeHit(Avatar, Hit, HitParams);
		UE_LOG(LogknightAI, Verbose, TEXT("%s: %s hit %s (outcome %d)."), *GetNameSafe(Avatar), *GetName(), *GetNameSafe(Target), static_cast<int32>(Outcome));
		if (Outcome == EknightHitOutcome::Hit || Outcome == EknightHitOutcome::Guarded)
		{
			Knockback(*Avatar, *Target, Outcome == EknightHitOutcome::Guarded ? GuardKnockbackScale : 1.f);
		}
	}

#if ENABLE_DRAW_DEBUG
	if (knightMonsterCharge::ShouldDraw())
	{
		DrawDebugSphere(World, Center, HitRadius, 16, NewHits == 0 ? FColor::Red : FColor::Magenta, false, knightMonsterCharge::TickInterval, 0, 3.f);
	}
#endif
}

void UknightGameplayAbility_MonsterChargeAttack::Knockback(const AActor& Avatar, AActor& Target, float Scale)
{
	if (KnockbackSpeed <= 0.f || KnockbackTime <= 0.f)
	{
		return;
	}
	// 주로 돌진 경로 밖(타깃이 있던 쪽)으로 날려 보낸다. 몬스터가 계속 달려오므로 앞쪽 성분만으로는 다시 몸에 걸린다.
	const FVector Forward = Avatar.GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = Avatar.GetActorRightVector().GetSafeNormal2D();
	const FVector Side = FVector::DotProduct(Target.GetActorLocation() - Avatar.GetActorLocation(), Right) >= 0.f ? Right : -Right;
	KnockbackActor = &Target;
	KnockbackSide = Side;
	bKnockbackFlipped = false;
	KnockbackVelocity = (Side * 2.f + Forward).GetSafeNormal() * KnockbackSpeed * Scale;
	KnockbackRemaining = KnockbackTime;
	UE_LOG(LogknightAI, Verbose, TEXT("%s: knockback %s at %.0f cm/s for %.2fs."), *GetName(), *GetNameSafe(&Target), KnockbackSpeed * Scale, KnockbackTime);
}

void UknightGameplayAbility_MonsterChargeAttack::TickKnockback(float DeltaTime)
{
	AActor* const Target = KnockbackActor.Get();
	if (!Target || KnockbackRemaining <= 0.f)
	{
		KnockbackRemaining = 0.f;
		return;
	}
	// 플레이어 피격 몽타주의 루트 모션이 캐릭터 무브먼트의 속도(LaunchCharacter, 루트 모션 소스)보다 우선하므로
	// 위치를 직접 옮긴다. 스윕해서 벽은 뚫지 않는다.
	const float Step = FMath::Min(DeltaTime, KnockbackRemaining);
	const FVector Delta = KnockbackVelocity * Step;
	FHitResult Blocked;
	// 캐릭터는 무브먼트로 옮겨 경사진 지형은 타고 넘고, 벽에서만 막히게 한다.
	ACharacter* const Character = Cast<ACharacter>(Target);
	UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (Movement && Movement->UpdatedComponent)
	{
		Movement->SafeMoveUpdatedComponent(Delta, Movement->UpdatedComponent->GetComponentQuat(), true, Blocked);
		if (Blocked.IsValidBlockingHit() && Movement->IsWalkable(Blocked))
		{
			// 오르막 지형이면 남은 이동을 경사면에 붙여 다시 옮긴다. 벽(걸을 수 없는 면)만 막힌 것으로 본다.
			const FVector Rest = FVector::VectorPlaneProject(Delta * (1.f - Blocked.Time), Blocked.Normal);
			Movement->SafeMoveUpdatedComponent(Rest, Movement->UpdatedComponent->GetComponentQuat(), true, Blocked);
			Blocked.bBlockingHit = Blocked.bBlockingHit && !Movement->IsWalkable(Blocked);
		}
	}
	else
	{
		Target->AddActorWorldOffset(Delta, true, &Blocked);
	}
	// 한쪽이 벽에 막혔으면 반대쪽으로 한 번 바꿔 날린다. 몸 앞에 붙은 채 밀려가지 않게 한다.
	if (Blocked.bBlockingHit && !bKnockbackFlipped)
	{
		bKnockbackFlipped = true;
		KnockbackVelocity -= 2.f * FVector::DotProduct(KnockbackVelocity, KnockbackSide) * KnockbackSide;
		UE_LOG(LogknightAI, Verbose, TEXT("%s: knockback blocked by %s, flipped side."), *GetName(), *GetNameSafe(Blocked.GetActor()));
	}
	KnockbackRemaining -= Step;
}

void UknightGameplayAbility_MonsterChargeAttack::HandleMontageEnded()
{
	if (bMontageEnded)
	{
		return;
	}
	bMontageEnded = true;
	UAbilityTask_WaitDelay* const RecoveryTask = UAbilityTask_WaitDelay::WaitDelay(this, RecoveryTime);
	RecoveryTask->OnFinish.AddDynamic(this, &ThisClass::HandleRecoveryFinished);
	RecoveryTask->ReadyForActivation();
}

void UknightGameplayAbility_MonsterChargeAttack::HandleMontageInterrupted()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

void UknightGameplayAbility_MonsterChargeAttack::HandleRecoveryFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}
