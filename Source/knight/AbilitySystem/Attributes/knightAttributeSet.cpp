#include "AbilitySystem/Attributes/knightAttributeSet.h"

#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"

UknightAttributeSet::UknightAttributeSet()
{
	InitBoostGauge(100.f);
	InitMaxBoostGauge(100.f);
	InitBoostRecoveryRate(25.f);

	// 부위 체력 기본값: 4부위 x 25 = 100으로, Core의 MaxHealth 기본값(100)과 맞춘다.
	InitHeadHealth(25.f);
	InitMaxHeadHealth(25.f);
	InitTorsoHealth(25.f);
	InitMaxTorsoHealth(25.f);
	InitArmsHealth(25.f);
	InitMaxArmsHealth(25.f);
	InitLegsHealth(25.f);
	InitMaxLegsHealth(25.f);

	// 적재 한도는 임시값. 파츠 데이터가 정해지면 조정한다.
	InitWeight(0.f);
	InitLoadCapacity(100.f);

	// 배율 스탯은 1.0으로 두어 기존 동작이 바뀌지 않게 한다.
	// 퀵스텝 쿨타임은 0.5초 (2026-10-07, 연타 테스트 후 임시로 정한 값 — 이후 조정).
	InitWalkSpeed(1.f);
	InitRunSpeed(1.f);
	InitActionSpeed(1.f);
	InitJumpSpeed(1.f);
	InitDodgeSpeed(1.f);
	InitDodgeCooldown(0.5f);

	InitRepairEfficiency(1.f);
}

void UknightAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UknightAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UknightAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetBoostGaugeAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxBoostGauge());
	}
	else if (Attribute == GetMaxBoostGaugeAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
	else if (Attribute == GetBoostRecoveryRateAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
	// 부위 체력은 0 ~ 해당 부위 최대치.
	else if (Attribute == GetHeadHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHeadHealth());
	}
	else if (Attribute == GetTorsoHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxTorsoHealth());
	}
	else if (Attribute == GetArmsHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxArmsHealth());
	}
	else if (Attribute == GetLegsHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxLegsHealth());
	}
	// 최대치 계열은 1 이상. (0이면 비율 계산이 깨진다)
	else if (Attribute == GetMaxHeadHealthAttribute() || Attribute == GetMaxTorsoHealthAttribute()
		|| Attribute == GetMaxArmsHealthAttribute() || Attribute == GetMaxLegsHealthAttribute()
		|| Attribute == GetLoadCapacityAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
	// 행동 속도·퀵스텝 속도는 0이면 모션·이동이 멈추므로 0.1을 하한으로 둔다.
	else if (Attribute == GetActionSpeedAttribute() || Attribute == GetDodgeSpeedAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.1f);
	}
	// 나머지는 음수만 막는다.
	else if (Attribute == GetWeightAttribute() || Attribute == GetWalkSpeedAttribute()
		|| Attribute == GetRunSpeedAttribute() || Attribute == GetJumpSpeedAttribute()
		|| Attribute == GetDodgeCooldownAttribute() || Attribute == GetRepairEfficiencyAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

void UknightAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetMaxBoostGaugeAttribute())
	{
		AdjustForMaxChange(GetOwningAbilitySystemComponent(), GetBoostGaugeAttribute(), OldValue, NewValue, true);
	}
	else if (Attribute == GetBoostGaugeAttribute() && NewValue > 0.f)
	{
		bBoostDepleted = false;
	}
	// 부위 최대 체력이 바뀌면(파츠 교체) 현재 부위 체력을 같은 비율로 맞춘다.
	else if (Attribute == GetMaxHeadHealthAttribute())
	{
		AdjustForMaxChange(GetOwningAbilitySystemComponent(), GetHeadHealthAttribute(), OldValue, NewValue, true);
	}
	else if (Attribute == GetMaxTorsoHealthAttribute())
	{
		AdjustForMaxChange(GetOwningAbilitySystemComponent(), GetTorsoHealthAttribute(), OldValue, NewValue, true);
	}
	else if (Attribute == GetMaxArmsHealthAttribute())
	{
		AdjustForMaxChange(GetOwningAbilitySystemComponent(), GetArmsHealthAttribute(), OldValue, NewValue, true);
	}
	else if (Attribute == GetMaxLegsHealthAttribute())
	{
		AdjustForMaxChange(GetOwningAbilitySystemComponent(), GetLegsHealthAttribute(), OldValue, NewValue, true);
	}
}

void UknightAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetBoostGaugeAttribute() && GetBoostGauge() <= 0.f && !bBoostDepleted)
	{
		bBoostDepleted = true;
		const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetEffectContext();
		OnBoostDepleted.Broadcast(Context.GetOriginalInstigator(), Context.GetEffectCauser(), &Data.EffectSpec,
			Data.EvaluatedData.Magnitude, GetBoostGauge() - Data.EvaluatedData.Magnitude, GetBoostGauge());
	}
}
