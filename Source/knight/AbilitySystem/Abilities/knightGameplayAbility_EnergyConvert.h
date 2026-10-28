#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_EnergyConvert.generated.h"

/**
 * 에너지 → 수리 키트 전환 (2026-10-28 사용자 결정).
 *
 * 사용법: GA_Knight_EnergyConvert(이 클래스의 BP)를 어빌리티 세트에 InputTag.Energy.Convert(기본 E 키, IA_EnergyConvert)로 넣는다.
 *
 * 누르면 바로 에너지(메카 속성 Energy)를 전환 비용만큼 쓰고 수리 키트 +1(AknightCharacter::ConvertEnergyToRepairKit).
 *   - 비용 = AknightCharacter::EnergyConvertCost(기본 50) − 충전 보조 소켓 수 × EnergyConvertCostPerSocket(기본 10), 최소 MinEnergyConvertCost.
 *   - 에너지가 모자라거나 키트가 최대 개수면 나가지 않는다(아무것도 바뀌지 않음).
 *   - 에너지는 퍼펙트 가드(패링) 1번에 10씩 찬다(AknightCharacter::EnergyPerPerfectGuard). 전에는 100이 차면 키트가 자동으로 생겼다.
 * 독립 그룹이라 공격·가드·퀵스텝 중에도 쓸 수 있고, 다른 행동을 끊지 않는다. 동작(몽타주)은 없다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_EnergyConvert : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_EnergyConvert(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 에너지가 비용 이상이고 키트가 최대 개수보다 적을 때만 발동한다. */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
