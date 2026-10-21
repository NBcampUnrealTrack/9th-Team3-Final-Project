#include "Item/knightEquipmentStats.h"

#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "AbilitySystem/knightAbilitySet.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "GameplayEffectAggregator.h"
#include "Item/knightEquipmentData.h"
#include "Item/knightItemDefinitions.h"
#include "Item/knightPartsComponent.h"
#include "Progression/knightProgressionSubsystem.h"
#include "System/knightGameplayTags.h"

namespace
{
	using FLoadout = knightEquipmentStats::FLoadout;
	bool IsSetSlot(FGameplayTag Slot)
	{
		return Slot == knightGameplayTags::Equipment_Slot_Head || Slot == knightGameplayTags::Equipment_Slot_Torso
			|| Slot == knightGameplayTags::Equipment_Slot_Arms || Slot == knightGameplayTags::Equipment_Slot_Legs
			|| Slot == knightGameplayTags::Equipment_Slot_Weapon;
	}

	void AddMod(TMap<FGameplayAttribute, TSharedPtr<FAggregator>>& Aggregators,
		const FGameplayAttribute& Attribute, EGameplayModOp::Type Op, float Value)
	{
		if (!Attribute.IsValid()) return;
		TSharedPtr<FAggregator>& Aggregator = Aggregators.FindOrAdd(Attribute);
		if (!Aggregator.IsValid())
		{
			const UAttributeSet* Defaults = Attribute.GetAttributeSetClass()->GetDefaultObject<UAttributeSet>();
			Aggregator = MakeShared<FAggregator>(Attribute.GetNumericValue(Defaults));
		}
		Aggregator->AddAggregatorMod(Value, Op, EGameplayModEvaluationChannel::Channel0, nullptr, nullptr, false);
	}
}

knightEquipmentStats::FLoadout knightEquipmentStats::ReadLoadout(
	const UknightProgressionSubsystem* Progression, const UknightMaintenanceCatalog* Catalog)
{
	FLoadout Result;
	// (2026-10-20) 기본 장착 에셋이 있으면 그것만 읽는다. 아래 캐릭터 클래스 로드는 BP_Knight 전체를 읽어 정비소 첫 진입이 멈췄다.
	if (Catalog && Catalog->DefaultLoadout)
	{
		for (const UknightPartItemDefinition* Part : Catalog->DefaultLoadout->Parts)
		{
			if (Part && Part->SlotTag.IsValid()) Result.Add(Part->SlotTag, Part);
		}
	}
	else if (Catalog && !Catalog->PlayerCharacterClass.IsNull())
	{
		if (const UClass* Class = Catalog->PlayerCharacterClass.LoadSynchronous())
		{
			if (const ACharacter* Character = Class->GetDefaultObject<ACharacter>())
			{
				if (const UknightPartsComponent* Parts = Character->FindComponentByClass<UknightPartsComponent>())
				{
					for (const UknightPartItemDefinition* Part : Parts->GetDefaultLoadout())
					{
						if (Part && Part->SlotTag.IsValid()) Result.Add(Part->SlotTag, Part);
					}
				}
			}
		}
	}
	if (Progression)
	{
		const FGameplayTag Slots[] = { knightGameplayTags::Equipment_Slot_Head, knightGameplayTags::Equipment_Slot_Torso,
			knightGameplayTags::Equipment_Slot_Arms, knightGameplayTags::Equipment_Slot_Legs,
			knightGameplayTags::Equipment_Slot_Weapon, knightGameplayTags::Equipment_Slot_Booster,
			knightGameplayTags::Equipment_Slot_Generator, knightGameplayTags::Equipment_Slot_Exterior };
		for (FGameplayTag Slot : Slots)
		{
			if (const UknightPartItemDefinition* Part = Progression->GetEquippedPart(Slot)) Result.Add(Slot, Part);
			else if (Progression->IsSlotExplicitlyEmpty(Slot)) Result.Remove(Slot);
		}
	}
	return Result;
}

FknightEquipmentStats knightEquipmentStats::Calculate(const FLoadout& Loadout, const UknightMaintenanceCatalog* Catalog,
	const UknightProgressionSubsystem* Progression)
{
	FknightEquipmentStats Result;
	TMap<FGameplayAttribute, TSharedPtr<FAggregator>> Aggregators;
	const FGameplayAttribute DisplayAttributes[] = {
		UknightCoreAttributeSet::GetMaxHealthAttribute(), UknightCoreAttributeSet::GetMaxPoiseAttribute(),
		UknightCoreAttributeSet::GetAttackPowerAttribute(), UknightCoreAttributeSet::GetDefenseAttribute(),
		UknightCoreAttributeSet::GetCriticalChanceAttribute(), UknightCoreAttributeSet::GetGuardEfficiencyAttribute(),
		UknightAttributeSet::GetMaxBoostGaugeAttribute(), UknightAttributeSet::GetLoadCapacityAttribute(),
		UknightAttributeSet::GetWalkSpeedAttribute(), UknightAttributeSet::GetRunSpeedAttribute(),
		UknightAttributeSet::GetMaxHeadHealthAttribute(), UknightAttributeSet::GetMaxTorsoHealthAttribute(),
		UknightAttributeSet::GetMaxArmsHealthAttribute(), UknightAttributeSet::GetMaxLegsHealthAttribute() };
	for (const FGameplayAttribute& Attribute : DisplayAttributes)
	{
		const UAttributeSet* Defaults = Attribute.GetAttributeSetClass()->GetDefaultObject<UAttributeSet>();
		Aggregators.Add(Attribute, MakeShared<FAggregator>(Attribute.GetNumericValue(Defaults)));
	}
	for (const auto& Entry : Loadout)
	{
		const UknightPartItemDefinition* Part = Entry.Value;
		if (!Part) continue;
		if (IsSetSlot(Entry.Key) && Part->SetTag.IsValid()) ++Result.SetCounts.FindOrAdd(Part->SetTag);
		for (const FknightPartStatModifier& Mod : Part->StatModifiers)
		{
			AddMod(Aggregators, Mod.Attribute, Mod.ModifierOp, Mod.Value);
		}
		if (Entry.Key == knightGameplayTags::Equipment_Slot_Weapon)
		{
			if (const UknightWeaponItemDefinition* Weapon = Cast<UknightWeaponItemDefinition>(Part))
			{
				AddMod(Aggregators, UknightCoreAttributeSet::GetAttackPowerAttribute(), EGameplayModOp::AddBase,
					Weapon->GetUpgradeAttackBonus(Progression ? Progression->GetUpgradeLevel(Weapon) : 0));
			}
		}
		if (Part->AbilitySet)
		{
			for (const FknightAbilitySet_GameplayEffect& Granted : Part->AbilitySet->GetGrantedGameplayEffects())
			{
				const UGameplayEffect* Effect = Granted.GameplayEffect ? Granted.GameplayEffect->GetDefaultObject<UGameplayEffect>() : nullptr;
				if (!Effect || Effect->DurationPolicy != EGameplayEffectDurationType::Infinite)
				{
					Result.bComplete = false;
					Result.IncompleteReason = TEXT("비지속 또는 없는 어빌리티 효과는 계산할 수 없습니다.");
					continue;
				}
				const float Level = Granted.EffectLevel + (Progression ? Progression->GetUpgradeLevel(Part) : 0);
				for (const FGameplayModifierInfo& Mod : Effect->Modifiers)
				{
					float Magnitude = 0.f;
					if (Mod.ModifierMagnitude.GetStaticMagnitudeIfPossible(Level, Magnitude))
						AddMod(Aggregators, Mod.Attribute, Mod.ModifierOp, Magnitude);
					else
					{
						Result.bComplete = false;
						Result.IncompleteReason = TEXT("조건부 또는 동적 어빌리티 효과는 계산할 수 없습니다.");
					}
				}
			}
		}
	}
	if (Catalog)
	{
		for (const UknightEquipmentSetDefinition* Set : Catalog->Sets)
		{
			if (!Set || !Set->SetTag.IsValid()) continue;
			const int32 Count = Result.SetCounts.FindRef(Set->SetTag);
			for (const FknightSetTier* Tier : { &Set->TwoPiece, &Set->FourPiece })
			{
				if (Count < Tier->RequiredCount) continue;
				for (const FknightPartStatModifier& Mod : Tier->StatModifiers)
					AddMod(Aggregators, Mod.Attribute, Mod.ModifierOp, Mod.Value);
			}
		}
	}
	FAggregatorEvaluateParameters Params;
	// Part-health component derives core MaxHealth from the four part maxima.
	const float PartHealth = Aggregators.FindChecked(UknightAttributeSet::GetMaxHeadHealthAttribute())->Evaluate(Params)
		+ Aggregators.FindChecked(UknightAttributeSet::GetMaxTorsoHealthAttribute())->Evaluate(Params)
		+ Aggregators.FindChecked(UknightAttributeSet::GetMaxArmsHealthAttribute())->Evaluate(Params)
		+ Aggregators.FindChecked(UknightAttributeSet::GetMaxLegsHealthAttribute())->Evaluate(Params);
	Aggregators.FindChecked(UknightCoreAttributeSet::GetMaxHealthAttribute())->SetBaseValue(PartHealth);
	for (const auto& Entry : Aggregators) Result.Values.Add(Entry.Key, Entry.Value->Evaluate(Params));
	return Result;
}
