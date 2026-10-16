#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightAttributeSetBase.h"
#include "knightAttributeSet.generated.h"

/**
 * 나이트 메카만 가지는 속성. 플레이어에게는 UknightCoreAttributeSet과 함께 붙는다.
 * 피해 계산이 읽는 속성은 여기에 두지 않는다: 몬스터에는 이 세트가 없고,
 * 세트가 없는 ASC에서 속성을 읽으려 하면 GAS가 크래시한다.
 *
 * 행동 에너지(BoostGauge, 스테미너) 비용은 어빌리티 BP의 StaminaCost로 낸다(UknightGameplayAbility, SetByCaller.BoostCost). (2026-10-09)
 * 회복은 UknightAbilitySystemComponent가 처리한다: 마지막으로 줄어든 뒤 BoostRecoveryDelay초가 지나면 초당 BoostRecoveryRate만큼 찬다.
 * (2026-10-09 전에는 주기형 무한 GE GE_Knight_BoostRecovery로 회복했다. 사용 후 회복 지연을 넣으려고 그로기 회복과 같은 방식으로 옮김)
 */
UCLASS()
class KNIGHT_API UknightAttributeSet : public UknightAttributeSetBase
{
	GENERATED_BODY()

public:
	UknightAttributeSet();

	// 행동 에너지
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, BoostGauge);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, MaxBoostGauge);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, BoostRecoveryRate);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, BoostRecoveryDelay);

	/** 행동 에너지가 마지막으로 줄어든(행동에 쓴) 월드 시각. 회복 대기 시간 계산에 쓴다(UknightAbilitySystemComponent). */
	float GetLastBoostSpendTime() const { return LastBoostSpendTime; }

	// 부위 체력
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, HeadHealth);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, MaxHeadHealth);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, TorsoHealth);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, MaxTorsoHealth);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, ArmsHealth);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, MaxArmsHealth);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, LegsHealth);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, MaxLegsHealth);

	// 중량
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, Weight);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, LoadCapacity);

	// 속도
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, WalkSpeed);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, RunSpeed);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, ActionSpeed);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, JumpSpeed);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, DodgeSpeed);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, DodgeCooldown);

	// 방어
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, PerfectGuardWindow);

	// 보조
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, RepairEfficiency);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, RepairKitCount);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, MaxRepairKitCount);
	KNIGHT_ATTRIBUTE_ACCESSORS(UknightAttributeSet, RepairKitGauge);

	/** 수리 키트 게이지가 이만큼 차면 키트가 1개 생긴다. (2026-10-16) */
	static constexpr float RepairKitGaugeFull = 100.f;

	/** BoostGauge가 0이 됐을 때. 부스트 어빌리티가 이를 받아 State.Overheated를 건다. */
	mutable FknightAttributeEvent OnBoostDepleted;

protected:
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

private:
	// ---- 행동 에너지 ----

	/** 행동 에너지(화면 이름). 부스트·회피·공격·가드에 소모하고, 0이 되면 과열된다. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData BoostGauge;

	/** 행동 에너지의 최대치. (화면 이름 '발전 출력') */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData MaxBoostGauge;

	/** 부스트를 쓰지 않는 동안 초당 회복하는 게이지 양. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData BoostRecoveryRate;

	/**
	 * 행동 에너지를 쓴 뒤 회복이 시작되기까지의 시간(초). 쓸 때마다 다시 센다. 0이면 바로 회복.
	 * 기본 1.2초(2026-10-13 사용자 결정, 전에는 1.5초). 파츠·강화로 바뀔 수 있다.
	 * 달리는 동안에는 이 시간과 별개로 회복이 멈춘다(UknightAbilitySystemComponent::bPauseBoostRecoveryWhileRunning).
	 */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData BoostRecoveryDelay;

	// ---- 부위 체력 ----
	// 필수 4부위가 각자 체력을 가진다. 부위에 맞으면 부위 체력과 전체 체력(Core의 Health)이 같이 준다.
	// 0 이하면 그 부위가 파괴, 1 이상이면 복구. 최대 체력(Core의 MaxHealth)은 네 부위 최대 체력의 합이다.
	// (피해 분배, 파괴 효과, MaxHealth 합산은 부위 체력 단계에서 연결한다)

	/** 머리 체력. 파괴 시 받는 피해량 증가. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech|Parts", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData HeadHealth;

	/** 머리 체력의 최대치. 머리 파츠로 정해진다. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech|Parts", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData MaxHeadHealth;

	/** 몸통 체력. 파괴 시 강인도(MaxPoise) 감소. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech|Parts", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData TorsoHealth;

	/** 몸통 체력의 최대치. 몸통 파츠로 정해진다. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech|Parts", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData MaxTorsoHealth;

	/** 팔 체력. 파괴 시 공격력 감소. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech|Parts", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData ArmsHealth;

	/** 팔 체력의 최대치. 팔 파츠로 정해진다. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech|Parts", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData MaxArmsHealth;

	/** 다리 체력. 파괴 시 이동 속도 감소. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech|Parts", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData LegsHealth;

	/** 다리 체력의 최대치. 다리 파츠로 정해진다. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech|Parts", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData MaxLegsHealth;

	// ---- 중량 ----

	/** 장착 파츠 무게의 합. 무거울수록 속도가 느려진다. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech|Weight", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData Weight;

	/** 적재 한도. 장착할 수 있는 최대 중량이자 중량 페널티 계산의 기준. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech|Weight", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData LoadCapacity;

	// ---- 속도 ----
	// 속도 스탯은 배율(기본 1.0)이다. 실제 값 = 기본값 x 배율 x 중량 계수.
	// 속도에 영향을 주는 것은 부스터 파츠와 중량뿐이다. (DodgeCooldown만 예외로 초 단위)

	/** 걷기 속도 배율. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech|Speed", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData WalkSpeed;

	/** 달리기 속도 배율. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech|Speed", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData RunSpeed;

	/** 행동 속도 배율. 공격·가드 모션의 재생 속도. 0이면 모션이 멈추므로 하한을 둔다. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech|Speed", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData ActionSpeed;

	/** 점프 힘 배율. (행동 속도의 하위 스탯) */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech|Speed", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData JumpSpeed;

	/** 퀵스텝 이동 속도 배율. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech|Speed", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData DodgeSpeed;

	/** 퀵스텝을 다시 쓰기까지 기다리는 시간(초, 소수점 가능). 대시 이동이 끝난 시점부터 센다. 기본 0.5초(임시). */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech|Speed", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData DodgeCooldown;

	// ---- 방어 ----

	/**
	 * 퍼펙트 가드 판정 시간(초). 가드를 누른 뒤 이 시간 동안 정면에서 막은 패리 가능한 공격은 퍼펙트 가드가 된다
	 * (피해 0, 공격한 적에게 그로기 피해). 그 뒤는 일반 가드. 0이면 퍼펙트 가드 없음.
	 * 기본 0.25초(2026-10-08 사용자 결정). 파츠·강화로 바뀔 수 있다. 가드를 누를 때 읽는다(knightGameplayAbility_Guard).
	 */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech|Defense", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData PerfectGuardWindow;

	// ---- 보조 ----

	/** 수리 키트 회복량 배율(기본 1.0). 특수 파츠·업그레이드로만 오른다. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData RepairEfficiency;

	/**
	 * 남은 수리 키트 개수(정수로 쓴다). 수리 키트 어빌리티(knightGameplayAbility_RepairKit)가 사용할 때 1 줄인다.
	 * 스탯이 아니라 소모품 개수지만, HUD가 다른 값처럼 어트리뷰트 변경 알림으로 읽을 수 있게 여기 둔다(2026-10-09).
	 * 기본 3 = 기획 문서 '출격당 3개'. 출격할 때마다 새 캐릭터라 3개로 시작한다. 보유 수량·상점 연결은 인벤토리 담당이 정해진 뒤.
	 * 테스트 보충: knight.Player.SetStat RepairKitCount 3
	 */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData RepairKitCount;

	/**
	 * (2026-10-16) 수리 키트 최대 개수. 기본 3 = 출격 시작 개수. 이보다 적을 때만 퍼펙트 가드로 수리 키트 게이지가 찬다.
	 * 바뀔 수 있는 값(파츠·업그레이드로 늘릴 수 있게 스탯으로 둠).
	 */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData MaxRepairKitCount;

	/**
	 * (2026-10-16 사용자 결정) 수리 키트 게이지(0 ~ RepairKitGaugeFull 100). 키트가 최대 개수보다 적을 때 퍼펙트 가드에 성공하면
	 * 차고(AknightCharacter::RepairKitGaugePerPerfectGuard, 기본 20 = 퍼펙트 가드 5번), 꽉 차면 키트 +1에 0으로 돌아간다.
	 * 출격할 때마다 0에서 시작한다. HUD는 RepairKitCount처럼 어트리뷰트 변경 알림으로 읽는다.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Mech", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData RepairKitGauge;

	bool bBoostDepleted = false;

	/** 아직 쓴 적이 없으면 아주 먼 과거로 두어, 처음부터 회복 대기 시간이 지난 것으로 본다. */
	float LastBoostSpendTime = -UE_BIG_NUMBER;
};
