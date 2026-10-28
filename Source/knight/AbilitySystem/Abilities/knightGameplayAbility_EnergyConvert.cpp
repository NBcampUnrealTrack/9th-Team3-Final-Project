#include "AbilitySystem/Abilities/knightGameplayAbility_EnergyConvert.h"

#include "System/knightGameplayTags.h"
#include "knightCharacter.h"

UknightGameplayAbility_EnergyConvert::UknightGameplayAbility_EnergyConvert(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EknightAbilityActivationPolicy::OnInputTriggered;
	// 독립 그룹: 누르는 순간 바로 바꾸고 끝난다. 공격·가드·퀵스텝을 끊지도, 그것들에 막히지도 않는다.
	ActivationGroup = EknightAbilityActivationGroup::Independent;
	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_Energy_Convert));
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
}

bool UknightGameplayAbility_EnergyConvert::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const AknightCharacter* const Knight = ActorInfo ? Cast<AknightCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Knight || !Knight->CanConvertEnergyToRepairKit())
	{
		return false;
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UknightGameplayAbility_EnergyConvert::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AknightCharacter* const Knight = Cast<AknightCharacter>(GetAvatarActorFromActorInfo());
	if (!Knight || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Knight->ConvertEnergyToRepairKit();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
