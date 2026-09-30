#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightAttributeSetBase.h"
#include "knightAttributeSet.generated.h"

/**
 * Attributes only the Knight mech has. Paired with UknightCoreAttributeSet on the player.
 * Keep attributes that the damage execution captures out of here: monsters do not have this set,
 * and capturing an attribute from an ASC without its set crashes GAS.
 *
 * Boost costs are ordinary GE cost modifiers on BoostGauge (use SetByCaller.BoostCost for variable
 * costs). Recovery is a periodic infinite GE scaled by BoostRecoveryRate, blocked by State.Boosting
 * and State.Overheated.
 */
UCLASS()
class KNIGHT_API UknightAttributeSet : public UknightAttributeSetBase
{
	GENERATED_BODY()

public:
	UknightAttributeSet();

	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, BoostGauge);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, MaxBoostGauge);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, BoostRecoveryRate);

	/** BoostGauge reached zero. The boost ability applies State.Overheated in response. */
	mutable FknightAttributeEvent OnBoostDepleted;

protected:
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

private:
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData BoostGauge;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData MaxBoostGauge;

	/** Gauge recovered per second while not boosting. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData BoostRecoveryRate;

	bool bBoostDepleted = false;
};
