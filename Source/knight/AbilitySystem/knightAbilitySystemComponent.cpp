#include "AbilitySystem/knightAbilitySystemComponent.h"

#include "AbilitySystem/knightAbilityTagRelationshipMapping.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

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

	// Held input: WhileInputActive abilities retry every frame until they activate.
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

	// Pressed this frame: running abilities receive the press, idle OnInputTriggered abilities activate.
	for (const FGameplayAbilitySpecHandle& Handle : InputPressedSpecHandles)
	{
		FGameplayAbilitySpec* const Spec = FindAbilitySpecFromHandle(Handle);
		if (!Spec || !Spec->Ability)
		{
			continue;
		}
		Spec->InputPressed = true;
		if (Spec->IsActive())
		{
			AbilitySpecInputPressed(*Spec);
		}
		else
		{
			const UknightGameplayAbility* const AbilityCDO = Cast<UknightGameplayAbility>(Spec->Ability);
			if (AbilityCDO && AbilityCDO->GetActivationPolicy() == EknightAbilityActivationPolicy::OnInputTriggered)
			{
				AbilitiesToActivate.AddUnique(Spec->Handle);
			}
		}
	}

	// Activate after processing presses so a single press never activates and then receives its own press event.
	for (const FGameplayAbilitySpecHandle& Handle : AbilitiesToActivate)
	{
		TryActivateAbility(Handle);
	}

	// Released this frame: running abilities receive the release (charge attacks end their charge here).
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
		if (Spec.Ability && !Spec.IsActive() && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
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
	// Wake UAbilityTask_WaitInputPress. All project abilities are instanced, so the primary instance holds the key.
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
	// Wake UAbilityTask_WaitInputRelease.
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
		// Combo window: only attacks may chain. Everything else waits for the cancel window.
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
		// Only InputTags live in the dynamic source tags (see UknightAbilitySet).
		RecentlyActivatedInputTags.AppendTags(Spec->GetDynamicSpecSourceTags());
	}

	const EknightAbilityActivationGroup Group = KnightAbility->GetActivationGroup();
	ActivationGroupCounts[static_cast<uint8>(Group)]++;

	// A new exclusive ability replaces whatever replaceable exclusive ability was running.
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
