#include "AbilitySystem/Attributes/knightAttributeSet.h"

#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"

UknightAttributeSet::UknightAttributeSet()
{
	InitBoostGauge(100.f);
	InitMaxBoostGauge(100.f);
	InitBoostRecoveryRate(25.f);
	// 행동 에너지를 쓴 뒤 1.2초가 지나야 회복이 시작된다 (2026-10-09 1.5초 → 2026-10-13 사용자 결정 1.2초).
	InitBoostRecoveryDelay(1.2f);

	// 부위 체력 기본값: 4부위 x 25 = 100으로, Core의 MaxHealth 기본값(100)과 맞춘다.
	// (2026-10-21 사용자 결정) 기본 메카 총 체력 100 → 150: 4부위 x 37.5. 기본 메카로 잔열 3판 모두 쓰러져서(체력 + 수리 키트 250으로 약 12번).
	// 출격 때 캐릭터가 MaxHealth를 부위 최대 체력의 합으로 맞추므로 여기만 바꾸면 된다. 바뀔 수 있는 값.
	InitHeadHealth(37.5f);
	InitMaxHeadHealth(37.5f);
	InitTorsoHealth(37.5f);
	InitMaxTorsoHealth(37.5f);
	InitArmsHealth(37.5f);
	InitMaxArmsHealth(37.5f);
	InitLegsHealth(37.5f);
	InitMaxLegsHealth(37.5f);

	// 적재 한도는 임시값. 파츠 데이터가 정해지면 조정한다.
	// (2026-10-21 사용자 결정) 100 → 70: 다리 장비가 한도를 올려 주고(잔열 +30), 무거운 무기·소켓을 쓰려면 다리부터 맞추게. 바뀔 수 있는 값.
	InitWeight(0.f);
	InitLoadCapacity(70.f);

	// 배율 스탯은 1.0으로 두어 기존 동작이 바뀌지 않게 한다.
	// 퀵스텝 쿨타임은 0.5초 (2026-10-07, 연타 테스트 후 임시로 정한 값 — 이후 조정).
	InitWalkSpeed(1.f);
	InitRunSpeed(1.f);
	InitActionSpeed(1.f);
	InitJumpSpeed(1.f);
	InitDodgeSpeed(1.f);
	InitDodgeCooldown(0.5f);

	// 퍼펙트 가드 판정 시간 0.25초 (2026-10-08).
	InitPerfectGuardWindow(0.25f);

	InitRepairEfficiency(1.f);

	// 수리 키트 출격당 3개 (기획 문서). 최대 개수도 3개, 게이지는 0에서 시작 (2026-10-16).
	// (2026-10-19 사용자 결정) 시작·최대 3 → 5개.
	InitRepairKitCount(5.f);
	InitMaxRepairKitCount(5.f);
	InitRepairKitGauge(0.f);
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
	else if (Attribute == GetBoostRecoveryRateAttribute() || Attribute == GetBoostRecoveryDelayAttribute())
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
	else if (Attribute == GetRepairKitGaugeAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, RepairKitGaugeFull);
	}
	// 나머지는 음수만 막는다.
	else if (Attribute == GetWeightAttribute() || Attribute == GetWalkSpeedAttribute()
		|| Attribute == GetRunSpeedAttribute() || Attribute == GetJumpSpeedAttribute()
		|| Attribute == GetDodgeCooldownAttribute() || Attribute == GetPerfectGuardWindowAttribute() || Attribute == GetRepairEfficiencyAttribute()
		|| Attribute == GetRepairKitCountAttribute() || Attribute == GetMaxRepairKitCountAttribute())
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
	else if (Attribute == GetBoostGaugeAttribute())
	{
		if (NewValue > 0.f)
		{
			bBoostDepleted = false;
		}
		// 줄어들면(행동에 씀) 회복 대기 시간을 처음부터 다시 센다. 회복으로 늘어날 때는 기록하지 않는다. (2026-10-09)
		if (NewValue < OldValue)
		{
			if (const UWorld* const World = GetWorld())
			{
				LastBoostSpendTime = World->GetTimeSeconds();
			}
		}
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
