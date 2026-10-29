#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_StanceSwitch.generated.h"

class UAnimMontage;

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
 * (2026-10-28) 전환 이펙트는 눈부심 피드백으로 제거. 이동 중 전환은 ABP_Knight_Demo의 Weapon style 전이(Inertialization)와
 *   DeadBlending 노드가 직전 포즈에서 자연스럽게 이어 준다.
 * 태그는 누른 순간 바꾼다. 몽타주가 중간에 끊겨도 스탠스는 이미 바뀐 상태다.
 *
 * 활성화 그룹은 공격과 같다(Exclusive_Replaceable): 공격 도중에 누르면 입력 버퍼에서 기다렸다가 캔슬 창에서 나간다.
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

	/** 검이 이 소켓에 붙어 있을 때만 전환할 수 있다. */
	UPROPERTY(EditDefaultsOnly, Category="Stance")
	FName WeaponHandSocket = TEXT("weapon_r");

	UPROPERTY(EditDefaultsOnly, Category="Stance")
	FName WeaponComponentTag = TEXT("Weapon");

private:
	UFUNCTION()
	void OnMontageFinished();

	bool IsWeaponInHand(const FGameplayAbilityActorInfo* ActorInfo) const;
};
