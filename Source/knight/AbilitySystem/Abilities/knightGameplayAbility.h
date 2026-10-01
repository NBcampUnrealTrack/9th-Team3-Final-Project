#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "knightGameplayAbility.generated.h"

class UknightAbilitySystemComponent;

/** How an ability is activated. */
UENUM(BlueprintType)
enum class EknightAbilityActivationPolicy : uint8
{
	/** Activate once when the bound input is pressed. */
	OnInputTriggered,
	/** Try to activate every frame while the bound input is held. */
	WhileInputActive,
	/** Activate as soon as the ability is granted (passives, state listeners). */
	OnSpawn
};

/** How an ability relates to other running abilities. */
UENUM(BlueprintType)
enum class EknightAbilityActivationGroup : uint8
{
	/** Runs alongside anything. Death uses this and cancels the rest through its cancel tags. */
	Independent,
	/**
	 * Exclusive. A newer exclusive ability replaces it only inside Window.Cancel, or inside Window.Combo
	 * when the newer ability is an attack. Outside the windows, the press waits in the input buffer.
	 * Attacks, guard, boost dash.
	 */
	Exclusive_Replaceable,
	/** Exclusive. Cancels running replaceable abilities and blocks every other exclusive ability (hit reactions). */
	Exclusive_Blocking,

	MAX UMETA(Hidden)
};

/** Base for every ability in the project. */
UCLASS(Abstract)
class KNIGHT_API UknightGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	EknightAbilityActivationPolicy GetActivationPolicy() const { return ActivationPolicy; }
	EknightAbilityActivationGroup GetActivationGroup() const { return ActivationGroup; }

	UFUNCTION(BlueprintCallable, Category="Knight|Ability")
	UknightAbilitySystemComponent* GetKnightAbilitySystemComponentFromActorInfo() const;

	/** Activates OnSpawn abilities. Called when granted and again when the avatar changes. */
	void TryActivateAbilityOnSpawn(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) const;

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	/** Adds the tag relationship mapping's required and blocked tags to the engine checks. */
	virtual bool DoesAbilitySatisfyTagRequirements(const UAbilitySystemComponent& AbilitySystemComponent,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Knight|Ability Activation")
	EknightAbilityActivationPolicy ActivationPolicy = EknightAbilityActivationPolicy::OnInputTriggered;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Knight|Ability Activation")
	EknightAbilityActivationGroup ActivationGroup = EknightAbilityActivationGroup::Independent;
};
