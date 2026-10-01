#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightAbilitySystemComponent.generated.h"

class UknightAbilityTagRelationshipMapping;

/**
 * Project ASC. Adds input-tag activation, activation groups and data-driven tag relationships.
 *
 * Input flow: the PlayerController forwards InputTag press/release here, then calls ProcessAbilityInput
 * once per frame from PostProcessInput. Abilities are matched by the InputTag stored in the spec's
 * dynamic source tags, which UknightAbilitySet writes when granting.
 */
UCLASS()
class KNIGHT_API UknightAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UknightAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

	// Input
	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	void AbilityInputTagReleased(const FGameplayTag& InputTag);
	void ProcessAbilityInput(float DeltaTime, bool bGamePaused);
	void ClearAbilityInput();

	/** Activates the first inactive ability bound to InputTag immediately. Used by the input buffer. */
	bool TryActivateAbilityByInputTag(const FGameplayTag& InputTag);

	/** True if an ability bound to InputTag activated since the last ProcessAbilityInput. Read by the input buffer. */
	bool WasInputTagActivatedThisFrame(const FGameplayTag& InputTag) const { return RecentlyActivatedInputTags.HasTagExact(InputTag); }

	// Activation groups
	/**
	 * Exclusive_Blocking abilities block every other exclusive ability.
	 * A running Exclusive_Replaceable ability can only be replaced inside Window.Cancel,
	 * or inside Window.Combo when the new ability is an Ability.Attack.
	 */
	bool IsActivationGroupBlocked(EknightAbilityActivationGroup Group, const FGameplayTagContainer& AbilityTags) const;
	void CancelActivationGroupAbilities(EknightAbilityActivationGroup Group, const UknightGameplayAbility* IgnoreAbility);

	// Tag relationships
	void SetTagRelationshipMapping(UknightAbilityTagRelationshipMapping* NewMapping);
	void GetAdditionalActivationTagRequirements(const FGameplayTagContainer& AbilityTags,
		FGameplayTagContainer& OutActivationRequired, FGameplayTagContainer& OutActivationBlocked) const;

protected:
	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;
	virtual void NotifyAbilityActivated(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability) override;
	virtual void NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled) override;
	virtual void ApplyAbilityBlockAndCancelTags(const FGameplayTagContainer& AbilityTags, UGameplayAbility* RequestingAbility,
		bool bEnableBlockTags, const FGameplayTagContainer& BlockTags, bool bExecuteCancelTags, const FGameplayTagContainer& CancelTags) override;

	void TryActivateAbilitiesOnSpawn();

	/** Optional. Assigned by the avatar from its pawn data or defaults. */
	UPROPERTY()
	TObjectPtr<UknightAbilityTagRelationshipMapping> TagRelationshipMapping;

	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;

	/** Input tags of abilities activated since ProcessAbilityInput last ran. */
	FGameplayTagContainer RecentlyActivatedInputTags;

	int32 ActivationGroupCounts[static_cast<uint8>(EknightAbilityActivationGroup::MAX)];
};
