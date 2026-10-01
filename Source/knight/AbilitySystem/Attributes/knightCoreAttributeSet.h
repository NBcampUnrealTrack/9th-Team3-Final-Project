#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightAttributeSetBase.h"
#include "knightCoreAttributeSet.generated.h"

/**
 * Attributes every combatant has: the Knight, monsters, and damageable objectives.
 *
 * Damage and PoiseDamage are meta attributes. UknightDamageExecution writes them, and
 * PostGameplayEffectExecute converts them into Health and Poise loss, then resets them to zero.
 * Nothing else should modify Health downward directly.
 */
UCLASS()
class KNIGHT_API UknightCoreAttributeSet : public UknightAttributeSetBase
{
	GENERATED_BODY()

public:
	UknightCoreAttributeSet();

	KNIGHT_ATTRIBUTE_ACCESSORS(UknightCoreAttributeSet, Health);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightCoreAttributeSet, MaxHealth);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightCoreAttributeSet, Poise);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightCoreAttributeSet, MaxPoise);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightCoreAttributeSet, AttackPower);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightCoreAttributeSet, Defense);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightCoreAttributeSet, CriticalChance);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightCoreAttributeSet, CriticalDamageMultiplier);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightCoreAttributeSet, Damage);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightCoreAttributeSet, PoiseDamage);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightCoreAttributeSet, Healing);

	/** Health reached zero. Fires once until Health is restored above zero. */
	mutable FknightAttributeEvent OnOutOfHealth;

	/** Poise reached zero. Fires once until Poise is restored above zero. */
	mutable FknightAttributeEvent OnPoiseBroken;

	/** Any effect lowered Health (after damage resolution). Magnitude is the Health lost. */
	mutable FknightAttributeEvent OnDamaged;

protected:
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

private:
	UPROPERTY(BlueprintReadOnly, Category="Knight|Core", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData Health;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Core", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData MaxHealth;

	/** Stagger resistance. Hits drain it; at zero the owner is staggered. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Core", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData Poise;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Core", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData MaxPoise;

	/** Raw attack. For the Knight it comes from the weapon and parts; for monsters from the monster data. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Core", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData AttackPower;

	/** Flat defense. Damage is scaled by 100 / (100 + Defense). */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Core", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData Defense;

	/** 0..1 chance for an outgoing hit to be critical. Lives here, not in the mech set, because every attacker is captured for it. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Core", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData CriticalChance;

	/** Damage multiplier applied to critical hits. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Core", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData CriticalDamageMultiplier;

	/** Meta: incoming damage, converted to -Health. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Core|Meta", meta=(AllowPrivateAccess=true, HideFromModifiers))
	FGameplayAttributeData Damage;

	/** Meta: incoming poise damage, converted to -Poise. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Core|Meta", meta=(AllowPrivateAccess=true, HideFromModifiers))
	FGameplayAttributeData PoiseDamage;

	/** Meta: incoming healing, converted to +Health. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Core|Meta", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData Healing;

	bool bOutOfHealth = false;
	bool bPoiseBroken = false;
};
