#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Combat/knightCombatTypes.h"
#include "knightGameplayAbility.generated.h"

class UknightAbilitySystemComponent;
class UAnimMontage;

/** How an ability is activated. */
UENUM(BlueprintType)
enum class EknightAbilityActivationPolicy : uint8
{
	/** Activate once when the bound input is pressed. */
	OnInputTriggered,
	/** Try to activate every frame while the bound input is held. */
	WhileInputActive,
	/** Activate as soon as the ability is granted (passives, state listeners). */
	OnSpawn
};

/** How an ability relates to other running abilities. */
UENUM(BlueprintType)
enum class EknightAbilityActivationGroup : uint8
{
	/** Runs alongside anything. Death uses this and cancels the rest through its cancel tags. */
	Independent,
	/**
	 * Exclusive. A newer exclusive ability replaces it only inside Window.Cancel, or inside Window.Combo
	 * when the newer ability is an attack. Outside the windows, the press waits in the input buffer.
	 * Attacks, guard, boost dash.
	 */
	Exclusive_Replaceable,
	/** Exclusive. Cancels running replaceable abilities and blocks every other exclusive ability (hit reactions). */
	Exclusive_Blocking,

	MAX UMETA(Hidden)
};

/** Base for every ability in the project. */
UCLASS(Abstract)
class KNIGHT_API UknightGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	EknightAbilityActivationPolicy GetActivationPolicy() const { return ActivationPolicy; }
	EknightAbilityActivationGroup GetActivationGroup() const { return ActivationGroup; }

	/**
	 * 실행 중에 입력이 다시 들어오면 재발동(지금 것을 끝내고 다시 시작)을 시도하는지 (bRetriggerInstancedAbility). (2026-10-08)
	 * 입력 처리(knightAbilitySystemComponent, 입력 버퍼)가 읽는다. 받아들일지는 각 어빌리티의 CanActivateAbility가 정한다(예: 퀵스텝은 경직 중에만).
	 */
	bool CanRetriggerWhileActive() const { return bRetriggerInstancedAbility; }

	/** (2026-10-15) 누른 순간 막혀 있었으면 키를 누르고 있는 동안 다시 시도하는지(bRetryPressWhileHeld). 입력 처리가 읽는다. */
	bool ShouldRetryPressWhileHeld() const { return bRetryPressWhileHeld; }

	UFUNCTION(BlueprintCallable, Category="Knight|Ability")
	UknightAbilitySystemComponent* GetKnightAbilitySystemComponentFromActorInfo() const;

	/** Activates OnSpawn abilities. Called when granted and again when the avatar changes. */
	void TryActivateAbilityOnSpawn(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) const;

	/**
	 * (2026-10-26) 실행 중에 활성화 그룹을 바꾼다. ASC의 그룹별 실행 수도 같이 고친다(배타 그룹으로 바꾸면 실행 중인 교체 가능 배타 어빌리티를 끊음).
	 * 예: 차지 공격은 모으는 동안 독립 그룹이다가, 해방할 때 일반 공격처럼 교체 가능 배타 그룹이 된다(캔슬 창·후딜 규칙을 같이 받게).
	 * 끝난 뒤 원래 그룹으로 되돌리는 건 부른 어빌리티가 EndAbility에서 한다(인스턴스가 다음 발동에도 남으므로).
	 */
	void ChangeActivationGroupWhileActive(EknightAbilityActivationGroup NewGroup);

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** 기본 비용(Cost GE)에 더해 StaminaCost만큼 행동 에너지가 있는지 본다. (2026-10-09) */
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** 기본 비용(Cost GE)에 더해 StaminaCost만큼 행동 에너지를 쓴다. CommitAbility에서 불린다. (2026-10-09) */
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	/**
	 * (2026-10-29 사용자 결정) 끄면 이 어빌리티는 발동하지 않는다(CanActivateAbility가 거절). 어빌리티 세트·입력은 그대로라 BP에서 켜기만 하면 다시 쓴다.
	 * 지금 끈 것: GA_Knight_BoostLunge(락온 돌진), GA_Knight_JumpAttack(점프 공격). 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Knight|Ability")
	bool bAbilityEnabled = true;

	/**
	 * 발동할 때(CommitAbility) 쓰는 행동 에너지(BoostGauge, 화면 이름 행동 에너지/스테미너). 0이면 소모 없음. (2026-10-09)
	 * 어빌리티 BP에서 행동마다 조절한다. 행동 에너지가 없는 액터(몬스터)는 무시한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Knight|Cost", meta=(ClampMin=0))
	float StaminaCost = 0.f;

	/**
	 * 켜면 StaminaCost만큼 남아 있어야 발동한다. 끄면(기본) 조금이라도 남아 있으면 발동하고 모자란 만큼 음수가 된다(가불).
	 * (2026-10-09 사용자 결정: 공격·가드는 조금이라도 있으면 발동, 퀵스텝은 전부 있어야 발동)
	 * (2026-10-26 사용자 결정: 퀵스텝도 가불 — 지금 켜 둔 어빌리티는 없다. 0 이하면 어떤 행동도 못 쓴다.)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Knight|Cost")
	bool bRequireFullStaminaCost = false;

	/**
	 * (2026-10-20) 행동 에너지를 Amount만큼 쓴다(ApplyCost와 같은 GE, 회복 대기도 같다). 발동 뒤 조건에 따라 따로 쓰는 비용용
	 * (점프: 땅에서 실제로 뛸 때만). 행동 에너지가 없는 액터는 무시한다.
	 */
	void ApplyStaminaCost(float Amount) const;

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	/** (2026-10-20) 기계식 타이밍이 남아 있으면 끝낸다. */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Adds the tag relationship mapping's required and blocked tags to the engine checks. */
	virtual bool DoesAbilitySatisfyTagRequirements(const UAbilitySystemComponent& AbilitySystemComponent,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** (2026-10-15) 위 검사 중 태그 관계 매핑 부분만. 엔진 검사를 따로 하는 하위 클래스(공격의 콤보 이어받기)가 쓴다. */
	bool SatisfiesTagRelationshipRequirements(const UAbilitySystemComponent& AbilitySystemComponent, FGameplayTagContainer* OptionalRelevantTags) const;

	/**
	 * 행동 속도 배율(메카 속성 ActionSpeed, 기본 1.0, 하한 0.1). (2026-10-08)
	 * 공격·가드 몽타주의 재생 속도에 곱한다. 몽타주를 시작할 때 읽으므로, 재생 도중 값이 바뀌면 다음 동작부터 반영된다.
	 * 메카 속성 세트가 없는 액터(몬스터 등)는 1.0.
	 */
	float GetActionSpeed() const;

	/**
	 * (2026-10-14) 공격 몽타주용 재생 속도 배율 = 행동 속도 x ASC의 공격 배속(AttackAnimationSpeed, 기본 0.75).
	 * 공격 어빌리티(일반·달리기·반격·강공격 탭·차지, 록온 돌진, 내려찍기, 스킬)가 공격 몽타주에 쓴다. 발도·가드는 GetActionSpeed 그대로.
	 * 시험: PIE 콘솔 knight.Player.AttackSpeed 0.9
	 */
	float GetAttackSpeed() const;

	/**
	 * (2026-10-20) 스탠스(한손/양손) 보정을 곱한 타격 값. 공격 어빌리티가 ApplyMeleeHit·그로기 예측(WatchAttack)에 넘기기 전에 부른다.
	 * 보정값은 ASC(UknightAbilitySystemComponent::GetStanceModifiers). 몬스터는 그대로. bApplyStanceHitScale을 끈 공격도 그대로.
	 */
	FknightMeleeHitParams GetStanceScaledHitParams(const FknightMeleeHitParams& Params) const;

	/** (2026-10-20) 이번에 쓸 행동 에너지. 공격(Ability.Attack 계열)이면 StaminaCost x 스탠스 배율, 아니면 StaminaCost. */
	float GetEffectiveStaminaCost(const UAbilitySystemComponent* ASC) const;

	/**
	 * (2026-10-20) 방금 재생한 공격 모션에 기계식 타이밍(락 → 버스트 → 스톱)을 건다. BaseRate = 그 모션을 재생한 배속.
	 * 이전 것은 끝낸다. 끝낼 때 StopMechTiming. 나이트 ASC가 아니거나 꺼져 있으면 아무것도 하지 않는다.
	 */
	void StartMechTiming(UAnimMontage* Montage, float BaseRate);
	void StopMechTiming();

	/**
	 * 락온 중이 아니고 이동 입력이 있으면 아바타를 그 방향으로 즉시 돌린다. 돌렸으면 true. (2026-10-08)
	 * 퀵스텝과 같은 규칙으로, 방향키를 누른 채 공격·가드를 시작하면 그 방향을 보고 나가게 한다.
	 */
	bool FaceMoveInputDirection() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Knight|Ability Activation")
	EknightAbilityActivationPolicy ActivationPolicy = EknightAbilityActivationPolicy::OnInputTriggered;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Knight|Ability Activation")
	EknightAbilityActivationGroup ActivationGroup = EknightAbilityActivationGroup::Independent;

	/**
	 * (2026-10-15 사용자 결정) OnInputTriggered인데 누른 순간 막혀 있었으면(공격 후딜 등) 키를 누르고 있는 동안 매 프레임 다시 시도한다.
	 * 그 누름으로 한 번 발동하면 끝이다(다른 행동 뒤 자동으로 다시 나가지 않음). 키를 떼면 그만 시도한다. 가드가 켠다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Knight|Ability Activation")
	bool bRetryPressWhileHeld = false;

	/** (2026-10-20) 공격 모션에 기계식 타이밍(ASC MechTiming)을 쓸지. 공격마다 끌 수 있다(StartMechTiming을 부르는 공격만 해당). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Knight|Attack Feel")
	bool bUseMechTiming = true;

	/**
	 * (2026-10-28 사용자 결정) 끄면 이 공격은 스탠스 피해·그로기 보정(GetStanceScaledHitParams)을 받지 않는다. 속도·행동 에너지 보정은 그대로.
	 * 패링 반격처럼 나가는 횟수가 보스 공격에 달린 공격용: 한손이 빨라도 더 나가지 않아서 양손 배율이 그냥 이득이 됐다.
	 * GA_Knight_ParryCounter가 끈다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Knight|Stance")
	bool bApplyStanceHitScale = true;

private:
	TSharedPtr<class FknightMechAttackTiming> ActiveMechTiming;
};
