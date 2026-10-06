#include "Character/knightPartHealthComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Engine/HitResult.h"
#include "GameplayEffect.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

// ---- 순수 계산 (테스트 대상) ----

namespace knightPartHealth
{
	void DistributeDamage(float (&InOutHealth)[PartCount], float Damage, int32 HitPart)
	{
		if (Damage <= 0.f)
		{
			return;
		}

		// 부위를 아는 피해: 그 부위에서만 뺀다.
		if (HitPart >= 0 && HitPart < PartCount)
		{
			InOutHealth[HitPart] = FMath::Max(0.f, InOutHealth[HitPart] - Damage);
			return;
		}

		// 부위를 모르는 피해: 파괴되지 않은(체력 > 0) 부위에 똑같이 나눈다.
		int32 AliveCount = 0;
		for (const float Health : InOutHealth)
		{
			AliveCount += Health > 0.f ? 1 : 0;
		}
		if (AliveCount == 0)
		{
			return;
		}
		const float Share = Damage / AliveCount;
		for (float& Health : InOutHealth)
		{
			if (Health > 0.f)
			{
				Health = FMath::Max(0.f, Health - Share);
			}
		}
	}
}

// ---- 부위 ↔ 속성·태그 ----

namespace
{
	FGameplayAttribute GetPartHealthAttribute(EknightBodyPart Part)
	{
		switch (Part)
		{
		case EknightBodyPart::Head:  return UknightAttributeSet::GetHeadHealthAttribute();
		case EknightBodyPart::Torso: return UknightAttributeSet::GetTorsoHealthAttribute();
		case EknightBodyPart::Arms:  return UknightAttributeSet::GetArmsHealthAttribute();
		default:                     return UknightAttributeSet::GetLegsHealthAttribute();
		}
	}

	FGameplayAttribute GetPartMaxHealthAttribute(EknightBodyPart Part)
	{
		switch (Part)
		{
		case EknightBodyPart::Head:  return UknightAttributeSet::GetMaxHeadHealthAttribute();
		case EknightBodyPart::Torso: return UknightAttributeSet::GetMaxTorsoHealthAttribute();
		case EknightBodyPart::Arms:  return UknightAttributeSet::GetMaxArmsHealthAttribute();
		default:                     return UknightAttributeSet::GetMaxLegsHealthAttribute();
		}
	}

	EknightBodyPart PartFromIndex(int32 Index)
	{
		return static_cast<EknightBodyPart>(Index);
	}
}

FGameplayTag UknightPartHealthComponent::GetPartTag(EknightBodyPart Part)
{
	switch (Part)
	{
	case EknightBodyPart::Head:  return knightGameplayTags::Knight_Part_Head;
	case EknightBodyPart::Torso: return knightGameplayTags::Knight_Part_Torso;
	case EknightBodyPart::Arms:  return knightGameplayTags::Knight_Part_Arms;
	default:                     return knightGameplayTags::Knight_Part_Legs;
	}
}

FGameplayTag UknightPartHealthComponent::GetBrokenTag(EknightBodyPart Part)
{
	switch (Part)
	{
	case EknightBodyPart::Head:  return knightGameplayTags::State_PartBroken_Head;
	case EknightBodyPart::Torso: return knightGameplayTags::State_PartBroken_Torso;
	case EknightBodyPart::Arms:  return knightGameplayTags::State_PartBroken_Arms;
	default:                     return knightGameplayTags::State_PartBroken_Legs;
	}
}

// ---- 컴포넌트 ----

UknightPartHealthComponent::UknightPartHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UknightPartHealthComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InASC)
{
	// 처음 한 번만 연결한다 (재빙의로 다시 불려도 중복 연결하지 않음).
	if (!InASC || ASC.IsValid())
	{
		return;
	}
	ASC = InASC;

	if (const UknightCoreAttributeSet* const CoreSet = InASC->GetSet<UknightCoreAttributeSet>())
	{
		CoreSet->OnDamaged.AddUObject(this, &ThisClass::HandleDamaged);
	}
	for (int32 Index = 0; Index < knightPartHealth::PartCount; ++Index)
	{
		const EknightBodyPart Part = PartFromIndex(Index);
		InASC->GetGameplayAttributeValueChangeDelegate(GetPartHealthAttribute(Part)).AddUObject(this, &ThisClass::HandlePartHealthChanged, Part);
		InASC->GetGameplayAttributeValueChangeDelegate(GetPartMaxHealthAttribute(Part)).AddWeakLambda(this,
			[this](const FOnAttributeChangeData&) { SyncMaxHealth(); });
	}
	SyncMaxHealth();
}

FGameplayTag UknightPartHealthComponent::ResolveHitPartTag(const FHitResult& Hit) const
{
	const EknightBodyPart* const Found = BoneToPart.Find(Hit.BoneName);
	return GetPartTag(Found ? *Found : DefaultHitPart);
}

float UknightPartHealthComponent::GetDamageMultiplier(const FGameplayTag& PartTag) const
{
	float Multiplier = 1.f;
	for (int32 Index = 0; Index < knightPartHealth::PartCount; ++Index)
	{
		const EknightBodyPart Part = PartFromIndex(Index);
		// 파괴된 부위를 또 맞으면 크리티컬.
		if (PartTag == GetPartTag(Part) && IsPartBroken(Part))
		{
			Multiplier *= BrokenPartHitMultiplier;
		}
	}
	// 머리가 파괴돼 있으면 어디를 맞든 피해 증가.
	if (IsPartBroken(EknightBodyPart::Head))
	{
		Multiplier *= HeadBrokenDamageMultiplier;
	}
	return Multiplier;
}

bool UknightPartHealthComponent::IsPartBroken(EknightBodyPart Part) const
{
	return ASC.IsValid() && ASC->HasMatchingGameplayTag(GetBrokenTag(Part));
}

int32 UknightPartHealthComponent::GetBrokenPartCount() const
{
	int32 Count = 0;
	for (int32 Index = 0; Index < knightPartHealth::PartCount; ++Index)
	{
		Count += IsPartBroken(PartFromIndex(Index)) ? 1 : 0;
	}
	return Count;
}

void UknightPartHealthComponent::HandleDamaged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* EffectSpec,
	float Magnitude, float OldValue, float NewValue)
{
	UAbilitySystemComponent* const OwnerASC = ASC.Get();
	if (!OwnerASC)
	{
		return;
	}

	// 히트 정보가 있으면 맞은 부위, 없으면 -1(나눠 받기).
	int32 HitPart = -1;
	if (const FHitResult* const Hit = EffectSpec ? EffectSpec->GetContext().GetHitResult() : nullptr)
	{
		const EknightBodyPart* const Found = BoneToPart.Find(Hit->BoneName);
		HitPart = static_cast<int32>(Found ? *Found : DefaultHitPart);
	}

	float Health[knightPartHealth::PartCount];
	for (int32 Index = 0; Index < knightPartHealth::PartCount; ++Index)
	{
		Health[Index] = OwnerASC->GetNumericAttributeBase(GetPartHealthAttribute(PartFromIndex(Index)));
	}
	const float Before[knightPartHealth::PartCount] = { Health[0], Health[1], Health[2], Health[3] };

	// Magnitude = 이번 피격으로 실제 줄어든 전체 체력. 같은 양을 부위에서 뺀다.
	knightPartHealth::DistributeDamage(Health, Magnitude, HitPart);

	for (int32 Index = 0; Index < knightPartHealth::PartCount; ++Index)
	{
		if (Health[Index] != Before[Index])
		{
			// 파괴·복구 판정은 값이 바뀔 때 HandlePartHealthChanged가 한다.
			OwnerASC->SetNumericAttributeBase(GetPartHealthAttribute(PartFromIndex(Index)), Health[Index]);
		}
	}
}

void UknightPartHealthComponent::HandlePartHealthChanged(const FOnAttributeChangeData& Data, EknightBodyPart Part)
{
	const bool bBroken = IsPartBroken(Part);
	if (Data.NewValue <= 0.f && !bBroken)
	{
		BreakPart(Part);
	}
	else if (Data.NewValue >= 1.f && bBroken)
	{
		RepairPart(Part);
	}
}

void UknightPartHealthComponent::SyncMaxHealth()
{
	UAbilitySystemComponent* const OwnerASC = ASC.Get();
	if (!OwnerASC || !OwnerASC->GetSet<UknightCoreAttributeSet>())
	{
		return;
	}
	float Sum = 0.f;
	for (int32 Index = 0; Index < knightPartHealth::PartCount; ++Index)
	{
		Sum += OwnerASC->GetNumericAttribute(GetPartMaxHealthAttribute(PartFromIndex(Index)));
	}
	OwnerASC->SetNumericAttributeBase(UknightCoreAttributeSet::GetMaxHealthAttribute(), Sum);
}

void UknightPartHealthComponent::BreakPart(EknightBodyPart Part)
{
	UAbilitySystemComponent* const OwnerASC = ASC.Get();
	if (!OwnerASC)
	{
		return;
	}
	OwnerASC->AddLooseGameplayTag(GetBrokenTag(Part));
	if (const UGameplayEffect* const Penalty = GetPenaltyEffect(Part))
	{
		PenaltyHandles[static_cast<int32>(Part)] = OwnerASC->ApplyGameplayEffectToSelf(Penalty, 1.f, OwnerASC->MakeEffectContext());
	}
	UE_LOG(Logknight, Log, TEXT("%s: part %s broken (%d broken)."), *GetNameSafe(GetOwner()), *GetPartTag(Part).ToString(), GetBrokenPartCount());
}

void UknightPartHealthComponent::RepairPart(EknightBodyPart Part)
{
	UAbilitySystemComponent* const OwnerASC = ASC.Get();
	if (!OwnerASC)
	{
		return;
	}
	OwnerASC->RemoveLooseGameplayTag(GetBrokenTag(Part));
	FActiveGameplayEffectHandle& Handle = PenaltyHandles[static_cast<int32>(Part)];
	if (Handle.IsValid())
	{
		OwnerASC->RemoveActiveGameplayEffect(Handle);
		Handle.Invalidate();
	}
	UE_LOG(Logknight, Log, TEXT("%s: part %s repaired (%d broken)."), *GetNameSafe(GetOwner()), *GetPartTag(Part).ToString(), GetBrokenPartCount());
}

const UGameplayEffect* UknightPartHealthComponent::GetPenaltyEffect(EknightBodyPart Part)
{
	// 머리는 스탯이 아니라 받는 피해 배율(GetDamageMultiplier)로 처리하므로 GE가 없다.
	if (Part == EknightBodyPart::Head)
	{
		return nullptr;
	}

	PenaltyEffects.SetNum(knightPartHealth::PartCount);
	TObjectPtr<UGameplayEffect>& Effect = PenaltyEffects[static_cast<int32>(Part)];
	if (Effect)
	{
		return Effect;
	}

	// 무한 지속 GE를 코드에서 한 번 만들어 재사용한다. 스탯에 (1 - 페널티)를 곱한다.
	auto MakePenalty = [this](const TCHAR* Name, std::initializer_list<FGameplayAttribute> Attributes)
	{
		UGameplayEffect* const NewEffect = NewObject<UGameplayEffect>(this, Name);
		NewEffect->DurationPolicy = EGameplayEffectDurationType::Infinite;
		for (const FGameplayAttribute& Attribute : Attributes)
		{
			FGameplayModifierInfo Modifier;
			Modifier.Attribute = Attribute;
			Modifier.ModifierOp = EGameplayModOp::MultiplyCompound;
			Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.f - BrokenStatPenalty));
			NewEffect->Modifiers.Add(Modifier);
		}
		return NewEffect;
	};

	switch (Part)
	{
	case EknightBodyPart::Torso:
		Effect = MakePenalty(TEXT("GE_PartBroken_Torso"), { UknightCoreAttributeSet::GetMaxPoiseAttribute() });
		break;
	case EknightBodyPart::Arms:
		Effect = MakePenalty(TEXT("GE_PartBroken_Arms"), { UknightCoreAttributeSet::GetAttackPowerAttribute() });
		break;
	default:
		Effect = MakePenalty(TEXT("GE_PartBroken_Legs"), { UknightAttributeSet::GetWalkSpeedAttribute(), UknightAttributeSet::GetRunSpeedAttribute() });
		break;
	}
	return Effect;
}
