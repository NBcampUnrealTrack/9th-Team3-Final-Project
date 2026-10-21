#include "AbilitySystem/Abilities/knightGameplayAbility.h"

#include "AbilitySystem/knightAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "AbilitySystem/Effects/knightGameplayEffect_StaminaCost.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "Combat/knightMechAttackTiming.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "System/knightGameplayTags.h"

UknightGameplayAbility::UknightGameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

UknightAbilitySystemComponent* UknightGameplayAbility::GetKnightAbilitySystemComponentFromActorInfo() const
{
	return CurrentActorInfo ? Cast<UknightAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent.Get()) : nullptr;
}

bool UknightGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}
	const UAbilitySystemComponent* const ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAttribute Attribute = UknightAttributeSet::GetBoostGaugeAttribute();
	const float Cost = GetEffectiveStaminaCost(ASC);
	if (Cost <= 0.f || !ASC || !ASC->HasAttributeSetForAttribute(Attribute))
	{
		return true;
	}
	const float Stamina = ASC->GetNumericAttribute(Attribute);
	const bool bEnough = bRequireFullStaminaCost ? Stamina >= Cost : Stamina > 0.f;
	if (!bEnough && OptionalRelevantTags)
	{
		OptionalRelevantTags->AddTag(knightGameplayTags::Ability_ActivateFail_Cost);
	}
	return bEnough;
}

void UknightGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	const UAbilitySystemComponent* const ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const float Cost = GetEffectiveStaminaCost(ASC);
	if (Cost <= 0.f || !ASC || !ASC->HasAttributeSetForAttribute(UknightAttributeSet::GetBoostGaugeAttribute()))
	{
		return;
	}
	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo,
		UknightGameplayEffect_StaminaCost::StaticClass());
	if (SpecHandle.IsValid())
	{
		// 남은 양보다 많아도 그대로 뺀다. BoostGauge는 속성 세트에서 0 아래로 내려가지 않는다.
		SpecHandle.Data->SetSetByCallerMagnitude(knightGameplayTags::SetByCaller_BoostCost, -Cost);
		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
	}
}

float UknightGameplayAbility::GetActionSpeed() const
{
	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	const FGameplayAttribute Attribute = UknightAttributeSet::GetActionSpeedAttribute();
	return ASC && ASC->HasAttributeSetForAttribute(Attribute) ? ASC->GetNumericAttribute(Attribute) : 1.f;
}

float UknightGameplayAbility::GetAttackSpeed() const
{
	const UknightAbilitySystemComponent* const ASC = GetKnightAbilitySystemComponentFromActorInfo();
	// (2026-10-20) x 스탠스 공격 속도(한손은 빠르고 양손은 느리다).
	return GetActionSpeed() * (ASC ? ASC->GetAttackAnimationSpeed() * ASC->GetStanceModifiers().AttackSpeedScale : 1.f);
}

FknightMeleeHitParams UknightGameplayAbility::GetStanceScaledHitParams(const FknightMeleeHitParams& Params) const
{
	FknightMeleeHitParams Scaled = Params;
	if (const UknightAbilitySystemComponent* const ASC = GetKnightAbilitySystemComponentFromActorInfo())
	{
		const FknightStanceModifiers& Modifiers = ASC->GetStanceModifiers();
		Scaled.BaseDamage *= Modifiers.DamageScale;
		Scaled.MotionValue *= Modifiers.DamageScale;
		Scaled.PoiseDamage *= Modifiers.PoiseDamageScale;
	}
	return Scaled;
}

float UknightGameplayAbility::GetEffectiveStaminaCost(const UAbilitySystemComponent* ASC) const
{
	const UknightAbilitySystemComponent* const KnightASC = Cast<UknightAbilitySystemComponent>(ASC);
	if (StaminaCost <= 0.f || !KnightASC || !GetAssetTags().HasTag(knightGameplayTags::Ability_Attack))
	{
		return StaminaCost;
	}
	return StaminaCost * KnightASC->GetStanceModifiers().StaminaCostScale;
}

void UknightGameplayAbility::StartMechTiming(UAnimMontage* Montage, float BaseRate)
{
	StopMechTiming();
	const UknightAbilitySystemComponent* const ASC = GetKnightAbilitySystemComponentFromActorInfo();
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UAnimInstance* const Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	// 행동 에너지 속성이 있는 나이트만(몬스터도 같은 ASC를 쓴다).
	if (!bUseMechTiming || !ASC || !Anim || !ASC->HasAttributeSetForAttribute(UknightAttributeSet::GetBoostGaugeAttribute()))
	{
		// (2026-10-21) 소리 문제 확인용. 몬스터(행동 에너지 없음)는 로그를 남기지 않는다.
		if (ASC && ASC->HasAttributeSetForAttribute(UknightAttributeSet::GetBoostGaugeAttribute()))
		{
			UE_LOG(LogTemp, Log, TEXT("MechTiming not used by %s (bUseMechTiming %d, anim %d)"), *GetName(), bUseMechTiming, Anim != nullptr);
		}
		return;
	}
	ActiveMechTiming = FknightMechAttackTiming::Start(Anim, Montage, BaseRate, ASC->GetMechTimingSettings(),
		ASC->HasMatchingGameplayTag(knightGameplayTags::State_Stance_TwoHanded));
}

void UknightGameplayAbility::StopMechTiming()
{
	if (ActiveMechTiming.IsValid())
	{
		ActiveMechTiming->Stop();
		ActiveMechTiming.Reset();
	}
}

void UknightGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopMechTiming();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UknightGameplayAbility::FaceMoveInputDirection() const
{
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	if (!Movement || (ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_LockedOn)))
	{
		return false;
	}
	// 퀵스텝(knightGameplayAbility_BoostDash)과 같은 입력 방향: 무브먼트가 마지막으로 받은 이동 입력.
	const FVector Direction = Movement->GetLastInputVector().GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		return false;
	}
	Character->SetActorRotation(FRotator(0.f, Direction.Rotation().Yaw, 0.f));
	return true;
}

void UknightGameplayAbility::TryActivateAbilityOnSpawn(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) const
{
	// Abilities granted before InitAbilityActorInfo have no avatar yet; the ASC retries once it is set.
	if (ActivationPolicy != EknightAbilityActivationPolicy::OnSpawn || !ActorInfo || !ActorInfo->AvatarActor.IsValid() || Spec.IsActive())
	{
		return;
	}
	if (UAbilitySystemComponent* const ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		ASC->TryActivateAbility(Spec.Handle);
	}
}

bool UknightGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return false;
	}
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const UknightAbilitySystemComponent* const ASC = Cast<UknightAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get());
	if (ASC && ASC->IsActivationGroupBlocked(ActivationGroup, GetAssetTags()))
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(knightGameplayTags::Ability_ActivateFail_ActivationGroup);
		}
		return false;
	}
	return true;
}

void UknightGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);
	TryActivateAbilityOnSpawn(ActorInfo, Spec);
}

bool UknightGameplayAbility::DoesAbilitySatisfyTagRequirements(const UAbilitySystemComponent& AbilitySystemComponent,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::DoesAbilitySatisfyTagRequirements(AbilitySystemComponent, SourceTags, TargetTags, OptionalRelevantTags)
		&& SatisfiesTagRelationshipRequirements(AbilitySystemComponent, OptionalRelevantTags);
}

bool UknightGameplayAbility::SatisfiesTagRelationshipRequirements(const UAbilitySystemComponent& AbilitySystemComponent,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	const UknightAbilitySystemComponent* const ASC = Cast<UknightAbilitySystemComponent>(&AbilitySystemComponent);
	if (!ASC)
	{
		return true;
	}
	FGameplayTagContainer RequiredTags;
	FGameplayTagContainer BlockedTags;
	ASC->GetAdditionalActivationTagRequirements(GetAssetTags(), RequiredTags, BlockedTags);
	if (RequiredTags.IsEmpty() && BlockedTags.IsEmpty())
	{
		return true;
	}

	const FGameplayTagContainer& OwnedTags = AbilitySystemComponent.GetOwnedGameplayTags();
	if (!BlockedTags.IsEmpty() && OwnedTags.HasAny(BlockedTags))
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(UAbilitySystemGlobals::Get().ActivateFailTagsBlockedTag);
			OptionalRelevantTags->AppendMatchingTags(OwnedTags, BlockedTags);
		}
		return false;
	}
	if (!RequiredTags.IsEmpty() && !OwnedTags.HasAll(RequiredTags))
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(UAbilitySystemGlobals::Get().ActivateFailTagsMissingTag);
		}
		return false;
	}
	return true;
}
