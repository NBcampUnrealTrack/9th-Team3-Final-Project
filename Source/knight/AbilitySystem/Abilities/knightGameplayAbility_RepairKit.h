#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_RepairKit.generated.h"

class UAnimMontage;
class UGameplayEffect;

/**
 * 수리 키트 (2026-10-09, 기획 문서 '부위 체력' 규칙 + 사용자 결정).
 *
 * 사용법: GA_Knight_RepairKit(이 클래스의 BP)를 어빌리티 세트에 InputTag.Item.Use(기본 R 키, IA_UseItem)로 넣는다.
 *
 * 흐름: 누름 → 키트 1개 소모(메카 속성 RepairKitCount, 비용처럼 CheckCost·ApplyCost) → 사용 시간 동안 수리 → 끝나면 회복 → 종료.
 *   사용 시간 = BaseUseTime + 파괴된 부위 수 x TimePerBrokenPart. 시작할 때 정한다.
 *   회복량 = 최대 체력 x HealRatio x 수리 효율(메카 속성 RepairEfficiency).
 *     전체 체력은 회복량만큼(HealEffect), 부위 체력은 같은 양을 체력 비율이 낮은 부위부터(UknightPartHealthComponent::ApplyRepair).
 *
 * 사용 중 (사용자 결정):
 *   - 걷기만 된다. 달리기 키를 눌러도 걷기 속도(줄지 않음, 무브먼트 SetRunLocked).
 *     공격·가드·퀵스텝은 활성화 그룹(교체 가능 + 캔슬 창 없음) 때문에 나가지 않는다.
 *   - 맞아도 계속된다. State.SuperArmor로 작은 피격 경직이 나오지 않는다.
 *     그로기 붕괴(비틀거림·다운) 반응은 나오지만 수리는 이어서 끝난다. 사망하면 모든 어빌리티가 취소되어 회복하지 않는다.
 * 풀피(전체 체력이 가득 차 있고 깎인 부위도 없음)면 나가지 않는다(키트 낭비 방지, 2026-10-09 사용자 결정).
 * 연출: 수리 중 GameplayCue.Repair.Loop(반복, 끝나면 제거), 회복 순간 GameplayCue.Repair.Finish(한 번).
 *   큐 에셋 GCN_Repair_Loop·GCN_Repair_Finish(Content/Knight/Abilities/Cues)는 임시 이펙트(SlashTrail 팩 Scifi). 바꿀 때는 에셋만 바꾸면 된다.
 * 공격 도중에 누르면 캔슬 창이 열릴 때 나간다(입력 버퍼).
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_RepairKit : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_RepairKit(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 풀피(전체 체력 최대 + 깎인 부위 없음)면 발동하지 않는다. */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** 행동 에너지 비용에 더해 수리 키트가 1개 이상 있는지 본다. 없으면 Ability.ActivateFail.Cost. */
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** 행동 에너지 비용에 더해 수리 키트를 1개 줄인다. CommitAbility에서 불린다. */
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** 파괴된 부위가 없을 때의 사용 시간. 기본 1.5초 (2026-10-09 사용자 결정, 바뀔 수 있는 값). */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit", meta=(ClampMin=0, Units="s"))
	float BaseUseTime = 1.5f;

	/** 파괴된 부위 하나당 늘어나는 사용 시간. 기본 0.2초 (기획 문서, 바뀔 수 있는 값). 네 부위가 모두 파괴돼 있으면 1.5 + 0.8 = 2.3초. */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit", meta=(ClampMin=0, Units="s"))
	float TimePerBrokenPart = 0.2f;

	/** 최대 체력 대비 회복 비율. 기본 0.3 (기획 문서 '최대 체력 30% 회복', 바뀔 수 있는 값). 수리 효율을 곱한다. */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit", meta=(ClampMin=0))
	float HealRatio = 0.3f;

	/** 전체 체력 회복 이펙트(Healing += SetByCaller.Healing). 기본은 C++ 회복 이펙트(UknightGameplayEffect_Heal). */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit")
	TSubclassOf<UGameplayEffect> HealEffect;

	/** 선택 사항. 수리 동작 몽타주(걷기와 섞이도록 상체 슬롯 권장). 비어 있으면 동작 없이 사용 시간만 흐른다. */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit")
	TObjectPtr<UAnimMontage> RepairMontage;

private:
	/** 사용 시간이 끝났을 때: 회복을 적용하고 끝낸다. */
	UFUNCTION()
	void OnRepairFinished();

	bool bRunLockApplied = false;
};
