#include "AbilitySystem/Executions/knightDamageExecution.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Interfaces/CombatInterface.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

// ---- 순수 계산 (테스트 대상) ----

namespace knightDamageMath
{
	float GetGuardedMultiplier(float BaseGuardMultiplier, float GuardEfficiency)
	{
		// 기준 배율로 줄이던 양(1 - 기준)에 효율을 곱한다. 효율이 높을수록 더 많이 막는다.
		const float Reduction = (1.f - FMath::Clamp(BaseGuardMultiplier, 0.f, 1.f)) * FMath::Max(0.f, GuardEfficiency);
		return FMath::Clamp(1.f - Reduction, 0.f, 1.f);
	}

	float GetDefenseMultiplier(float Defense)
	{
		return 100.f / (100.f + FMath::Max(0.f, Defense));
	}

	FResult Compute(const FInputs& In)
	{
		FResult Out;
		Out.RawDamage = In.BaseDamage + In.AttackPower * In.MotionValue / 100.f;
		Out.CriticalMultiplier = In.bCritical ? FMath::Max(1.f, In.CriticalDamageMultiplier) : 1.f;
		Out.DefenseMultiplier = GetDefenseMultiplier(In.Defense);
		if (In.bGuarded)
		{
			Out.GuardMultiplier = GetGuardedMultiplier(In.GuardDamageMultiplier, In.GuardEfficiency);
			Out.GuardPoiseMultiplier = GetGuardedMultiplier(In.GuardPoiseDamageMultiplier, In.GuardEfficiency);
		}

		Out.Damage = FMath::Max(0.f, Out.RawDamage * FMath::Max(0.f, In.PartMultiplier) * Out.CriticalMultiplier
			* Out.DefenseMultiplier * Out.GuardMultiplier * FMath::Max(0.f, In.DamageTakenMultiplier));
		Out.PoiseDamage = FMath::Max(0.f, In.BasePoiseDamage * FMath::Max(0.f, In.StaggerPower) * Out.GuardPoiseMultiplier
			* FMath::Max(0.f, In.PoiseDamageTakenMultiplier));
		return Out;
	}
}

// ---- GAS 실행 ----

namespace knightDamageStatics
{
	struct FCaptureDefs
	{
		FGameplayEffectAttributeCaptureDefinition AttackPower;
		FGameplayEffectAttributeCaptureDefinition StaggerPower;
		FGameplayEffectAttributeCaptureDefinition Defense;
		FGameplayEffectAttributeCaptureDefinition GuardEfficiency;
		FGameplayEffectAttributeCaptureDefinition DamageTakenMultiplier;
		FGameplayEffectAttributeCaptureDefinition PoiseDamageTakenMultiplier;
		FGameplayEffectAttributeCaptureDefinition CriticalChance;
		FGameplayEffectAttributeCaptureDefinition CriticalDamageMultiplier;

		FCaptureDefs()
		{
			// 공격하는 쪽 스탯은 스펙을 만들 때 고정(스냅샷)한다. 그래서 투사체는 발사할 때의 스탯을 유지한다.
			AttackPower = FGameplayEffectAttributeCaptureDefinition(UknightCoreAttributeSet::GetAttackPowerAttribute(), EGameplayEffectAttributeCaptureSource::Source, true);
			StaggerPower = FGameplayEffectAttributeCaptureDefinition(UknightCoreAttributeSet::GetStaggerPowerAttribute(), EGameplayEffectAttributeCaptureSource::Source, true);
			CriticalChance = FGameplayEffectAttributeCaptureDefinition(UknightCoreAttributeSet::GetCriticalChanceAttribute(), EGameplayEffectAttributeCaptureSource::Source, true);
			CriticalDamageMultiplier = FGameplayEffectAttributeCaptureDefinition(UknightCoreAttributeSet::GetCriticalDamageMultiplierAttribute(), EGameplayEffectAttributeCaptureSource::Source, true);

			// 맞는 쪽 스탯은 맞는 순간의 값을 읽는다.
			Defense = FGameplayEffectAttributeCaptureDefinition(UknightCoreAttributeSet::GetDefenseAttribute(), EGameplayEffectAttributeCaptureSource::Target, false);
			GuardEfficiency = FGameplayEffectAttributeCaptureDefinition(UknightCoreAttributeSet::GetGuardEfficiencyAttribute(), EGameplayEffectAttributeCaptureSource::Target, false);
			DamageTakenMultiplier = FGameplayEffectAttributeCaptureDefinition(UknightCoreAttributeSet::GetDamageTakenMultiplierAttribute(), EGameplayEffectAttributeCaptureSource::Target, false);
			PoiseDamageTakenMultiplier = FGameplayEffectAttributeCaptureDefinition(UknightCoreAttributeSet::GetPoiseDamageTakenMultiplierAttribute(), EGameplayEffectAttributeCaptureSource::Target, false);
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
	RelevantAttributesToCapture.Add(Defs.StaggerPower);
	RelevantAttributesToCapture.Add(Defs.Defense);
	RelevantAttributesToCapture.Add(Defs.GuardEfficiency);
	RelevantAttributesToCapture.Add(Defs.DamageTakenMultiplier);
	RelevantAttributesToCapture.Add(Defs.PoiseDamageTakenMultiplier);
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
	// 퍼펙트 가드(State.Parrying)는 여기서 보지 않는다. ApplyMeleeHit이 이펙트 적용 전에 판정한다(헤더 주석 참고).
	if (TargetTags && TargetTags->HasTag(knightGameplayTags::State_Invulnerable))
	{
		return;
	}

	knightDamageMath::FInputs In;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Defs.AttackPower, EvaluateParams, In.AttackPower);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Defs.StaggerPower, EvaluateParams, In.StaggerPower);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Defs.Defense, EvaluateParams, In.Defense);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Defs.GuardEfficiency, EvaluateParams, In.GuardEfficiency);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Defs.DamageTakenMultiplier, EvaluateParams, In.DamageTakenMultiplier);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Defs.PoiseDamageTakenMultiplier, EvaluateParams, In.PoiseDamageTakenMultiplier);
	float CriticalChance = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Defs.CriticalChance, EvaluateParams, CriticalChance);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Defs.CriticalDamageMultiplier, EvaluateParams, In.CriticalDamageMultiplier);

	In.BaseDamage = Spec.GetSetByCallerMagnitude(knightGameplayTags::SetByCaller_Damage, false, 0.f);
	In.MotionValue = Spec.GetSetByCallerMagnitude(knightGameplayTags::SetByCaller_MotionValue, false, 0.f);
	In.BasePoiseDamage = Spec.GetSetByCallerMagnitude(knightGameplayTags::SetByCaller_PoiseDamage, false, 0.f);

	const UAbilitySystemComponent* const TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	const AActor* const TargetActor = TargetASC ? TargetASC->GetAvatarActor_Direct() : nullptr;
	const AActor* const AttackerActor = Context.GetInstigator();

	if (const FHitResult* const Hit = Context.GetHitResult())
	{
		if (TargetActor && TargetActor->GetClass()->ImplementsInterface(UCombatInterface::StaticClass()))
		{
			AActor* const MutableTarget = const_cast<AActor*>(TargetActor);
			In.PartMultiplier = ICombatInterface::Execute_GetPartDamageMultiplierBP(
				MutableTarget, ICombatInterface::Execute_ResolveHitPartBP(MutableTarget, *Hit));
		}
	}

	In.bCritical = CriticalChance > 0.f && FMath::FRand() < CriticalChance;
	In.bGuarded = TargetTags && TargetTags->HasTag(knightGameplayTags::State_Guarding) && IsAttackerInGuardArc(TargetActor, AttackerActor);
	In.GuardDamageMultiplier = GuardDamageMultiplier;
	In.GuardPoiseDamageMultiplier = GuardPoiseDamageMultiplier;

	const knightDamageMath::FResult Out = knightDamageMath::Compute(In);

	UE_LOG(LogknightCombat, Verbose, TEXT("Damage %s -> %s: raw %.1f part x%.2f crit x%.2f def x%.2f guard x%.2f (eff %.2f) taken x%.2f = %.1f, poise %.1f x stagger %.2f x guard %.2f x taken %.2f = %.1f"),
		*GetNameSafe(AttackerActor), *GetNameSafe(TargetActor), Out.RawDamage, In.PartMultiplier, Out.CriticalMultiplier,
		Out.DefenseMultiplier, Out.GuardMultiplier, In.GuardEfficiency, In.DamageTakenMultiplier, Out.Damage,
		In.BasePoiseDamage, In.StaggerPower, Out.GuardPoiseMultiplier, In.PoiseDamageTakenMultiplier, Out.PoiseDamage);

	if (Out.Damage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UknightCoreAttributeSet::GetDamageAttribute(), EGameplayModOp::AddBase, Out.Damage));
	}
	if (Out.PoiseDamage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UknightCoreAttributeSet::GetPoiseDamageAttribute(), EGameplayModOp::AddBase, Out.PoiseDamage));
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
