#include "AbilitySystem/Abilities/knightGameplayAbility_StanceSwitch.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "System/knightGameplayTags.h"

UknightGameplayAbility_StanceSwitch::UknightGameplayAbility_StanceSwitch(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EknightAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = EknightAbilityActivationGroup::Exclusive_Replaceable;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	ActivationOwnedTags.AddTag(knightGameplayTags::State_Stance_Switching);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_HitReact);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Guarding);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Charging);
}

bool UknightGameplayAbility_StanceSwitch::IsTwoHanded(const UAbilitySystemComponent* ASC)
{
	return ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_Stance_TwoHanded);
}

void UknightGameplayAbility_StanceSwitch::ResetToOneHanded(UAbilitySystemComponent* ASC)
{
	if (ASC && ASC->GetTagCount(knightGameplayTags::State_Stance_TwoHanded) > 0)
	{
		ASC->SetLooseGameplayTagCount(knightGameplayTags::State_Stance_TwoHanded, 0);
	}
}

bool UknightGameplayAbility_StanceSwitch::IsWeaponInHand(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const ACharacter* const Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const USkeletalMeshComponent* const CharacterMesh = Character ? Character->GetMesh() : nullptr;
	if (!CharacterMesh)
	{
		return false;
	}
	TArray<UMeshComponent*> Meshes;
	Character->GetComponents<UMeshComponent>(Meshes);
	for (const UMeshComponent* const Mesh : Meshes)
	{
		if (Mesh != CharacterMesh && Mesh->ComponentHasTag(WeaponComponentTag))
		{
			return Mesh->GetAttachSocketName() == WeaponHandSocket;
		}
	}
	return false;
}

bool UknightGameplayAbility_StanceSwitch::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	// 검이 등에 있으면 전환하지 않는다. 뽑을 때는 항상 한손이다.
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) && IsWeaponInHand(ActorInfo);
}

void UknightGameplayAbility_StanceSwitch::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bToTwoHanded = !IsTwoHanded(ASC);
	if (bToTwoHanded)
	{
		ASC->SetLooseGameplayTagCount(knightGameplayTags::State_Stance_TwoHanded, 1);
	}
	else
	{
		ResetToOneHanded(ASC);
	}

	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());

	const bool bStandingStill = Character && Character->GetVelocity().Size2D() <= MaxSpeedForMontage;
	UAnimMontage* const Montage = bToTwoHanded ? ToTwoHandedMontage.Get() : ToOneHandedMontage.Get();
	if (!Montage || !bStandingStill)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	UAbilityTask_PlayMontageAndWait* const Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, TEXT("StanceSwitch"), Montage, PlayRate * GetActionSpeed());
	Task->OnCompleted.AddDynamic(this, &ThisClass::OnMontageFinished);
	Task->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageFinished);
	Task->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageFinished);
	Task->OnCancelled.AddDynamic(this, &ThisClass::OnMontageFinished);
	Task->ReadyForActivation();
}

void UknightGameplayAbility_StanceSwitch::OnMontageFinished()
{
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}
