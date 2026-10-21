#include "AbilitySystem/Abilities/knightGameplayAbility_CombatStance.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_StanceSwitch.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "System/knightGameplayTags.h"
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
		StillSince = -1.0;
		if (!bInCombat)
		{
			bInCombat = true;
			ASC->AddLooseGameplayTag(CombatStateTag);
		}
		SetAlert(true);
	}
	else if (bInCombat && !bSheathing && !Timers.IsTimerActive(SheatheTimer) && bKeepDrawnWhileMoving)
	{
		// (2026-10-15 사용자 결정) 전투 대기 자세(Alert)를 유지한 채, 멈춘 뒤 StillTimeBeforeSheathe초가 지나면 바로 넣는다.
		// 이동 중에는 넣지 않는다. 이 모드에서는 AlertDuration·SheatheDelay를 쓰지 않는다.
		StillSince = -1.0;
		Timers.SetTimer(SheatheTimer, this, &ThisClass::StartSheathe, 0.1f, false);
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

	// (2026-10-20) 락온 중에는 칼을 넣지 않는다. 멈춰 서서 적을 보다가 칼이 등으로 들어가면 비전투 자세로 인해
	// 락온 옆걸음이 풀린다. 락온이 풀리면 이동 중과 같이 다시 멈춘 시간부터 새로 잰다.
	if (const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
		ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_LockedOn))
	{
		StillSince = -1.0;
		if (UWorld* const World = GetWorld())
		{
			World->GetTimerManager().SetTimer(SheatheTimer, this, &ThisClass::StartSheathe, 0.2f, false);
		}
		return;
	}

	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const bool bStandingStill = Character && Character->GetVelocity().Size2D() <= MaxSpeedForSheatheMontage;
	if (bKeepDrawnWhileMoving)
	{
		// Moving with the sword in hand keeps it drawn; sheathe once the Knight has stood still for a moment.
		UWorld* const World = GetWorld();
		if (World)
		{
			const double Now = World->GetTimeSeconds();
			if (!bStandingStill)
			{
				StillSince = -1.0;
				World->GetTimerManager().SetTimer(SheatheTimer, this, &ThisClass::StartSheathe, 0.2f, false);
				return;
			}
			if (StillSince < 0.0)
			{
				StillSince = Now;
			}
			if (Now - StillSince < StillTimeBeforeSheathe)
			{
				World->GetTimerManager().SetTimer(SheatheTimer, this, &ThisClass::StartSheathe, 0.1f, false);
				return;
			}
		}
		StillSince = -1.0;
	}
	// (2026-10-15) 한손 스탠스면 한손 납도 몽타주.
	UAnimMontage* const Montage = (OneHandedSheatheMontage && !UknightGameplayAbility_StanceSwitch::IsTwoHanded(GetAbilitySystemComponentFromActorInfo()))
		? OneHandedSheatheMontage.Get() : SheatheMontage.Get();
	if (!Montage || !bStandingStill)
	{
		AttachWeaponToBack();
		ExitCombat();
		return;
	}

	bSheathing = true;
	UAbilityTask_PlayMontageAndWait* const Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, TEXT("Sheathe"), Montage, 1.f, NAME_None, false, 0.f);
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
		// (2026-10-15) 검을 넣으면 한손 스탠스로 되돌린다. 다시 뽑으면 항상 한손으로 시작한다(사용자 결정).
		UknightGameplayAbility_StanceSwitch::ResetToOneHanded(ASC);
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
