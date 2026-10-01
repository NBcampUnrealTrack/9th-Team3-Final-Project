#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightAttributeSetBase.h"
#include "knightMonsterAttributeSet.generated.h"

/**
 * Attributes only monsters have. Paired with UknightCoreAttributeSet on every monster.
 * Part HP lives in the monster part component (phase 5), not here, because the part count differs per monster.
 */
UCLASS()
class KNIGHT_API UknightMonsterAttributeSet : public UknightAttributeSetBase
{
	GENERATED_BODY()

public:
	UknightMonsterAttributeSet();

	KNIGHT_ATTRIBUTE_ACCESSORS(UknightMonsterAttributeSet, Rage);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightMonsterAttributeSet, MaxRage);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightMonsterAttributeSet, Stamina);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightMonsterAttributeSet, MaxStamina);

	/** Rage reached MaxRage. The AI applies State.Monster.Enraged in response. */
	mutable FknightAttributeEvent OnRageMaxed;

	/** Stamina reached zero. The AI applies State.Monster.Exhausted in response. */
	mutable FknightAttributeEvent OnStaminaDepleted;

protected:
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

private:
	/** Builds up from damage taken and time in combat. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Monster", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData Rage;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Monster", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData MaxRage;

	/** Spent by big attacks. At zero the monster becomes exhausted. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Monster", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData Stamina;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Monster", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData MaxStamina;

	bool bRageMaxed = false;
	bool bStaminaDepleted = false;
};
