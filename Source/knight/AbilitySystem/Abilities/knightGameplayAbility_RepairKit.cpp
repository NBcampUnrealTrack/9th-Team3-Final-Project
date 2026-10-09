#include "AbilitySystem/Abilities/knightGameplayAbility_RepairKit.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "AbilitySystem/Effects/knightGameplayEffect_Heal.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Character/knightMovementComponent.h"
#include "Character/knightPartHealthComponent.h"
#include "GameFramework/Character.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

UknightGameplayAbility_RepairKit::UknightGameplayAbility_RepairKit(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EknightAbilityActivationPolicy::OnInputTriggered;
	// 교체 가능 그룹: 공격 도중에는 캔슬 창에서만 나가고, 수리 중에는 캔슬 창을 열지 않으므로 공격·가드·퀵스텝이 막힌다.
	ActivationGroup = EknightAbilityActivationGroup::Exclusive_Replaceable;
	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_Item_RepairKit));
	ActivationOwnedTags.AddTag(knightGameplayTags::State_Repairing);
	// 맞아도 계속(사용자 결정): 작은 피격 경직을 막는다. 피격 경직 어빌리티는 이 어빌리티를 취소하지 않는다.
	ActivationOwnedTags.AddTag(knightGameplayTags::State_SuperArmor);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_HitReact);

	HealEffect = UknightGameplayEffect_Heal::StaticClass();
}

bool UknightGameplayAbility_RepairKit::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	// 풀피면 쓰지 않는다(2026-10-09 사용자 결정, 키트 낭비 방지). 전체 체력이 가득 차 있어도 깎인 부위가 있으면 쓸 수 있다.
	const UAbilitySystemComponent* const ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UknightCoreAttributeSet* const CoreSet = ASC ? ASC->GetSet<UknightCoreAttributeSet>() : nullptr;
	if (!CoreSet || CoreSet->GetHealth() < CoreSet->GetMaxHealth())
	{
		return true;
	}
	const AActor* const Avatar = ActorInfo->AvatarActor.Get();
	const UknightPartHealthComponent* const PartHealth = Avatar ? Avatar->FindComponentByClass<UknightPartHealthComponent>() : nullptr;
	return PartHealth && PartHealth->HasDamagedPart();
}

bool UknightGameplayAbility_RepairKit::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}
	const UAbilitySystemComponent* const ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAttribute Attribute = UknightAttributeSet::GetRepairKitCountAttribute();
	const bool bHasKit = ASC && ASC->HasAttributeSetForAttribute(Attribute) && ASC->GetNumericAttribute(Attribute) >= 1.f;
	if (!bHasKit && OptionalRelevantTags)
	{
		OptionalRelevantTags->AddTag(knightGameplayTags::Ability_ActivateFail_Cost);
	}
	return bHasKit;
}

void UknightGameplayAbility_RepairKit::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	UAbilitySystemComponent* const ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAttribute Attribute = UknightAttributeSet::GetRepairKitCountAttribute();
	if (ASC && ASC->HasAttributeSetForAttribute(Attribute))
	{
		// 개수는 정수로 쓴다. 속성 세트가 0 아래로 내려가지 않게 자른다.
		ASC->SetNumericAttributeBase(Attribute, FMath::FloorToFloat(ASC->GetNumericAttributeBase(Attribute)) - 1.f);
	}
}

void UknightGameplayAbility_RepairKit::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 걷기만: 달리기 키를 누르고 있어도 걷기 속도.
	if (UknightMovementComponent* const Movement = Cast<UknightMovementComponent>(Character->GetCharacterMovement()))
	{
		Movement->SetRunLocked(true);
		bRunLockApplied = true;
	}

	// 사용 시간 = 기본 + 파괴된 부위 수 x 부위당 시간. 시작할 때 정한다(사용 중에 부위가 파괴돼도 늘지 않음).
	const UknightPartHealthComponent* const PartHealth = Character->FindComponentByClass<UknightPartHealthComponent>();
	const int32 BrokenParts = PartHealth ? PartHealth->GetBrokenPartCount() : 0;
	const float UseTime = BaseUseTime + BrokenParts * TimePerBrokenPart;

	// 연출: 수리 중 반복 큐. 어빌리티가 끝나면(사망으로 취소될 때 포함) 자동으로 지워진다. 에셋은 GCN_Repair_Loop(임시 이펙트).
	K2_AddGameplayCue(knightGameplayTags::GameplayCue_Repair_Loop, MakeEffectContext(Handle, ActorInfo), true);

	if (RepairMontage)
	{
		UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, RepairMontage, 1.f, NAME_None, true);
		MontageTask->ReadyForActivation();
	}

	UAbilityTask_WaitDelay* const DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, UseTime);
	DelayTask->OnFinish.AddDynamic(this, &ThisClass::OnRepairFinished);
	DelayTask->ReadyForActivation();

	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	UE_LOG(LogknightAbility, Log, TEXT("%s: repair kit started, use time %.2fs (%d broken parts), kits left %.0f."),
		*GetNameSafe(Character), UseTime, BrokenParts,
		ASC ? ASC->GetNumericAttribute(UknightAttributeSet::GetRepairKitCountAttribute()) : 0.f);
}

void UknightGameplayAbility_RepairKit::OnRepairFinished()
{
	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	const UknightCoreAttributeSet* const CoreSet = ASC ? ASC->GetSet<UknightCoreAttributeSet>() : nullptr;
	if (CoreSet)
	{
		const FGameplayAttribute EfficiencyAttribute = UknightAttributeSet::GetRepairEfficiencyAttribute();
		const float Efficiency = ASC->HasAttributeSetForAttribute(EfficiencyAttribute) ? ASC->GetNumericAttribute(EfficiencyAttribute) : 1.f;
		const float Amount = CoreSet->GetMaxHealth() * HealRatio * Efficiency;

		// 전체 체력: 회복 이펙트(Healing 메타 속성 → Health). 부위가 가득 차 있어도 채운다(기획 문서).
		if (HealEffect && Amount > 0.f)
		{
			const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(HealEffect);
			if (SpecHandle.IsValid())
			{
				SpecHandle.Data->SetSetByCallerMagnitude(knightGameplayTags::SetByCaller_Healing, Amount);
				ApplyGameplayEffectSpecToOwner(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), SpecHandle);
			}
		}
		// 부위 체력: 같은 양을 체력 비율이 낮은 부위부터.
		const AActor* const Avatar = GetAvatarActorFromActorInfo();
		if (UknightPartHealthComponent* const PartHealth = Avatar ? Avatar->FindComponentByClass<UknightPartHealthComponent>() : nullptr)
		{
			PartHealth->ApplyRepair(Amount);
		}
		UE_LOG(LogknightAbility, Log, TEXT("%s: repair kit finished, healed %.1f (max health %.1f x %.2f x efficiency %.2f), health %.1f."),
			*GetNameSafe(Avatar), Amount, CoreSet->GetMaxHealth(), HealRatio, Efficiency, CoreSet->GetHealth());

		// 연출: 회복 순간 한 번. 에셋은 GCN_Repair_Finish(임시 이펙트).
		if (Avatar)
		{
			FGameplayCueParameters CueParams;
			CueParams.Location = Avatar->GetActorLocation();
			CueParams.Normal = FVector::UpVector;
			CueParams.Instigator = GetAvatarActorFromActorInfo();
			CueParams.RawMagnitude = Amount;
			K2_ExecuteGameplayCueWithParams(knightGameplayTags::GameplayCue_Repair_Finish, CueParams);
		}
	}
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UknightGameplayAbility_RepairKit::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bRunLockApplied)
	{
		bRunLockApplied = false;
		const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
		if (UknightMovementComponent* const Movement = Character ? Cast<UknightMovementComponent>(Character->GetCharacterMovement()) : nullptr)
		{
			Movement->SetRunLocked(false);
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
