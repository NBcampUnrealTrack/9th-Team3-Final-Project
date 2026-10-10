#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "knightDamageExecution.generated.h"

/**
 * 피해 공식만 따로 떼어 낸 순수 계산 함수 모음.
 * GAS 객체 없이 숫자만 넣고 결과를 받을 수 있어서 자동화 테스트(knightDamageMathTest.cpp)로 검증한다.
 * UknightDamageExecution은 속성을 읽어 이 함수들에 넘기기만 한다.
 */
namespace knightDamageMath
{
	/** 한 번의 피격 계산에 필요한 값. 기본값은 "아무 보정 없음"이다. */
	struct FInputs
	{
		float BaseDamage = 0.f;           // SetByCaller.Damage
		float AttackPower = 0.f;          // 공격하는 쪽 공격력
		float MotionValue = 0.f;          // SetByCaller.MotionValue (100 = 공격력 100%)
		float BasePoiseDamage = 0.f;      // SetByCaller.PoiseDamage
		float StaggerPower = 1.f;         // 공격하는 쪽 경직치 (배율)
		float PartMultiplier = 1.f;       // 맞은 부위 배율 (약점은 1보다 큼)
		float CriticalDamageMultiplier = 1.f;
		bool bCritical = false;           // 치명타 여부 (확률 판정은 호출하는 쪽에서 한다)
		float Defense = 0.f;              // 맞는 쪽 방어력
		bool bGuarded = false;            // 가드 범위 안에서 막았는지
		float GuardEfficiency = 1.f;      // 맞는 쪽 가드 효율 (배율)
		float GuardDamageMultiplier = 0.2f;      // 가드 효율 1.0일 때 남는 피해 비율
		float GuardPoiseDamageMultiplier = 0.5f; // 가드 효율 1.0일 때 남는 그로기 비율
		float DamageTakenMultiplier = 1.f;       // 맞는 쪽 받는 피해 배율 (2026-10-10, 마지막에 곱함)
		float PoiseDamageTakenMultiplier = 1.f;  // 맞는 쪽 받는 그로기 배율 = 그로기 저항 (2026-10-10, 마지막에 곱함)
	};

	struct FResult
	{
		float Damage = 0.f;
		float PoiseDamage = 0.f;

		// 로그용 중간값
		float RawDamage = 0.f;
		float CriticalMultiplier = 1.f;
		float DefenseMultiplier = 1.f;
		float GuardMultiplier = 1.f;
		float GuardPoiseMultiplier = 1.f;
	};

	/**
	 * 가드 효율을 반영한, 가드 후 남는 비율.
	 *   남는 비율 = 1 - (1 - 기준 배율) x 가드 효율   (0~1로 제한)
	 * 효율 1.0이면 기준 배율 그대로, 효율이 높을수록 덜 받는다.
	 * 예) 기준 0.2: 효율 0.5 → 0.6, 1.0 → 0.2, 1.25 → 0.0
	 */
	KNIGHT_API float GetGuardedMultiplier(float BaseGuardMultiplier, float GuardEfficiency);

	/** 방어력 배율 = 100 / (100 + 방어력). 방어력이 음수면 0으로 본다. */
	KNIGHT_API float GetDefenseMultiplier(float Defense);

	/** 위 규칙을 모두 적용한 최종 피해·그로기 피해. 둘 다 0 미만이 되지 않는다. */
	KNIGHT_API FResult Compute(const FInputs& In);
}

/**
 * 한 번의 피격을 Damage·PoiseDamage 메타 속성 출력으로 바꾼다.
 *
 *   Raw    = SetByCaller.Damage + AttackPower * SetByCaller.MotionValue / 100
 *   Damage = Raw * PartMultiplier * Critical * 100 / (100 + Defense) * Guard * DamageTakenMultiplier
 *   Poise  = SetByCaller.PoiseDamage * StaggerPower * GuardPoise * PoiseDamageTakenMultiplier
 *   (2026-10-10) DamageTakenMultiplier·PoiseDamageTakenMultiplier = 맞는 쪽의 받는 피해·그로기 배율(기본 1.0, 파츠 효과용).
 *   마지막에 곱해서 방어력·가드와 상관없이 정확한 비율로 줄인다(예: 0.85 = 받는 피해 -15%).
 *   Guard / GuardPoise = 1 - (1 - 기준 배율) * GuardEfficiency   (가드 범위 안에서 막았을 때만)
 *
 * PartMultiplier는 이펙트 컨텍스트의 히트 결과로 맞는 쪽 ICombatInterface에서 가져오므로,
 * 공격 판정 쪽에서 이펙트를 적용하기 전에 FHitResult를 컨텍스트에 넣어야 한다.
 * 맞는 쪽에 State.Invulnerable이 있으면 피격이 0이 된다.
 * State.Guarding은 공격자가 가드 범위 안에 있을 때 피해를 줄인다.
 * 퍼펙트 가드(State.Parrying)는 UknightCombatLibrary::ApplyMeleeHit이 이펙트를 적용하기 전에 판정한다(가드 범위 안 + 패리 가능한 공격이면
 * 이펙트 자체를 적용하지 않음). 그래서 여기까지 온 피격은 패리 구간이어도 일반 가드·피격으로 계산한다. (2026-10-08, 전에는 여기서 0으로 만들어
 * 등 뒤 공격·패리 불가 공격도 패리 구간이면 피해가 0이었다)
 *
 * 모든 캡처는 UknightCoreAttributeSet에서 읽는다. 공격하는 쪽과 맞는 쪽 ASC 모두 이 세트가 있어야 한다:
 * GAS는 캡처 집계기를 만들 때 소유 세트를 참조하므로, 세트가 없으면 크래시한다.
 *
 * 배율은 블루프린트 서브클래스에서 조정하고, 피해 이펙트가 그 서브클래스를 참조하게 한다.
 */
UCLASS(Blueprintable)
class KNIGHT_API UknightDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UknightDamageExecution();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;

	/** 공격자가 맞는 쪽의 가드 범위(GuardArcDegrees) 안에 있는지. 퍼펙트 가드도 같은 범위를 쓰도록 공개한다(UknightCombatLibrary). */
	bool IsAttackerInGuardArc(const AActor* Target, const AActor* Attacker) const;

protected:
	/** 가드한 피격의 피해 배율. 맞는 쪽 가드 효율이 1.0일 때의 기준값이다. */
	UPROPERTY(EditDefaultsOnly, Category="Guard", meta=(ClampMin=0, ClampMax=1))
	float GuardDamageMultiplier = 0.2f;

	/** 가드한 피격의 그로기 피해 배율. 맞는 쪽 가드 효율이 1.0일 때의 기준값이다. */
	UPROPERTY(EditDefaultsOnly, Category="Guard", meta=(ClampMin=0, ClampMax=1))
	float GuardPoiseDamageMultiplier = 0.5f;

	/** 가드 범위의 전체 각도(도). 맞는 쪽의 정면을 중심으로 한다. */
	UPROPERTY(EditDefaultsOnly, Category="Guard", meta=(ClampMin=0, ClampMax=360))
	float GuardArcDegrees = 150.f;
};
