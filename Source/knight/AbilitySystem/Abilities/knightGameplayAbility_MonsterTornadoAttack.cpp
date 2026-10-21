#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterTornadoAttack.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Monster/knightMonsterAirTurnComponent.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterMovementComponent.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

namespace knightMonsterTornadoAttack
{
	constexpr float TickInterval = 1.f / 30.f;
	constexpr int32 MaxTicks = 100000;
}

UknightGameplayAbility_MonsterTornadoAttack::UknightGameplayAbility_MonsterTornadoAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Selection.MinRange = 0.f;
	Selection.MaxRange = 3000.f;
	Selection.MaxAngle = 180.f;
	HitParams.bCanBeParried = false;
}

void UknightGameplayAbility_MonsterTornadoAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	SpawnCount = FMath::Max(1, ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Enraged) ? EnragedTornadoCount : TornadoCount);
	Spawned = 0;
	Phase = EPhase::Summon;
	PhaseTime = 0.f;
	bLanded = false;
	if (SummonMontage)
	{
		UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, SummonMontage);
		MontageTask->ReadyForActivation();
	}
	UAbilityTask_Repeat* const Task = UAbilityTask_Repeat::RepeatAction(this, knightMonsterTornadoAttack::TickInterval, knightMonsterTornadoAttack::MaxTicks);
	Task->OnPerformAction.AddDynamic(this, &ThisClass::HandleTick);
	TickTask = Task;
	Task->ReadyForActivation();
}

void UknightGameplayAbility_MonsterTornadoAttack::HandleTick(int32 ActionNumber)
{
	PhaseTime += knightMonsterTornadoAttack::TickInterval;
	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	if (!Monster)
	{
		return;
	}
	switch (Phase)
	{
	case EPhase::Summon:
	{
		const UknightMonsterTargetingComponent* const Targeting = Monster->GetTargetingComponent();
		const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
		UknightMonsterMovementComponent* const Movement = Monster->GetMonsterMovementComponent();
		if (Movement && Target)
		{
			Movement->TurnTowards(Target->GetActorLocation(), knightMonsterTornadoAttack::TickInterval, TurnRate, 2.f);
		}
		while (Spawned < SpawnCount && PhaseTime >= SpawnTime + Spawned * SpawnInterval)
		{
			SpawnTornado(Spawned, SpawnCount);
			++Spawned;
		}
		if (Spawned >= SpawnCount && PhaseTime >= ExhaustTime)
		{
			const UknightMonsterAirTurnComponent* const AirTurn = Monster->FindComponentByClass<UknightMonsterAirTurnComponent>();
			if (AirTurn && AirTurn->IsAirborne())
			{
				StartLand();
			}
			else
			{
				HandleGetUpFinished();
			}
		}
		break;
	}
	case EPhase::Land:
		if (PhaseTime >= LandHoldTime)
		{
			Phase = EPhase::GetUp;
			if (!GetUpMontage)
			{
				HandleGetUpFinished();
				return;
			}
			UAbilityTask_PlayMontageAndWait* const GetUpTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, GetUpMontage);
			GetUpTask->OnCompleted.AddDynamic(this, &ThisClass::HandleGetUpFinished);
			GetUpTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleGetUpFinished);
			GetUpTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleGetUpFinished);
			GetUpTask->OnCancelled.AddDynamic(this, &ThisClass::HandleGetUpFinished);
			GetUpTask->ReadyForActivation();
		}
		break;
	case EPhase::GetUp:
		break;
	}
}

void UknightGameplayAbility_MonsterTornadoAttack::SpawnTornado(int32 Index, int32 Count)
{
	AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	UWorld* const World = GetWorld();
	if (!Monster || !World)
	{
		return;
	}
	const UknightMonsterTargetingComponent* const Targeting = Monster->GetTargetingComponent();
	AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;

	// 몸 아래 바닥(캡슐 바닥)에서 생긴다. 여러 개면 가운데를 기준으로 좌우로 벌린다.
	const float Side = Index - (Count - 1) * 0.5f;
	const UCapsuleComponent* const Capsule = Monster->GetCapsuleComponent();
	const FVector Ground = Monster->GetActorLocation() - FVector(0.f, 0.f, Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.f);
	const FVector Location = Ground + Monster->GetActorRightVector().GetSafeNormal2D() * Side * SpawnSpread;
	const FVector ToTarget = Target ? (Target->GetActorLocation() - Location).GetSafeNormal2D() : FVector::ZeroVector;
	const FVector Forward = ToTarget.IsNearlyZero() ? Monster->GetActorForwardVector() : ToTarget;
	const FVector Direction = Forward.RotateAngleAxis(Side * SpreadAngle, FVector::UpVector);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Monster;
	SpawnParams.Instigator = Monster;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AknightMonsterTornado* const Spawn = World->SpawnActor<AknightMonsterTornado>(Location, Direction.Rotation(), SpawnParams))
	{
		Spawn->Launch(Monster, Target, HitParams, Tornado, Direction);
	}
	UE_LOG(LogknightAI, Log, TEXT("%s: tornado %d/%d spawned."), *GetNameSafe(Monster), Index + 1, Count);
}

void UknightGameplayAbility_MonsterTornadoAttack::StartLand()
{
	AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	if (UknightMonsterAirTurnComponent* const AirTurn = Monster ? Monster->FindComponentByClass<UknightMonsterAirTurnComponent>() : nullptr)
	{
		AirTurn->EndAirTurnNow(0.5f);
	}
	bLanded = true;
	Phase = EPhase::Land;
	PhaseTime = 0.f;
	if (!LandMontage)
	{
		HandleGetUpFinished();
		return;
	}
	UAbilityTask_PlayMontageAndWait* const LandTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, LandMontage);
	LandTask->ReadyForActivation();
	UE_LOG(LogknightAI, Log, TEXT("%s: tornado summoned, exhausted landing."), *GetNameSafe(Monster));
}

void UknightGameplayAbility_MonsterTornadoAttack::HandleGetUpFinished()
{
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UknightGameplayAbility_MonsterTornadoAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (TickTask.IsValid())
	{
		TickTask->EndTask();
	}
	TickTask.Reset();

	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	if (UknightMonsterAirTurnComponent* const AirTurn = Monster && bLanded ? Monster->FindComponentByClass<UknightMonsterAirTurnComponent>() : nullptr)
	{
		// 탈진 착지(딜 타임)가 끝난 뒤부터 지상 턴을 센다.
		AirTurn->RestartGroundTurn();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
