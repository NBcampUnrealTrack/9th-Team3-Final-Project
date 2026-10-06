#include "AbilitySystem/Abilities/knightGameplayAbility_CombatStance.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

UknightGameplayAbility_CombatStance::UknightGameplayAbility_CombatStance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EknightAbilityActivationPolicy::OnSpawn;
	ActivationGroup = EknightAbilityActivationGroup::Independent;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UknightGameplayAbility_CombatStance::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !CombatStateTag.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	for (const FGameplayTag& Tag : CombatTriggerTags)
	{
		const FDelegateHandle DelegateHandle = ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::AnyCountChange)
			.AddUObject(this, &ThisClass::OnTriggerTagChanged);
		TagEventHandles.Emplace(Tag, DelegateHandle);
	}
	RefreshCombatState();
}

void UknightGameplayAbility_CombatStance::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SheatheTimer);
		World->GetTimerManager().ClearTimer(AlertTimer);
	}
	if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
	{
		for (const TPair<FGameplayTag, FDelegateHandle>& Entry : TagEventHandles)
		{
			ASC->RegisterGameplayTagEvent(Entry.Key, EGameplayTagEventType::AnyCountChange).Remove(Entry.Value);
		}
	}
	TagEventHandles.Reset();
	ExitCombat();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UknightGameplayAbility_CombatStance::OnTriggerTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	RefreshCombatState();
}

bool UknightGameplayAbility_CombatStance::HasAnyTriggerTag() const
{
	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	return ASC && ASC->HasAnyMatchingGameplayTags(CombatTriggerTags);
}

void UknightGameplayAbility_CombatStance::RefreshCombatState()
{
	UWorld* const World = GetWorld();
	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	if (!World || !ASC)
	{
		return;
	}

	FTimerManager& Timers = World->GetTimerManager();
	if (HasAnyTriggerTag())
	{
		Timers.ClearTimer(SheatheTimer);
		Timers.ClearTimer(AlertTimer);
		bSheathing = false;
		if (!bInCombat)
		{
			bInCombat = true;
			ASC->AddLooseGameplayTag(CombatStateTag);
		}
		SetAlert(true);
	}
	else if (bInCombat && !bSheathing && !Timers.IsTimerActive(SheatheTimer))
	{
		// Alert phase, then hold phase, then sheathe.
		if (AlertDuration > 0.f)
		{
			Timers.SetTimer(AlertTimer, this, &ThisClass::EndAlert, AlertDuration, false);
		}
		else
		{
			SetAlert(false);
		}
		Timers.SetTimer(SheatheTimer, this, &ThisClass::StartSheathe, FMath::Max(AlertDuration + SheatheDelay, 0.01f), false);
	}
}

void UknightGameplayAbility_CombatStance::EndAlert()
{
	if (!HasAnyTriggerTag())
	{
		SetAlert(false);
	}
}

void UknightGameplayAbility_CombatStance::SetAlert(const bool bNewAlert)
{
	if (bAlert == bNewAlert || !CombatAlertTag.IsValid())
	{
		bAlert = bNewAlert && CombatAlertTag.IsValid();
		return;
	}
	bAlert = bNewAlert;
	if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (bAlert)
		{
			ASC->AddLooseGameplayTag(CombatAlertTag);
		}
		else
		{
			ASC->RemoveLooseGameplayTag(CombatAlertTag);
		}
	}
}

void UknightGameplayAbility_CombatStance::StartSheathe()
{
	if (HasAnyTriggerTag())
	{
		return;
	}

	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const bool bStandingStill = Character && Character->GetVelocity().Size2D() <= MaxSpeedForSheatheMontage;
	if (!SheatheMontage || !bStandingStill)
	{
		AttachWeaponToBack();
		ExitCombat();
		return;
	}

	bSheathing = true;
	UAbilityTask_PlayMontageAndWait* const Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, TEXT("Sheathe"), SheatheMontage, 1.f, NAME_None, false, 0.f);
	Task->OnCompleted.AddDynamic(this, &ThisClass::OnSheatheMontageFinished);
	Task->OnBlendOut.AddDynamic(this, &ThisClass::OnSheatheMontageFinished);
	Task->OnInterrupted.AddDynamic(this, &ThisClass::OnSheatheMontageInterrupted);
	Task->OnCancelled.AddDynamic(this, &ThisClass::OnSheatheMontageInterrupted);
	Task->ReadyForActivation();
}

void UknightGameplayAbility_CombatStance::OnSheatheMontageFinished()
{
	if (!bSheathing)
	{
		return;
	}
	bSheathing = false;
	AttachWeaponToBack();
	ExitCombat();
}

void UknightGameplayAbility_CombatStance::OnSheatheMontageInterrupted()
{
	// Interrupted by an attack or a hit: the trigger tags are back, so stay in combat with the sword in hand.
	bSheathing = false;
	RefreshCombatState();
}

void UknightGameplayAbility_CombatStance::ExitCombat()
{
	SetAlert(false);
	if (!bInCombat)
	{
		return;
	}
	bInCombat = false;
	if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveLooseGameplayTag(CombatStateTag);
	}
}

void UknightGameplayAbility_CombatStance::AttachWeaponToBack() const
{
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* const CharacterMesh = Character ? Character->GetMesh() : nullptr;
	if (!CharacterMesh || !CharacterMesh->DoesSocketExist(BackSocket))
	{
		return;
	}
	TArray<UMeshComponent*> Meshes;
	Character->GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* const Mesh : Meshes)
	{
		if (Mesh != CharacterMesh && Mesh->ComponentHasTag(WeaponComponentTag))
		{
			Mesh->AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, BackSocket);
			return;
		}
	}
}
