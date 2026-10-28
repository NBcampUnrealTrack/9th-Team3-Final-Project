#include "AbilitySystem/Abilities/knightGameplayAbility_StanceSwitch.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "knight.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "System/knightGameplayTags.h"
#include "TimerManager.h"

UknightGameplayAbility_StanceSwitch::UknightGameplayAbility_StanceSwitch(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EknightAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = EknightAbilityActivationGroup::Exclusive_Replaceable;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// (2026-10-28) 공격 후딜에도 공격을 끊을 수 있게 ASC가 이 태그를 본다(bStanceSwitchCancelsAttackRecovery).
	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_Stance_Switch));
	ComboTagsToKeep.AddTag(knightGameplayTags::Combo_Greatsword);
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

void UknightGameplayAbility_StanceSwitch::PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
	const FGameplayEventData* TriggerEventData)
{
	// (2026-10-28) Super::PreActivate가 실행 중인 공격을 끊으면 그 공격의 콤보 단계 태그도 빠진다 → 먼저 적어 둔다.
	// 지난 전환이 남겨 둔 태그도 여기 들어가서, 콤보 중 두 번 바꿔도 이어진다.
	PendingComboTags.Reset();
	bInterruptingAction = false;
	if (const UAbilitySystemComponent* const ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		if (bKeepComboOnSwitch)
		{
			PendingComboTags = ASC->GetOwnedGameplayTags().Filter(ComboTagsToKeep);
		}
		bInterruptingAction = ASC->HasMatchingGameplayTag(knightGameplayTags::Window_Cancel) || !PendingComboTags.IsEmpty();
	}
	Super::PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate, TriggerEventData);
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
	if (bPlayStanceSwitchEffects && Character)
	{
		PlayStanceSwitchEffects(Character, bToTwoHanded);
	}

	// (2026-10-28) 끊은 공격의 콤보 단계를 남겨서 다음 공격 키가 다음 단계로 이어지게 한다. 지난 전환이 남긴 것은 PendingComboTags에 들어 있다.
	ReleaseKeptComboTags();
	if (!PendingComboTags.IsEmpty())
	{
		KeepComboTags(ASC, PendingComboTags);
	}

	const bool bStandingStill = Character && Character->GetVelocity().Size2D() <= MaxSpeedForMontage;
	UAnimMontage* const Montage = bToTwoHanded ? ToTwoHandedMontage.Get() : ToOneHandedMontage.Get();
	UE_LOG(LogknightAbility, Log, TEXT("%s: switched to %s, interrupting=%d, kept combo tags [%s]"), *GetName(),
		bToTwoHanded ? TEXT("two-handed") : TEXT("one-handed"), bInterruptingAction ? 1 : 0, *KeptComboTags.ToStringSimple());
	// (2026-10-28) 행동을 끊고 바꿀 때는 몽타주 없이 바로 바꾼다(몽타주가 돌면 다음 공격이 막힌다).
	if (!Montage || !bStandingStill || (bSkipMontageWhenInterrupting && bInterruptingAction))
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

void UknightGameplayAbility_StanceSwitch::PlayStanceSwitchEffects(const ACharacter* Character, bool bToTwoHanded) const
{
	USkeletalMeshComponent* const CharacterMesh = Character ? Character->GetMesh() : nullptr;
	if (!CharacterMesh)
	{
		return;
	}

	UMeshComponent* WeaponMesh = nullptr;
	TArray<UMeshComponent*> Meshes;
	Character->GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* const Mesh : Meshes)
	{
		if (Mesh != CharacterMesh && Mesh->ComponentHasTag(WeaponComponentTag))
		{
			WeaponMesh = Mesh;
			break;
		}
	}

	UMeshComponent* const AttachTarget = WeaponMesh ? WeaponMesh : CharacterMesh;
	const FName AttachSocket = WeaponMesh ? NAME_None : WeaponHandSocket;

	UNiagaraSystem* SparkAsset = WeaponSparkEffect;
	if (!SparkAsset)
	{
		SparkAsset = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Knight/Abilities/Cues/NS_Knight_ParrySpark.NS_Knight_ParrySpark"));
	}
	if (SparkAsset)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(SparkAsset, AttachTarget, AttachSocket,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true, true);
	}

	UNiagaraSystem* BurstAsset = bToTwoHanded ? ToTwoHandedBurstEffect.Get() : ToOneHandedBurstEffect.Get();
	if (!BurstAsset)
	{
		BurstAsset = LoadObject<UNiagaraSystem>(nullptr, bToTwoHanded
			? TEXT("/Game/SlashTrail_SoftTofu/Niagara/Fire/NS_Hit_Fire_Once.NS_Hit_Fire_Once")
			: TEXT("/Game/SlashTrail_SoftTofu/Niagara/Scifi/NS_Hit_Scifi_Once.NS_Hit_Scifi_Once"));
	}
	if (BurstAsset)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(BurstAsset, AttachTarget, AttachSocket,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true, true);
	}

	// 양손 스탠스로 전환할 때 등 뒤 부스터에서 짧게 불꽃이 뿜어져 나오는 메카닉 연출
	if (bToTwoHanded)
	{
		UNiagaraSystem* ThrusterAsset = ThrusterBurstEffect;
		if (!ThrusterAsset)
		{
			ThrusterAsset = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Knight/Abilities/Cues/NS_Knight_Thruster.NS_Knight_Thruster"));
		}
		if (ThrusterAsset)
		{
			TArray<TWeakObjectPtr<UNiagaraComponent>> SpawnedThrusters;
			for (const FName& Socket : ThrusterSockets)
			{
				if (UNiagaraComponent* const ThrusterFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
					ThrusterAsset, CharacterMesh, Socket, FVector::ZeroVector, FRotator::ZeroRotator,
					EAttachLocation::SnapToTarget, true, true))
				{
					SpawnedThrusters.Add(ThrusterFX);
				}
			}

			if (!SpawnedThrusters.IsEmpty() && Character->GetWorld())
			{
				FTimerHandle TimerHandle;
				Character->GetWorld()->GetTimerManager().SetTimer(TimerHandle, [SpawnedThrusters]()
				{
					for (const TWeakObjectPtr<UNiagaraComponent>& Comp : SpawnedThrusters)
					{
						if (UNiagaraComponent* const FX = Comp.Get())
						{
							FX->Deactivate();
						}
					}
				}, FMath::Max(0.05f, ThrusterBurstDuration), false);
			}
		}
	}
}

void UknightGameplayAbility_StanceSwitch::KeepComboTags(UAbilitySystemComponent* ASC, const FGameplayTagContainer& Tags)
{
	UWorld* const World = GetWorld();
	if (!ASC || !World || ComboKeepTime <= 0.f)
	{
		return;
	}
	KeptComboTags = Tags;
	ASC->AddLooseGameplayTags(KeptComboTags);
	World->GetTimerManager().SetTimer(ComboKeepTimer, this, &ThisClass::ReleaseKeptComboTags, ComboKeepTime, false);
	ComboKeepActivatedHandle = ASC->AbilityActivatedCallbacks.AddUObject(this, &ThisClass::OnAbilityActivatedWhileKeeping);
}

void UknightGameplayAbility_StanceSwitch::ReleaseKeptComboTags()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ComboKeepTimer);
	}
	if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (ComboKeepActivatedHandle.IsValid())
		{
			ASC->AbilityActivatedCallbacks.Remove(ComboKeepActivatedHandle);
		}
		if (!KeptComboTags.IsEmpty())
		{
			ASC->RemoveLooseGameplayTags(KeptComboTags);
		}
	}
	ComboKeepActivatedHandle.Reset();
	KeptComboTags.Reset();
}

void UknightGameplayAbility_StanceSwitch::OnAbilityActivatedWhileKeeping(UGameplayAbility* Ability)
{
	// 다음 배타 행동(공격·퀵스텝·가드 등)이 나가면 바로 지운다. 공격은 이미 남긴 태그로 발동 검사를 통과한 뒤다.
	// 남겨 두면 그 다음 단계(예: 2타)가 같은 태그로 한 번 더 나가거나, 퀵스텝 뒤 공격이 퀵스텝 공격 대신 2타가 될 수 있다.
	const UknightGameplayAbility* const KnightAbility = Cast<UknightGameplayAbility>(Ability);
	if (KnightAbility && KnightAbility != this && KnightAbility->GetActivationGroup() != EknightAbilityActivationGroup::Independent)
	{
		ReleaseKeptComboTags();
	}
}

void UknightGameplayAbility_StanceSwitch::OnMontageFinished()
{
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}
