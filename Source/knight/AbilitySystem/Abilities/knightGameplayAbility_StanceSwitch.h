#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_StanceSwitch.generated.h"

class UAnimMontage;
class UNiagaraSystem;

/**
 * (2026-10-15) Q: 한손 ↔ 양손 스탠스 전환. Q·E 스킬(SpinSlash, Shockwave)을 대신한다.
 *
 * 스탠스는 루즈 태그 State.Stance.TwoHanded 하나로 나타낸다. 태그가 없으면 한손, 있으면 양손.
 *   - 애님 인스턴스(UknightAnimInstance::bTwoHandedStance)가 이 태그를 읽어 대기·이동 자세를 고른다.
 *   - 공격(UknightGameplayAbility_MeleeAttack)은 한손일 때 OneHandedAttackMontage를 쓴다.
 *   - 검을 넣으면(UknightGameplayAbility_CombatStance) 태그를 지워서, 다시 뽑으면 항상 한손으로 시작한다.
 *
 * 검이 손에 있을 때만 쓴다. 서 있으면 전환 몽타주(ToTwoHandedMontage / ToOneHandedMontage)를 재생하고,
 * 움직이는 중이면 몽타주 없이 바로 바꾼다(애님 블루프린트가 자세를 블렌드한다).
 * 태그는 누른 순간 바꾼다. 몽타주가 중간에 끊겨도 스탠스는 이미 바뀐 상태다.
 *
 * 활성화 그룹은 공격과 같다(Exclusive_Replaceable): 공격 도중에 누르면 입력 버퍼에서 기다렸다가 캔슬 창에서 나간다.
 *
 * (2026-10-28 사용자 결정) 콤보 도중 전환:
 *   - 캔슬 창이 열리자마자 공격을 끊는다(후딜 시간도 기다리지 않음, ASC bStanceSwitchCancelsAttackRecovery).
 *   - 행동을 끊고 바꿀 때(캔슬 창 중이거나 콤보 단계 태그가 있을 때)는 전환 몽타주 없이 바로 바꾼다(bSkipMontageWhenInterrupting).
 *   - 끊은 공격의 콤보 단계 태그(예: Combo.Greatsword.1)를 ComboKeepTime초 동안 남겨서, 다음 공격 키가 바뀐 스탠스로 다음 단계(2타)를 낸다.
 *     다음 배타 행동(공격·퀵스텝·가드 등)이 나가면 바로 지운다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_StanceSwitch : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_StanceSwitch(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** (2026-10-28) 실행 중인 공격이 끊기기 전에(Super::PreActivate가 끊는다) 콤보 단계 태그를 적어 둔다. */
	virtual void PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
		const FGameplayEventData* TriggerEventData = nullptr) override;

	/** 지금 양손 스탠스인지 (State.Stance.TwoHanded). */
	static bool IsTwoHanded(const UAbilitySystemComponent* ASC);

	/** 스탠스를 한손으로 되돌린다(태그 제거). 검을 넣을 때·뽑을 때 부른다. */
	static void ResetToOneHanded(UAbilitySystemComponent* ASC);

protected:
	/** 한손 → 양손. 비우면 몽타주 없이 바꾼다. */
	UPROPERTY(EditDefaultsOnly, Category="Stance")
	TObjectPtr<UAnimMontage> ToTwoHandedMontage;

	/** 양손 → 한손. 비우면 몽타주 없이 바꾼다. */
	UPROPERTY(EditDefaultsOnly, Category="Stance")
	TObjectPtr<UAnimMontage> ToOneHandedMontage;

	UPROPERTY(EditDefaultsOnly, Category="Stance", meta=(ClampMin=0.01))
	float PlayRate = 1.f;

	/** 이 속도보다 빠르게 움직이고 있으면 몽타주 없이 바로 바꾼다. */
	UPROPERTY(EditDefaultsOnly, Category="Stance", meta=(ClampMin=0, Units="cm/s"))
	float MaxSpeedForMontage = 30.f;

	/**
	 * (2026-10-28 사용자 결정) 행동을 끊고 바꿀 때(캔슬 창 중이거나 콤보 단계 태그가 있을 때)는 서 있어도 전환 몽타주를 재생하지 않는다.
	 * 이펙트만 나오고 자세는 애님 블루프린트가 블렌드한다. 몽타주가 돌면 끝날 때까지 다음 공격이 막혀서 콤보가 끊긴다. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Stance|Combo")
	bool bSkipMontageWhenInterrupting = true;

	/**
	 * (2026-10-28 사용자 결정) 콤보 도중 바꾸면, 끊은 공격의 콤보 단계 태그(ComboTagsToKeep 계열)를 ComboKeepTime초 남긴다.
	 * 그래서 1타 → 스탠스 전환 → 공격 키면 바뀐 스탠스의 2타가 나간다. 끄면 예전처럼 다음 공격이 1타부터.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Stance|Combo")
	bool bKeepComboOnSwitch = true;

	/**
	 * (2026-10-28) 콤보 단계 태그를 남기는 시간(초, 실제 시간). 이 안에 공격 키를 누르면(입력 버퍼에 있던 누름 포함) 다음 단계가 나간다.
	 * 입력 버퍼 시간(0.5초)과 같게 시작한다. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Stance|Combo", meta=(ClampMin=0, Units="s", EditCondition="bKeepComboOnSwitch"))
	float ComboKeepTime = 0.5f;

	/** (2026-10-28) 남길 콤보 단계 태그의 계열. 기본 Combo.Greatsword(1~4타, 강공격 1~4타, 대쉬공격, 반격). */
	UPROPERTY(EditDefaultsOnly, Category="Stance|Combo", meta=(Categories="Combo", EditCondition="bKeepComboOnSwitch"))
	FGameplayTagContainer ComboTagsToKeep;

	/** 검이 이 소켓에 붙어 있을 때만 전환할 수 있다. */
	UPROPERTY(EditDefaultsOnly, Category="Stance")
	FName WeaponHandSocket = TEXT("weapon_r");

	UPROPERTY(EditDefaultsOnly, Category="Stance")
	FName WeaponComponentTag = TEXT("Weapon");

	/** 스탠스 전환 시 검신 스파크 및 등 뒤 부스터 순간 분사 이펙트를 재생할지 여부. */
	UPROPERTY(EditDefaultsOnly, Category="Stance|Effects")
	bool bPlayStanceSwitchEffects = true;

	/** 스탠스를 고쳐 잡을 때 검 손잡이/검신에서 튀는 금속 스파크 (기본: NS_Knight_ParrySpark). */
	UPROPERTY(EditDefaultsOnly, Category="Stance|Effects", meta=(EditCondition="bPlayStanceSwitchEffects"))
	TObjectPtr<UNiagaraSystem> WeaponSparkEffect;

	/** 양손 스탠스로 전환할 때 검에서 터지는 열기/화염 파동 (기본: NS_Hit_Fire_Once). */
	UPROPERTY(EditDefaultsOnly, Category="Stance|Effects", meta=(EditCondition="bPlayStanceSwitchEffects"))
	TObjectPtr<UNiagaraSystem> ToTwoHandedBurstEffect;

	/** 한손 스탠스로 전환할 때 검에서 터지는 청백색 에너지 파동 (기본: NS_Hit_Scifi_Once). */
	UPROPERTY(EditDefaultsOnly, Category="Stance|Effects", meta=(EditCondition="bPlayStanceSwitchEffects"))
	TObjectPtr<UNiagaraSystem> ToOneHandedBurstEffect;

	/** 양손 스탠스 전환 시 등 뒤 부스터에서 짧게 뿜어져 나오는 분사 이펙트 (기본: NS_Knight_Thruster). */
	UPROPERTY(EditDefaultsOnly, Category="Stance|Effects", meta=(EditCondition="bPlayStanceSwitchEffects"))
	TObjectPtr<UNiagaraSystem> ThrusterBurstEffect;

	/** 등 뒤 부스터 소켓 이름 목록. */
	UPROPERTY(EditDefaultsOnly, Category="Stance|Effects", meta=(EditCondition="bPlayStanceSwitchEffects"))
	TArray<FName> ThrusterSockets = { TEXT("thruster_l"), TEXT("thruster_r") };

	/** 양손 전환 시 부스터 분사 유지 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category="Stance|Effects", meta=(ClampMin=0.05, Units="s", EditCondition="bPlayStanceSwitchEffects"))
	float ThrusterBurstDuration = 0.28f;

private:
	UFUNCTION()
	void OnMontageFinished();

	bool IsWeaponInHand(const FGameplayAbilityActorInfo* ActorInfo) const;
	void PlayStanceSwitchEffects(const ACharacter* Character, bool bToTwoHanded) const;

	/** (2026-10-28) 콤보 단계 태그를 루즈 태그로 남기고, ComboKeepTime이 지나거나 다음 배타 행동이 나가면 지운다. */
	void KeepComboTags(UAbilitySystemComponent* ASC, const FGameplayTagContainer& Tags);
	void ReleaseKeptComboTags();
	void OnAbilityActivatedWhileKeeping(UGameplayAbility* Ability);

	/** PreActivate에서 적어 둔 이번 전환의 콤보 단계 태그와 '행동을 끊고 바꾸는지'. */
	FGameplayTagContainer PendingComboTags;
	bool bInterruptingAction = false;

	/** 지금 남겨 둔 콤보 단계 태그(ReleaseKeptComboTags가 지운다). */
	FGameplayTagContainer KeptComboTags;
	FTimerHandle ComboKeepTimer;
	FDelegateHandle ComboKeepActivatedHandle;
};
