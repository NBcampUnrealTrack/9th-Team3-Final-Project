#include "AbilitySystem/Abilities/knightGameplayAbility_MeleeAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Combat/knightCombatLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

UknightGameplayAbility_MeleeAttack::UknightGameplayAbility_MeleeAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EknightAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = EknightAbilityActivationGroup::Exclusive_Replaceable;
	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_Attack));
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_HitReact);
}

void UknightGameplayAbility_MeleeAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!AttackMontage)
	{
		UE_LOG(LogknightAbility, Error, TEXT("%s has no AttackMontage."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bAttackStarted = false;
	if (DrawMontage && !IsWeaponInHand())
	{
		UAbilityTask_PlayMontageAndWait* const DrawTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, TEXT("Draw"), DrawMontage, DrawPlayRate);
		DrawTask->OnBlendOut.AddDynamic(this, &ThisClass::OnDrawFinished);
		DrawTask->OnCompleted.AddDynamic(this, &ThisClass::OnDrawFinished);
		DrawTask->OnInterrupted.AddDynamic(this, &ThisClass::OnDrawInterrupted);
		DrawTask->OnCancelled.AddDynamic(this, &ThisClass::OnDrawInterrupted);
		DrawTask->ReadyForActivation();
		if (UWorld* const World = GetWorld())
		{
			if (!bAttackAfterDraw)
			{
				World->GetTimerManager().SetTimer(DrawTimer, this, &ThisClass::CheckDrawMoveCancel, 0.05f, true);
			}
			else if (DrawAttackStartTime > 0.f)
			{
				World->GetTimerManager().SetTimer(DrawTimer, this, &ThisClass::OnDrawFinished, DrawAttackStartTime / DrawPlayRate, false);
			}
		}
		return;
	}
	StartAttack();
}

void UknightGameplayAbility_MeleeAttack::OnDrawFinished()
{
	if (!IsActive() || bAttackStarted)
	{
		return;
	}
	if (bAttackAfterDraw)
	{
		StartAttack();
	}
	else
	{
		// Draw only: the next press attacks with the sword in hand.
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UknightGameplayAbility_MeleeAttack::CheckDrawMoveCancel()
{
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!IsActive() || bAttackStarted || !Character || !IsWeaponInHand() || Character->GetLastMovementInputVector().IsNearlyZero())
	{
		return;
	}
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DrawTimer);
	}
	MontageStop(RecoveryCancelBlendOut);
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UknightGameplayAbility_MeleeAttack::OnDrawInterrupted()
{
	// Starting the attack montage interrupts the draw; anything else (a hit, death) ends the attack.
	if (IsActive() && !bAttackStarted)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
	}
}

bool UknightGameplayAbility_MeleeAttack::IsWeaponInHand() const
{
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const USkeletalMeshComponent* const CharacterMesh = Character ? Character->GetMesh() : nullptr;
	if (!CharacterMesh)
	{
		return true;
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
	return true;
}

void UknightGameplayAbility_MeleeAttack::StartAttack()
{
	bAttackStarted = true;
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DrawTimer);
	}
	const bool bWarp = UknightCombatLibrary::SetAttackWarpTarget(GetAvatarActorFromActorInfo(), WarpTargetName, MaxWarpDistance,
		WarpStopDistance, bUseSoftTarget, SoftTargetRange, SoftTargetAngle);
	FVector WarpLocation;
	FRotator WarpRotation;
	float WarpTravel = 0.f;
	const bool bLunge = bWarp && UknightCombatLibrary::ComputeAttackWarp(GetAvatarActorFromActorInfo(), MaxWarpDistance, WarpStopDistance,
		bUseSoftTarget, SoftTargetRange, SoftTargetAngle, WarpLocation, WarpRotation, WarpTravel);

	UAbilityTask_WaitGameplayEvent* const HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, knightGameplayTags::Event_Melee_Hit, nullptr, false, true);
	HitTask->EventReceived.AddDynamic(this, &ThisClass::OnMeleeHitEvent);
	HitTask->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage, PlayRate, StartSection);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->ReadyForActivation();

	if (bLunge)
	{
		UknightCombatLibrary::StartAttackLunge(this, AttackMontage, PlayRate, WarpLocation, WarpTravel, MaxLungeTime);
	}

	if (bCancelRecoveryOnMove)
	{
		if (UWorld* const World = GetWorld())
		{
			World->GetTimerManager().SetTimer(MoveCancelTimer, this, &ThisClass::CheckMoveCancel, 0.05f, true);
		}
	}
}

void UknightGameplayAbility_MeleeAttack::CheckMoveCancel()
{
	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!ASC || !Character || !ASC->HasMatchingGameplayTag(knightGameplayTags::Window_Cancel))
	{
		return;
	}
	if (!bMoveCancelInComboWindow && ASC->HasMatchingGameplayTag(knightGameplayTags::Window_Combo))
	{
		return;
	}
	if (Character->GetLastMovementInputVector().IsNearlyZero())
	{
		return;
	}
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MoveCancelTimer);
	}
	MontageStop(RecoveryCancelBlendOut);
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UknightGameplayAbility_MeleeAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MoveCancelTimer);
		World->GetTimerManager().ClearTimer(DrawTimer);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UknightGameplayAbility_MeleeAttack::OnMontageFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UknightGameplayAbility_MeleeAttack::OnMeleeHitEvent(FGameplayEventData Payload)
{
	const FHitResult* const Hit = Payload.TargetData.Num() > 0 && Payload.TargetData.Get(0) ? Payload.TargetData.Get(0)->GetHitResult() : nullptr;
	if (!Hit)
	{
		return;
	}
	const EknightHitOutcome Outcome = UknightCombatLibrary::ApplyMeleeHit(GetAvatarActorFromActorInfo(), *Hit, HitParams);
	OnHitResolved(*Hit, Outcome);
}
