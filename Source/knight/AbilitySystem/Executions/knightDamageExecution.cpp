#include "AbilitySystem/Executions/knightDamageExecution.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Interfaces/CombatInterface.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

namespace knightDamageStatics
{
	struct FCaptureDefs
	{
		FGameplayEffectAttributeCaptureDefinition AttackPower;
		FGameplayEffectAttributeCaptureDefinition Defense;
		FGameplayEffectAttributeCaptureDefinition CriticalChance;
		FGameplayEffectAttributeCaptureDefinition CriticalDamageMultiplier;

		FCaptureDefs()
		{
			// Snapshot attacker stats when the spec is made, so a projectile keeps the stats it was fired with.
			AttackPower = FGameplayEffectAttributeCaptureDefinition(UknightCoreAttributeSet::GetAttackPowerAttribute(), EGameplayEffectAttributeCaptureSource::Source, true);
			Defense = FGameplayEffectAttributeCaptureDefinition(UknightCoreAttributeSet::GetDefenseAttribute(), EGameplayEffectAttributeCaptureSource::Target, false);
			CriticalChance = FGameplayEffectAttributeCaptureDefinition(UknightCoreAttributeSet::GetCriticalChanceAttribute(), EGameplayEffectAttributeCaptureSource::Source, true);
			CriticalDamageMultiplier = FGameplayEffectAttributeCaptureDefinition(UknightCoreAttributeSet::GetCriticalDamageMultiplierAttribute(), EGameplayEffectAttributeCaptureSource::Source, true);
		}
	};

	static const FCaptureDefs& Get()
	{
		static FCaptureDefs Defs;
		return Defs;
	}
}

UknightDamageExecution::UknightDamageExecution()
{
	const knightDamageStatics::FCaptureDefs& Defs = knightDamageStatics::Get();
	RelevantAttributesToCapture.Add(Defs.AttackPower);
	RelevantAttributesToCapture.Add(Defs.Defense);
	RelevantAttributesToCapture.Add(Defs.CriticalChance);
	RelevantAttributesToCapture.Add(Defs.CriticalDamageMultiplier);
}

void UknightDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const knightDamageStatics::FCaptureDefs& Defs = knightDamageStatics::Get();
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	const FGameplayEffectContextHandle& Context = Spec.GetContext();

	FAggregatorEvaluateParameters EvaluateParams;
	EvaluateParams.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvaluateParams.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	const FGameplayTagContainer* const TargetTags = EvaluateParams.TargetTags;
	if (TargetTags && (TargetTags->HasTag(knightGameplayTags::State_Invulnerable) || TargetTags->HasTag(knightGameplayTags::State_Parrying)))
	{
		return;
	}

	float AttackPower = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Defs.AttackPower, EvaluateParams, AttackPower);
	float Defense = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Defs.Defense, EvaluateParams, Defense);
	float CriticalChance = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Defs.CriticalChance, EvaluateParams, CriticalChance);
	float CriticalDamageMultiplier = 1.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Defs.CriticalDamageMultiplier, EvaluateParams, CriticalDamageMultiplier);

	const float BaseDamage = Spec.GetSetByCallerMagnitude(knightGameplayTags::SetByCaller_Damage, false, 0.f);
	const float MotionValue = Spec.GetSetByCallerMagnitude(knightGameplayTags::SetByCaller_MotionValue, false, 0.f);
	const float BasePoiseDamage = Spec.GetSetByCallerMagnitude(knightGameplayTags::SetByCaller_PoiseDamage, false, 0.f);

	const UAbilitySystemComponent* const TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	const AActor* const TargetActor = TargetASC ? TargetASC->GetAvatarActor_Direct() : nullptr;
	const AActor* const AttackerActor = Context.GetInstigator();

	float PartMultiplier = 1.f;
	if (const FHitResult* const Hit = Context.GetHitResult())
	{
		if (TargetActor && TargetActor->GetClass()->ImplementsInterface(UCombatInterface::StaticClass()))
		{
			AActor* const MutableTarget = const_cast<AActor*>(TargetActor);
			PartMultiplier = FMath::Max(0.f, ICombatInterface::Execute_GetPartDamageMultiplierBP(
				MutableTarget, ICombatInterface::Execute_ResolveHitPartBP(MutableTarget, *Hit)));
		}
	}

	const bool bCritical = CriticalChance > 0.f && FMath::FRand() < CriticalChance;
	const float CriticalMultiplier = bCritical ? FMath::Max(1.f, CriticalDamageMultiplier) : 1.f;
	const float DefenseMultiplier = 100.f / (100.f + FMath::Max(0.f, Defense));

	float GuardMultiplier = 1.f;
	float GuardPoiseMultiplier = 1.f;
	if (TargetTags && TargetTags->HasTag(knightGameplayTags::State_Guarding) && IsAttackerInGuardArc(TargetActor, AttackerActor))
	{
		GuardMultiplier = GuardDamageMultiplier;
		GuardPoiseMultiplier = GuardPoiseDamageMultiplier;
	}

	const float RawDamage = BaseDamage + AttackPower * MotionValue / 100.f;
	const float FinalDamage = FMath::Max(0.f, RawDamage * PartMultiplier * CriticalMultiplier * DefenseMultiplier * GuardMultiplier);
	const float FinalPoiseDamage = FMath::Max(0.f, BasePoiseDamage * GuardPoiseMultiplier);

	UE_LOG(LogknightCombat, Verbose, TEXT("Damage %s -> %s: raw %.1f part x%.2f crit x%.2f def x%.2f guard x%.2f = %.1f, poise %.1f"),
		*GetNameSafe(AttackerActor), *GetNameSafe(TargetActor), RawDamage, PartMultiplier, CriticalMultiplier,
		DefenseMultiplier, GuardMultiplier, FinalDamage, FinalPoiseDamage);

	if (FinalDamage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UknightCoreAttributeSet::GetDamageAttribute(), EGameplayModOp::AddBase, FinalDamage));
	}
	if (FinalPoiseDamage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UknightCoreAttributeSet::GetPoiseDamageAttribute(), EGameplayModOp::AddBase, FinalPoiseDamage));
	}
}

bool UknightDamageExecution::IsAttackerInGuardArc(const AActor* Target, const AActor* Attacker) const
{
	if (!Target || !Attacker)
	{
		return false;
	}
	const FVector ToAttacker = (Attacker->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
	if (ToAttacker.IsNearlyZero())
	{
		return true;
	}
	const float CosHalfArc = FMath::Cos(FMath::DegreesToRadians(GuardArcDegrees * 0.5f));
	return FVector::DotProduct(Target->GetActorForwardVector().GetSafeNormal2D(), ToAttacker) >= CosHalfArc;
}
