#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightAttributeSetBase.h"
#include "knightCoreAttributeSet.generated.h"

/**
 * 모든 전투 대상이 가지는 속성: 나이트, 몬스터, 파괴 가능한 목표물.
 *
 * Damage와 PoiseDamage는 메타 속성이다. UknightDamageExecution이 값을 쓰고,
 * PostGameplayEffectExecute가 이를 Health·Poise 감소로 바꾼 뒤 0으로 되돌린다.
 * 그 외의 경로로 Health를 직접 깎아서는 안 된다.
 *
 * 피해 계산(UknightDamageExecution)이 공격하는 쪽·맞는 쪽에서 읽는 속성은 반드시 이 세트에 둔다.
 * 몬스터에는 메카 세트(UknightAttributeSet)가 없어서, 그쪽에 두면 몬스터를 때릴 때 GAS가 크래시한다.
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
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightCoreAttributeSet, PoiseRecoveryRate);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightCoreAttributeSet, AttackPower);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightCoreAttributeSet, StaggerPower);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightCoreAttributeSet, Defense);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightCoreAttributeSet, GuardEfficiency);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightCoreAttributeSet, CriticalChance);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightCoreAttributeSet, CriticalDamageMultiplier);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightCoreAttributeSet, Damage);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightCoreAttributeSet, PoiseDamage);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightCoreAttributeSet, Healing);

	/** Health가 0이 됐을 때. Health가 다시 0보다 커질 때까지 한 번만 발생한다. */
	mutable FknightAttributeEvent OnOutOfHealth;

	/** Poise가 0이 됐을 때. Poise가 다시 0보다 커질 때까지 한 번만 발생한다. */
	mutable FknightAttributeEvent OnPoiseBroken;

	/** 어떤 효과로든 Health가 줄었을 때(피해 계산 이후). Magnitude는 줄어든 Health 양이다. */
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

	/** 경직 저항. 맞으면 줄고, 0이 되면 경직된다. (화면에는 '그로기'로 차오르는 게이지로 표시) */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Core", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData Poise;

	/** Poise의 최대치. (화면 이름 '강인도') */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Core", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData MaxPoise;

	/**
	 * 그로기 회복 속도. 맞지 않는 동안 초당 회복하는 Poise 양.
	 * 기본 0 = 자연 회복 없음(기존 동작과 같음). 몬스터도 같이 쓴다.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Core", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData PoiseRecoveryRate;

	/** 기본 공격력. 나이트는 무기와 파츠에서, 몬스터는 몬스터 데이터에서 정해진다. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Core", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData AttackPower;

	/**
	 * 경직치. 공격이 상대 그로기를 쌓는 양의 배율(기본 1.0). 공격력과 별개다.
	 * 피해 계산이 공격하는 쪽에서 읽으므로 몬스터도 가지는 이 세트에 둔다.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Core", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData StaggerPower;

	/** 고정 방어력. 받는 피해에 100 / (100 + Defense)를 곱한다. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Core", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData Defense;

	/**
	 * 가드 효율. 가드 성공 시 피해·그로기를 줄이는 정도의 배율(기본 1.0).
	 * 1.0이면 피해 계산의 기존 가드 고정값(피해 x0.2, 그로기 x0.5)과 같다.
	 * 피해 계산이 맞는 쪽에서 읽으므로 이 세트에 둔다.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Core", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData GuardEfficiency;

	/** 공격이 치명타가 될 확률(0~1). 피해 계산이 모든 공격자에게서 읽으므로 메카 세트가 아닌 이 세트에 있다. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Core", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData CriticalChance;

	/** 치명타일 때 피해에 곱하는 배율. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Core", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData CriticalDamageMultiplier;

	/** 메타: 들어온 피해. -Health로 바뀐다. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Core|Meta", meta=(AllowPrivateAccess=true, HideFromModifiers))
	FGameplayAttributeData Damage;

	/** 메타: 들어온 그로기 피해. -Poise로 바뀐다. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Core|Meta", meta=(AllowPrivateAccess=true, HideFromModifiers))
	FGameplayAttributeData PoiseDamage;

	/** 메타: 들어온 회복량. +Health로 바뀐다. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Core|Meta", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData Healing;

	bool bOutOfHealth = false;
	bool bPoiseBroken = false;
};
