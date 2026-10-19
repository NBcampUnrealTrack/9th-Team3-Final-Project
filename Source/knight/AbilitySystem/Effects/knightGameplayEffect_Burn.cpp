#include "AbilitySystem/Effects/knightGameplayEffect_Burn.h"

#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "System/knightGameplayTags.h"

UknightGameplayEffect_Burn::UknightGameplayEffect_Burn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 4초 동안 0.5초마다 2 (총 16). 걸리는 순간에는 피해가 없고 첫 주기부터 들어간다.
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(4.f));
	Period = FScalableFloat(0.5f);
	bExecutePeriodicEffectOnApplication = false;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UknightCoreAttributeSet::GetDamageAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(2.f));
	Modifiers.Add(Modifier);

	// SetStackingType은 모듈 밖으로 공개되지 않아(5.8) 아직 남아 있는 프로퍼티를 직접 쓴다.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackPeriodResetPolicy = EGameplayEffectStackingPeriodPolicy::NeverReset;

	// 죽으면 바로 꺼지고(남은 틱과 불길 표시 제거), 죽은 대상에는 걸리지 않는다.
	UTargetTagRequirementsGameplayEffectComponent* const TagRequirements =
		CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("TagRequirements"));
	TagRequirements->RemovalTagRequirements.RequireTags.AddTag(knightGameplayTags::State_Dead);
	GEComponents.Add(TagRequirements);

	// 걸려 있는 동안 몸에 불길이 붙는다(GCN_Burn_Loop).
	GameplayCues.Add(FGameplayEffectCue(knightGameplayTags::GameplayCue_Burn, 0.f, 1.f));
}
