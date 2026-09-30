#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "knightAttributeSetBase.generated.h"

class UknightAbilitySystemComponent;
struct FGameplayEffectSpec;

/** Getter, value getter/setter and initter for one attribute. */
#define KNIGHT_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * Broadcast for threshold events (out of health, poise broken, boost depleted).
 * Instigator and Causer come from the effect that crossed the threshold and may be null.
 */
DECLARE_MULTICAST_DELEGATE_SixParams(FknightAttributeEvent, AActor* /*EffectInstigator*/, AActor* /*EffectCauser*/,
	const FGameplayEffectSpec* /*EffectSpec*/, float /*EffectMagnitude*/, float /*OldValue*/, float /*NewValue*/);

/**
 * Base for the project's attribute sets.
 * No replication: Project Knight is single-player and runs as a standalone instance.
 */
UCLASS(Abstract)
class KNIGHT_API UknightAttributeSetBase : public UAttributeSet
{
	GENERATED_BODY()

public:
	UknightAbilitySystemComponent* GetKnightAbilitySystemComponent() const;

protected:
	/** Keeps Current <= Max when Max changes, preserving the current/max ratio when bPreserveRatio. */
	static void AdjustForMaxChange(UAbilitySystemComponent* ASC, const FGameplayAttribute& CurrentAttribute,
		float OldMax, float NewMax, bool bPreserveRatio);
};
