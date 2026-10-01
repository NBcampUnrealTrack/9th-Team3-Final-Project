#include "Combat/knightCombatLibrary.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/knightHitStopComponent.h"
#include "Combat/knightTargetLockComponent.h"
#include "GameplayEffect.h"
#include "Interfaces/CombatInterface.h"
#include "MotionWarpingComponent.h"
#include "Perception/AISense_Hearing.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

EknightHitOutcome UknightCombatLibrary::ApplyMeleeHit(AActor* Attacker, const FHitResult& Hit, const FknightMeleeHitParams& Params)
{
	AActor* const Target = Hit.GetActor();
	UAbilitySystemComponent* const SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Attacker);
	UAbilitySystemComponent* const TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	const ICombatInterface* const TargetCombat = Cast<ICombatInterface>(Target);
	if (!SourceASC || !TargetASC || !TargetCombat || !TargetCombat->CanReceiveHit(Attacker))
	{
		return EknightHitOutcome::Ignored;
	}

	FGameplayCueParameters CueParams;
	CueParams.Location = Hit.ImpactPoint;
	CueParams.Normal = Hit.ImpactNormal;
	CueParams.Instigator = Attacker;
	CueParams.EffectCauser = Attacker;
	CueParams.PhysicalMaterial = Hit.PhysMaterial;

	if (TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Invulnerable))
	{
		return EknightHitOutcome::Evaded;
	}

	if (TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Parrying))
	{
		FGameplayEventData Payload;
		Payload.EventTag = knightGameplayTags::Event_Parried;
		Payload.Instigator = Target;
		Payload.Target = Attacker;
		SourceASC->HandleGameplayEvent(Payload.EventTag, &Payload);
		TargetASC->ExecuteGameplayCue(knightGameplayTags::GameplayCue_Parry, CueParams);
		ApplyHitStopPair(Attacker, Target, Params.ParryHitStopDuration);
		UE_LOG(LogknightCombat, Log, TEXT("%s parried %s."), *GetNameSafe(Target), *GetNameSafe(Attacker));
		return EknightHitOutcome::Parried;
	}

	if (!Params.DamageEffect)
	{
		UE_LOG(LogknightCombat, Error, TEXT("%s hit %s with no DamageEffect set."), *GetNameSafe(Attacker), *GetNameSafe(Target));
		return EknightHitOutcome::Ignored;
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddHitResult(Hit, true);
	const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(Params.DamageEffect, 1.f, Context);
	if (!SpecHandle.IsValid())
	{
		return EknightHitOutcome::Ignored;
	}
	SpecHandle.Data->SetSetByCallerMagnitude(knightGameplayTags::SetByCaller_Damage, Params.BaseDamage);
	SpecHandle.Data->SetSetByCallerMagnitude(knightGameplayTags::SetByCaller_MotionValue, Params.MotionValue);
	SpecHandle.Data->SetSetByCallerMagnitude(knightGameplayTags::SetByCaller_PoiseDamage, Params.PoiseDamage);

	// Read before applying: the hit may kill the target and change its tags.
	const bool bGuarding = TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Guarding);
	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data, TargetASC);

	if (bGuarding)
	{
		// The guard ability decides whether the hit came from inside its arc and how to react.
		FGameplayEventData Payload;
		Payload.EventTag = knightGameplayTags::Event_Guarded;
		Payload.Instigator = Attacker;
		Payload.Target = Target;
		Payload.ContextHandle = Context;
		TargetASC->HandleGameplayEvent(Payload.EventTag, &Payload);
		TargetASC->ExecuteGameplayCue(knightGameplayTags::GameplayCue_Guard, CueParams);
		ApplyHitStopPair(Attacker, Target, Params.HitStopDuration * 0.5f);
		return EknightHitOutcome::Guarded;
	}

	const float PartMultiplier = TargetCombat->GetPartDamageMultiplier(TargetCombat->ResolveHitPart(Hit));
	const FGameplayTag CueTag = PartMultiplier >= Params.WeakpointMultiplierThreshold
		? knightGameplayTags::GameplayCue_Hit_Weakpoint.GetTag()
		: knightGameplayTags::GameplayCue_Hit_Melee.GetTag();
	CueParams.RawMagnitude = PartMultiplier;
	TargetASC->ExecuteGameplayCue(CueTag, CueParams);
	ApplyHitStopPair(Attacker, Target, Params.HitStopDuration);
	// Combat is audible: a monster that has not noticed the fight yet can hear it.
	UAISense_Hearing::ReportNoiseEvent(Attacker, Hit.ImpactPoint, 1.f, Attacker, 0.f, TEXT("Hit"));
	return EknightHitOutcome::Hit;
}

bool UknightCombatLibrary::SetWarpTargetToLockedTarget(AActor* Attacker, FName WarpTargetName, float MaxDistance)
{
	UMotionWarpingComponent* const MotionWarping = Attacker ? Attacker->FindComponentByClass<UMotionWarpingComponent>() : nullptr;
	const UknightTargetLockComponent* const TargetLock = Attacker ? Attacker->FindComponentByClass<UknightTargetLockComponent>() : nullptr;
	if (!MotionWarping)
	{
		return false;
	}
	FVector TargetLocation;
	if (!TargetLock || !TargetLock->GetLockedTargetLocation(TargetLocation))
	{
		MotionWarping->RemoveWarpTarget(WarpTargetName);
		return false;
	}

	const FVector AttackerLocation = Attacker->GetActorLocation();
	FVector ToTarget = TargetLocation - AttackerLocation;
	ToTarget.Z = 0.f;
	const float Distance = ToTarget.Size();
	if (Distance < KINDA_SMALL_NUMBER)
	{
		MotionWarping->RemoveWarpTarget(WarpTargetName);
		return false;
	}
	const FVector Direction = ToTarget / Distance;
	const FVector WarpLocation = AttackerLocation + Direction * FMath::Min(Distance, MaxDistance);
	MotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation(WarpTargetName, WarpLocation, Direction.Rotation());
	return true;
}

void UknightCombatLibrary::ApplyHitStopPair(AActor* Attacker, AActor* Target, float Duration)
{
	if (Duration <= 0.f)
	{
		return;
	}
	for (AActor* const Actor : { Attacker, Target })
	{
		if (UknightHitStopComponent* const HitStop = Actor ? Actor->FindComponentByClass<UknightHitStopComponent>() : nullptr)
		{
			HitStop->ApplyHitStop(Duration);
		}
	}
}
