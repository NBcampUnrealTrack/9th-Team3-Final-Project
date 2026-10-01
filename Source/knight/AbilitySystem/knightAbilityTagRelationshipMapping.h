#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "knightAbilityTagRelationshipMapping.generated.h"

/** What happens to other abilities while an ability with AbilityTag runs, and what it needs to start. */
USTRUCT()
struct FknightAbilityTagRelationship
{
	GENERATED_BODY()

	/** Matched against the ability's asset tags (exact match). */
	UPROPERTY(EditAnywhere, Category="Ability", meta=(Categories="Ability"))
	FGameplayTag AbilityTag;

	/** Abilities with these tags cannot activate while this one runs. */
	UPROPERTY(EditAnywhere, Category="Ability")
	FGameplayTagContainer AbilityTagsToBlock;

	/** Running abilities with these tags are cancelled when this one activates. */
	UPROPERTY(EditAnywhere, Category="Ability")
	FGameplayTagContainer AbilityTagsToCancel;

	/** Owner must have all of these to activate, in addition to the ability's own requirements. */
	UPROPERTY(EditAnywhere, Category="Ability")
	FGameplayTagContainer ActivationRequiredTags;

	/** Owner must have none of these to activate, in addition to the ability's own requirements. */
	UPROPERTY(EditAnywhere, Category="Ability")
	FGameplayTagContainer ActivationBlockedTags;
};

/**
 * Central table of ability interactions ("guard cancels charge", "nothing but death while staggered").
 * Keeping this in one asset stops the rules from scattering across dozens of ability defaults.
 */
UCLASS()
class KNIGHT_API UknightAbilityTagRelationshipMapping : public UDataAsset
{
	GENERATED_BODY()

public:
	void GetAbilityTagsToBlockAndCancel(const FGameplayTagContainer& AbilityTags,
		FGameplayTagContainer* OutTagsToBlock, FGameplayTagContainer* OutTagsToCancel) const;

	void GetRequiredAndBlockedActivationTags(const FGameplayTagContainer& AbilityTags,
		FGameplayTagContainer* OutActivationRequired, FGameplayTagContainer* OutActivationBlocked) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
	UPROPERTY(EditAnywhere, Category="Ability", meta=(TitleProperty="AbilityTag"))
	TArray<FknightAbilityTagRelationship> AbilityTagRelationships;
};
