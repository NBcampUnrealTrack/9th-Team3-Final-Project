#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterTornadoAttack.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Monster/knightMonsterAirTurnComponent.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterHoverComponent.h"
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
	bEnraged = ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Enraged);
	SpawnCount = FMath::Max(1, bEnraged ? EnragedTornadoCount : TornadoCount);
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
			if (!AirTurn || !AirTurn->IsAirborne())
			{
				HandleGetUpFinished();
			}
			else if (TiredTime > 0.f)
			{
				Phase = EPhase::Tired;
				PhaseTime = 0.f;
				if (UknightMonsterHoverComponent* const Hover = Monster->FindComponentByClass<UknightMonsterHoverComponent>())
				{
					Hover->SetAirAlpha(TiredAirAlpha, TiredTime);
				}
				UE_LOG(LogknightAI, Log, TEXT("%s: tornado summoned, tiring for %.1f s."), *GetNameSafe(Monster), TiredTime);
			}
			else
			{
				StartLand();
			}
		}
		break;
	}
	case EPhase::Tired:
		// 처음엔 천천히, 떨어지기 직전에 크게 느려진다.
		SetAnimRate(FMath::Lerp(1.f, TiredAnimRate, FMath::Square(FMath::Clamp(PhaseTime / TiredTime, 0.f, 1.f))));
		if (PhaseTime >= TiredTime)
		{
			StartLand();
		}
		break;
	case EPhase::Land:
		if (PhaseTime >= LandHoldTime)
		{
			Phase = EPhase::GetUp;
			SetGettingUp(true);
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

	// 몸 아래 바닥(캡슐 바닥)에서 생긴다. 여러 개면 가운데를 기준으로 좌우로 벌린다. 연사는 모두 가운데에서 나간다.
	const bool bVolley = bEnraged && bEnragedVolley;
	const float Side = bVolley ? 0.f : Index - (Count - 1) * 0.5f;
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
		FknightMonsterTornadoParams Params = Tornado;
		if (bEnraged && Index == Count - 1 && !FMath::IsNearlyEqual(EnragedLastScale, 1.f))
		{
			const float Scale = EnragedLastScale;
			Params.Radius *= Scale;
			Params.Height *= Scale;
			Params.EffectScale *= Scale;
			Params.BurstRippleRadius *= Scale;
			Params.WaterRippleRadius *= Scale;
			Params.WaterSplashScale *= Scale;
		}
		Spawn->Launch(Monster, Target, HitParams, Params, Direction);
	}
	UE_LOG(LogknightAI, Log, TEXT("%s: tornado %d/%d spawned."), *GetNameSafe(Monster), Index + 1, Count);
}

void UknightGameplayAbility_MonsterTornadoAttack::SetAnimRate(float Rate)
{
	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	if (USkeletalMeshComponent* const Mesh = Monster ? Monster->GetMesh() : nullptr)
	{
		Mesh->GlobalAnimRateScale = Rate;
	}
}

void UknightGameplayAbility_MonsterTornadoAttack::StartLand()
{
	SetAnimRate(1.f);
	AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	if (UknightMonsterAirTurnComponent* const AirTurn = Monster ? Monster->FindComponentByClass<UknightMonsterAirTurnComponent>() : nullptr)
	{
		AirTurn->EndAirTurnNow(LandDescendTime);
	}
	// 지침 중 내려온 높이에서 바닥까지 LandDescendTime이 걸리게 한다(EndAirTurnNow는 공중 높이 전체 기준).
	if (UknightMonsterHoverComponent* const Hover = Monster ? Monster->FindComponentByClass<UknightMonsterHoverComponent>() : nullptr)
	{
		Hover->SetAirAlpha(0.f, LandDescendTime);
	}
	bLanded = true;
	if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AddLooseGameplayTag(knightGameplayTags::State_Monster_Collapsed);
	}
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

void UknightGameplayAbility_MonsterTornadoAttack::SetGettingUp(bool bOn)
{
	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || bOn == bGettingUpApplied)
	{
		return;
	}
	if (bOn)
	{
		ASC->AddLooseGameplayTag(knightGameplayTags::State_Monster_GettingUp);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(knightGameplayTags::State_Monster_GettingUp);
	}
	bGettingUpApplied = bOn;
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
	SetAnimRate(1.f);

	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	UknightMonsterAirTurnComponent* const AirTurn = Monster ? Monster->FindComponentByClass<UknightMonsterAirTurnComponent>() : nullptr;
	if (AirTurn && bLanded)
	{
		// 탈진 착지(딜 타임)가 끝난 뒤부터 지상 턴을 센다.
		AirTurn->RestartGroundTurn();
	}
	else if (AirTurn && AirTurn->IsAirborne() && Phase == EPhase::Tired)
	{
		// 지치다가 끊겼는데 아직 공중이면 내려온 높이를 공중 높이로 되돌린다.
		if (UknightMonsterHoverComponent* const Hover = Monster->FindComponentByClass<UknightMonsterHoverComponent>())
		{
			Hover->SetAirborne(true, 1.f);
		}
	}
	if (UAbilitySystemComponent* const ASC = bLanded ? GetAbilitySystemComponentFromActorInfo() : nullptr)
	{
		ASC->RemoveLooseGameplayTag(knightGameplayTags::State_Monster_Collapsed);
	}
	SetGettingUp(false);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
