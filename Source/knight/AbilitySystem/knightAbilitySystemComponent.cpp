#include "AbilitySystem/knightAbilitySystemComponent.h"

#include "AbilitySystem/knightAbilityTagRelationshipMapping.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Engine/World.h"
#include "System/knightGameplayTags.h"
#include "TimerManager.h"
#include "knight.h"

// ---- 그로기 자연 회복: 순수 계산 (테스트 대상) ----

namespace knightPoiseRecovery
{
	float Step(float CurrentPoise, float MaxPoise, float RecoveryRate, float DeltaSeconds,
		float SecondsSinceLastHit, float RecoveryDelay)
	{
		if (RecoveryRate <= 0.f || CurrentPoise <= 0.f || CurrentPoise >= MaxPoise || SecondsSinceLastHit < RecoveryDelay)
		{
			return CurrentPoise;
		}
		return FMath::Min(MaxPoise, CurrentPoise + RecoveryRate * FMath::Max(0.f, DeltaSeconds));
	}
}

UknightAbilitySystemComponent::UknightAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FMemory::Memzero(ActivationGroupCounts, sizeof(ActivationGroupCounts));
}

void UknightAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	const FGameplayAbilityActorInfo* const ActorInfo = AbilityActorInfo.Get();
	const bool bHasNewAvatar = InAvatarActor && (!ActorInfo || InAvatarActor != ActorInfo->AvatarActor.Get());

	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);

	if (bHasNewAvatar)
	{
		TryActivateAbilitiesOnSpawn();
	}
	StartPoiseRecovery();
}

void UknightAbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PoiseRecoveryTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void UknightAbilitySystemComponent::StartPoiseRecovery()
{
	UWorld* const World = GetWorld();
	// 에디터의 BP 미리보기 같은 게임 밖 월드에서는 켜지 않는다.
	if (!World || !World->IsGameWorld() || World->GetTimerManager().IsTimerActive(PoiseRecoveryTimer))
	{
		return;
	}
	World->GetTimerManager().SetTimer(PoiseRecoveryTimer, this, &ThisClass::TickPoiseRecovery, PoiseRecoveryInterval, true);
}

void UknightAbilitySystemComponent::TickPoiseRecovery()
{
	const UknightCoreAttributeSet* const CoreSet = GetSet<UknightCoreAttributeSet>();
	const UWorld* const World = GetWorld();
	if (!CoreSet || !World || HasMatchingGameplayTag(knightGameplayTags::State_Dead))
	{
		return;
	}

	const FGameplayAttribute PoiseAttribute = UknightCoreAttributeSet::GetPoiseAttribute();
	const float CurrentPoise = GetNumericAttributeBase(PoiseAttribute);
	const float NewPoise = knightPoiseRecovery::Step(CurrentPoise, CoreSet->GetMaxPoise(), CoreSet->GetPoiseRecoveryRate(),
		PoiseRecoveryInterval, World->GetTimeSeconds() - CoreSet->GetLastHitTime(), PoiseRecoveryDelay);
	if (NewPoise != CurrentPoise)
	{
		// 몬스터 반응(UknightGameplayAbility_MonsterReaction)과 같은 방식으로 기본값을 바꾼다.
		SetNumericAttributeBase(PoiseAttribute, NewPoise);
	}
}

void UknightAbilitySystemComponent::TryActivateAbilitiesOnSpawn()
{
	ABILITYLIST_SCOPE_LOCK();
	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (const UknightGameplayAbility* const AbilityCDO = Cast<UknightGameplayAbility>(Spec.Ability))
		{
			AbilityCDO->TryActivateAbilityOnSpawn(AbilityActorInfo.Get(), Spec);
		}
	}
}

void UknightAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}
	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (Spec.Ability && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			InputPressedSpecHandles.AddUnique(Spec.Handle);
			InputHeldSpecHandles.AddUnique(Spec.Handle);
		}
	}
}

void UknightAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}
	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (Spec.Ability && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			InputReleasedSpecHandles.AddUnique(Spec.Handle);
			InputHeldSpecHandles.Remove(Spec.Handle);
		}
	}
}

void UknightAbilitySystemComponent::ProcessAbilityInput(float DeltaTime, bool bGamePaused)
{
	if (HasMatchingGameplayTag(knightGameplayTags::State_InputBlocked))
	{
		ClearAbilityInput();
		return;
	}

	RecentlyActivatedInputTags.Reset();
	TArray<FGameplayAbilitySpecHandle, TInlineAllocator<8>> AbilitiesToActivate;

	// 누르고 있는 입력: WhileInputActive 어빌리티는 발동할 때까지 매 프레임 다시 시도한다.
	for (const FGameplayAbilitySpecHandle& Handle : InputHeldSpecHandles)
	{
		const FGameplayAbilitySpec* const Spec = FindAbilitySpecFromHandle(Handle);
		if (Spec && Spec->Ability && !Spec->IsActive())
		{
			const UknightGameplayAbility* const AbilityCDO = Cast<UknightGameplayAbility>(Spec->Ability);
			if (AbilityCDO && AbilityCDO->GetActivationPolicy() == EknightAbilityActivationPolicy::WhileInputActive)
			{
				AbilitiesToActivate.AddUnique(Spec->Handle);
			}
		}
	}

	// 이번 프레임에 누른 입력: 실행 중인 어빌리티는 누름을 전달받고, 대기 중인 OnInputTriggered 어빌리티는 발동한다.
	for (const FGameplayAbilitySpecHandle& Handle : InputPressedSpecHandles)
	{
		FGameplayAbilitySpec* const Spec = FindAbilitySpecFromHandle(Handle);
		if (!Spec || !Spec->Ability)
		{
			continue;
		}
		Spec->InputPressed = true;
		const UknightGameplayAbility* const AbilityCDO = Cast<UknightGameplayAbility>(Spec->Ability);
		const bool bTriggeredByPress = AbilityCDO && AbilityCDO->GetActivationPolicy() == EknightAbilityActivationPolicy::OnInputTriggered;
		if (Spec->IsActive())
		{
			AbilitySpecInputPressed(*Spec);
			// 재발동을 허용하는 어빌리티(퀵스텝)는 실행 중에도 발동을 시도한다. 받아들일지는 어빌리티의 CanActivateAbility가 정한다. (2026-10-08)
			if (bTriggeredByPress && AbilityCDO->CanRetriggerWhileActive())
			{
				AbilitiesToActivate.AddUnique(Spec->Handle);
			}
		}
		else if (bTriggeredByPress)
		{
			AbilitiesToActivate.AddUnique(Spec->Handle);
		}
	}

	// 누름 처리를 끝낸 뒤 발동해야, 한 번의 누름으로 발동한 어빌리티가 같은 누름 이벤트를 또 받지 않는다.
	// 같은 입력 태그에 묶인 어빌리티는 한 번에 하나만 발동한다 (입력 버퍼의 TryActivateAbilityByInputTag와 같은 규칙).
	// 콤보처럼 단계들이 같은 키를 쓰면, 2단계가 발동하며 생긴 태그로 3단계까지 같은 프레임에 연달아 발동해 2단계를 건너뛸 수 있다.
	// (앞 단계 몽타주의 Window.Combo는 노티파이가 끝나는 다음 애니메이션 업데이트까지 남아 있다)
	FGameplayTagContainer ActivatedInputTags;
	for (const FGameplayAbilitySpecHandle& Handle : AbilitiesToActivate)
	{
		const FGameplayAbilitySpec* const Spec = FindAbilitySpecFromHandle(Handle);
		if (!Spec)
		{
			continue;
		}
		// 동적 소스 태그에는 InputTag만 들어 있다 (UknightAbilitySet 참고).
		const FGameplayTagContainer InputTags = Spec->GetDynamicSpecSourceTags();
		if (InputTags.HasAnyExact(ActivatedInputTags))
		{
			continue;
		}
		if (TryActivateAbility(Handle))
		{
			ActivatedInputTags.AppendTags(InputTags);
		}
	}

	// 이번 프레임에 뗀 입력: 실행 중인 어빌리티가 뗌을 전달받는다 (차지 공격은 여기서 차지를 끝낸다).
	for (const FGameplayAbilitySpecHandle& Handle : InputReleasedSpecHandles)
	{
		FGameplayAbilitySpec* const Spec = FindAbilitySpecFromHandle(Handle);
		if (!Spec || !Spec->Ability)
		{
			continue;
		}
		Spec->InputPressed = false;
		if (Spec->IsActive())
		{
			AbilitySpecInputReleased(*Spec);
		}
	}

	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
}

void UknightAbilitySystemComponent::ClearAbilityInput()
{
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
	InputHeldSpecHandles.Reset();
}

bool UknightAbilitySystemComponent::TryActivateAbilityByInputTag(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid() || HasMatchingGameplayTag(knightGameplayTags::State_InputBlocked))
	{
		return false;
	}
	TArray<FGameplayAbilitySpecHandle, TInlineAllocator<4>> Candidates;
	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		// 실행 중인 어빌리티는 재발동을 허용할 때만 후보로 둔다 (ProcessAbilityInput과 같은 규칙).
		const UknightGameplayAbility* const AbilityCDO = Cast<UknightGameplayAbility>(Spec.Ability);
		const bool bCanTry = !Spec.IsActive() || (AbilityCDO && AbilityCDO->CanRetriggerWhileActive());
		if (Spec.Ability && bCanTry && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			Candidates.Add(Spec.Handle);
		}
	}
	for (const FGameplayAbilitySpecHandle& Handle : Candidates)
	{
		if (TryActivateAbility(Handle))
		{
			return true;
		}
	}
	return false;
}

void UknightAbilitySystemComponent::AbilitySpecInputPressed(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputPressed(Spec);
	// UAbilityTask_WaitInputPress를 깨운다. 프로젝트의 모든 어빌리티는 인스턴스형이라 기본 인스턴스가 키를 가지고 있다.
	if (Spec.IsActive())
	{
		if (const UGameplayAbility* const Instance = Spec.GetPrimaryInstance())
		{
			InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle,
				Instance->GetCurrentActivationInfo().GetActivationPredictionKey());
		}
	}
}

void UknightAbilitySystemComponent::AbilitySpecInputReleased(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputReleased(Spec);
	// UAbilityTask_WaitInputRelease를 깨운다.
	if (Spec.IsActive())
	{
		if (const UGameplayAbility* const Instance = Spec.GetPrimaryInstance())
		{
			InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle,
				Instance->GetCurrentActivationInfo().GetActivationPredictionKey());
		}
	}
}

bool UknightAbilitySystemComponent::IsActivationGroupBlocked(EknightAbilityActivationGroup Group, const FGameplayTagContainer& AbilityTags) const
{
	const int32 BlockingCount = ActivationGroupCounts[static_cast<uint8>(EknightAbilityActivationGroup::Exclusive_Blocking)];
	const int32 ReplaceableCount = ActivationGroupCounts[static_cast<uint8>(EknightAbilityActivationGroup::Exclusive_Replaceable)];

	switch (Group)
	{
	case EknightAbilityActivationGroup::Independent:
		return false;
	case EknightAbilityActivationGroup::Exclusive_Blocking:
		return BlockingCount > 0;
	case EknightAbilityActivationGroup::Exclusive_Replaceable:
		if (BlockingCount > 0)
		{
			return true;
		}
		if (ReplaceableCount == 0 || HasMatchingGameplayTag(knightGameplayTags::Window_Cancel))
		{
			return false;
		}
		// 콤보 창: 공격만 이어서 쓸 수 있다. 나머지는 캔슬 창을 기다린다.
		return !(HasMatchingGameplayTag(knightGameplayTags::Window_Combo) && AbilityTags.HasTag(knightGameplayTags::Ability_Attack));
	default:
		checkf(false, TEXT("Invalid activation group %d"), static_cast<int32>(Group));
		return true;
	}
}

void UknightAbilitySystemComponent::CancelActivationGroupAbilities(EknightAbilityActivationGroup Group, const UknightGameplayAbility* IgnoreAbility)
{
	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.IsActive())
		{
			continue;
		}
		for (UGameplayAbility* const Instance : Spec.GetAbilityInstances())
		{
			UknightGameplayAbility* const KnightAbility = Cast<UknightGameplayAbility>(Instance);
			if (KnightAbility && KnightAbility != IgnoreAbility && KnightAbility->IsActive()
				&& KnightAbility->GetActivationGroup() == Group && KnightAbility->CanBeCanceled())
			{
				KnightAbility->CancelAbility(Spec.Handle, AbilityActorInfo.Get(), KnightAbility->GetCurrentActivationInfo(), true);
			}
		}
	}
}

void UknightAbilitySystemComponent::NotifyAbilityActivated(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability)
{
	Super::NotifyAbilityActivated(Handle, Ability);

	const UknightGameplayAbility* const KnightAbility = Cast<UknightGameplayAbility>(Ability);
	if (!KnightAbility)
	{
		return;
	}
	if (const FGameplayAbilitySpec* const Spec = FindAbilitySpecFromHandle(Handle))
	{
		// 동적 소스 태그에는 InputTag만 들어 있다 (UknightAbilitySet 참고).
		RecentlyActivatedInputTags.AppendTags(Spec->GetDynamicSpecSourceTags());
	}

	const EknightAbilityActivationGroup Group = KnightAbility->GetActivationGroup();
	ActivationGroupCounts[static_cast<uint8>(Group)]++;

	// 새 배타 어빌리티는 실행 중이던 교체 가능한 배타 어빌리티를 대신한다.
	if (Group != EknightAbilityActivationGroup::Independent)
	{
		CancelActivationGroupAbilities(EknightAbilityActivationGroup::Exclusive_Replaceable, KnightAbility);
	}

	const int32 ExclusiveCount = ActivationGroupCounts[static_cast<uint8>(EknightAbilityActivationGroup::Exclusive_Replaceable)]
		+ ActivationGroupCounts[static_cast<uint8>(EknightAbilityActivationGroup::Exclusive_Blocking)];
	if (ExclusiveCount > 1)
	{
		UE_LOG(LogknightAbility, Error, TEXT("%d exclusive abilities are active at once (latest: %s)."), ExclusiveCount, *KnightAbility->GetName());
	}
}

void UknightAbilitySystemComponent::NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled)
{
	Super::NotifyAbilityEnded(Handle, Ability, bWasCancelled);

	if (const UknightGameplayAbility* const KnightAbility = Cast<UknightGameplayAbility>(Ability))
	{
		int32& Count = ActivationGroupCounts[static_cast<uint8>(KnightAbility->GetActivationGroup())];
		if (ensure(Count > 0))
		{
			Count--;
		}
	}
}

void UknightAbilitySystemComponent::ApplyAbilityBlockAndCancelTags(const FGameplayTagContainer& AbilityTags, UGameplayAbility* RequestingAbility,
	bool bEnableBlockTags, const FGameplayTagContainer& BlockTags, bool bExecuteCancelTags, const FGameplayTagContainer& CancelTags)
{
	FGameplayTagContainer ModifiedBlockTags = BlockTags;
	FGameplayTagContainer ModifiedCancelTags = CancelTags;
	if (TagRelationshipMapping)
	{
		TagRelationshipMapping->GetAbilityTagsToBlockAndCancel(AbilityTags, &ModifiedBlockTags, &ModifiedCancelTags);
	}
	Super::ApplyAbilityBlockAndCancelTags(AbilityTags, RequestingAbility, bEnableBlockTags, ModifiedBlockTags, bExecuteCancelTags, ModifiedCancelTags);
}

void UknightAbilitySystemComponent::SetTagRelationshipMapping(UknightAbilityTagRelationshipMapping* NewMapping)
{
	TagRelationshipMapping = NewMapping;
}

void UknightAbilitySystemComponent::GetAdditionalActivationTagRequirements(const FGameplayTagContainer& AbilityTags,
	FGameplayTagContainer& OutActivationRequired, FGameplayTagContainer& OutActivationBlocked) const
{
	if (TagRelationshipMapping)
	{
		TagRelationshipMapping->GetRequiredAndBlockedActivationTags(AbilityTags, &OutActivationRequired, &OutActivationBlocked);
	}
}
