#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"

#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "Engine/World.h"

UknightCoreAttributeSet::UknightCoreAttributeSet()
{
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitPoise(100.f);
	InitMaxPoise(100.f);
	InitPoiseRecoveryRate(0.f);
	InitAttackPower(0.f);
	InitStaggerPower(1.f);
	InitDefense(0.f);
	InitGuardEfficiency(1.f);
	InitDamageTakenMultiplier(1.f);
	InitPoiseDamageTakenMultiplier(1.f);
	InitCriticalChance(0.f);
	InitCriticalDamageMultiplier(1.25f);
	InitDamage(0.f);
	InitPoiseDamage(0.f);
	InitHealing(0.f);
}

void UknightCoreAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UknightCoreAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UknightCoreAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetPoiseAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxPoise());
	}
	else if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxPoiseAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
	else if (Attribute == GetAttackPowerAttribute() || Attribute == GetDefenseAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
	// 그로기 회복 속도·경직치·가드 효율·받는 피해/그로기 배율은 음수만 막는다.
	else if (Attribute == GetPoiseRecoveryRateAttribute() || Attribute == GetStaggerPowerAttribute()
		|| Attribute == GetGuardEfficiencyAttribute() || Attribute == GetDamageTakenMultiplierAttribute()
		|| Attribute == GetPoiseDamageTakenMultiplierAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetCriticalChanceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 1.f);
	}
	else if (Attribute == GetCriticalDamageMultiplierAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
}

void UknightCoreAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetMaxHealthAttribute())
	{
		AdjustForMaxChange(GetOwningAbilitySystemComponent(), GetHealthAttribute(), OldValue, NewValue, false);
	}
	else if (Attribute == GetMaxPoiseAttribute())
	{
		AdjustForMaxChange(GetOwningAbilitySystemComponent(), GetPoiseAttribute(), OldValue, NewValue, false);
	}
	// 값이 다시 0보다 커지면 해당 임계 이벤트를 다시 발생할 수 있게 한다 (부활, Poise 회복).
	else if (Attribute == GetHealthAttribute() && NewValue > 0.f)
	{
		bOutOfHealth = false;
	}
	else if (Attribute == GetPoiseAttribute() && NewValue > 0.f)
	{
		bPoiseBroken = false;
	}
}

void UknightCoreAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetEffectContext();
	AActor* const Instigator = Context.GetOriginalInstigator();
	AActor* const Causer = Context.GetEffectCauser();

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float LocalDamage = GetDamage();
		SetDamage(0.f);
		if (LocalDamage > 0.f)
		{
			RecordHit();
			const float OldHealth = GetHealth();
			SetHealth(FMath::Clamp(OldHealth - LocalDamage, 0.f, GetMaxHealth()));
			const float NewHealth = GetHealth();
			if (NewHealth < OldHealth)
			{
				OnDamaged.Broadcast(Instigator, Causer, &Data.EffectSpec, OldHealth - NewHealth, OldHealth, NewHealth);
			}
			if (NewHealth <= 0.f && !bOutOfHealth)
			{
				bOutOfHealth = true;
				OnOutOfHealth.Broadcast(Instigator, Causer, &Data.EffectSpec, LocalDamage, OldHealth, NewHealth);
			}
		}
	}
	else if (Data.EvaluatedData.Attribute == GetPoiseDamageAttribute())
	{
		const float LocalPoiseDamage = GetPoiseDamage();
		SetPoiseDamage(0.f);
		if (LocalPoiseDamage > 0.f)
		{
			RecordHit();
			const float OldPoise = GetPoise();
			SetPoise(FMath::Clamp(OldPoise - LocalPoiseDamage, 0.f, GetMaxPoise()));
			if (GetPoise() <= 0.f && !bPoiseBroken)
			{
				bPoiseBroken = true;
				OnPoiseBroken.Broadcast(Instigator, Causer, &Data.EffectSpec, LocalPoiseDamage, OldPoise, GetPoise());
			}
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealingAttribute())
	{
		const float LocalHealing = GetHealing();
		SetHealing(0.f);
		if (LocalHealing > 0.f && !bOutOfHealth)
		{
			SetHealth(FMath::Clamp(GetHealth() + LocalHealing, 0.f, GetMaxHealth()));
		}
	}
}

void UknightCoreAttributeSet::RecordHit()
{
	if (const UWorld* const World = GetWorld())
	{
		LastHitTime = World->GetTimeSeconds();
	}
}
