#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAreaAttack.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Combat/knightCombatLibrary.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GenericTeamAgentInterface.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/CombatInterface.h"
#include "System/knightGameplayTags.h"
#include "knight.h"
#include "knightPerfCounters.h"

namespace knightMonsterAttackCVars
{
	static int32 DrawAttacks = 0;
	static FAutoConsoleVariableRef CVarDrawAttacks(
		TEXT("knight.Monster.DrawAttacks"), DrawAttacks,
		TEXT("Draw monster area attacks: telegraph (orange) and impact (red). 0: off, 1: on"), ECVF_Cheat);
}

UknightGameplayAbility_MonsterAreaAttack::UknightGameplayAbility_MonsterAreaAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Activated by the monster's StateTree, never by input.
	ActivationGroup = EknightAbilityActivationGroup::Exclusive_Replaceable;
	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_Attack));
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_HitReact);
}

void UknightGameplayAbility_MonsterAreaAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector Center = GetAreaCenter();
	if (TelegraphCue.IsValid())
	{
		if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
		{
			FGameplayCueParameters CueParams;
			CueParams.Location = Center;
			CueParams.RawMagnitude = AreaRadius;
			CueParams.Instigator = GetAvatarActorFromActorInfo();
			ASC->ExecuteGameplayCue(TelegraphCue, CueParams);
		}
	}
#if ENABLE_DRAW_DEBUG
	if (knightMonsterAttackCVars::DrawAttacks != 0)
	{
		DrawDebugSphere(GetWorld(), Center, AreaRadius, 16, FColor::Orange, false, WindupTime);
	}
#endif

	if (Montage)
	{
		UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage);
		MontageTask->ReadyForActivation();
	}

	UAbilityTask_WaitDelay* const WindupTask = UAbilityTask_WaitDelay::WaitDelay(this, WindupTime);
	WindupTask->OnFinish.AddDynamic(this, &ThisClass::HandleWindupFinished);
	WindupTask->ReadyForActivation();
}

void UknightGameplayAbility_MonsterAreaAttack::HandleWindupFinished()
{
	OnImpact();

	UAbilityTask_WaitDelay* const RecoveryTask = UAbilityTask_WaitDelay::WaitDelay(this, RecoveryTime);
	RecoveryTask->OnFinish.AddDynamic(this, &ThisClass::HandleRecoveryFinished);
	RecoveryTask->ReadyForActivation();
}

void UknightGameplayAbility_MonsterAreaAttack::HandleRecoveryFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UknightGameplayAbility_MonsterAreaAttack::OnImpact()
{
	KNIGHT_PERF_SCOPE("Monster.AreaAttackImpact");

	AActor* const Avatar = GetAvatarActorFromActorInfo();
	UWorld* const World = GetWorld();
	if (!Avatar || !World)
	{
		return;
	}

	const FVector Center = GetAreaCenter();
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(knightMonsterAreaAttack), false, Avatar);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(AreaRadius), QueryParams);

	TSet<AActor*> HitActors;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* const Target = Overlap.GetActor();
		UPrimitiveComponent* const Component = Overlap.GetComponent();
		if (!Target || !Component || HitActors.Contains(Target))
		{
			continue;
		}
		const ICombatInterface* const Combat = Cast<ICombatInterface>(Target);
		if (!Combat || !Combat->CanReceiveHit(Avatar) || FGenericTeamId::GetAttitude(Avatar, Target) != ETeamAttitude::Hostile)
		{
			continue;
		}
		HitActors.Add(Target);

		// Damage context, cues and part lookups expect a hit result; build one at the closest surface point.
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
		UknightCombatLibrary::ApplyMeleeHit(Avatar, Hit, HitParams);
	}

#if ENABLE_DRAW_DEBUG
	if (knightMonsterAttackCVars::DrawAttacks != 0)
	{
		DrawDebugSphere(World, Center, AreaRadius, 16, HitActors.IsEmpty() ? FColor::Red : FColor::Magenta, false, 0.5f, 0, 3.f);
	}
#endif
	UE_LOG(LogknightAI, Verbose, TEXT("%s: %s hit %d target(s)."), *GetNameSafe(Avatar), *GetName(), HitActors.Num());
}

FVector UknightGameplayAbility_MonsterAreaAttack::GetAreaCenter() const
{
	const AActor* const Avatar = GetAvatarActorFromActorInfo();
	return Avatar ? Avatar->GetActorTransform().TransformPosition(AreaOffset) : FVector::ZeroVector;
}
