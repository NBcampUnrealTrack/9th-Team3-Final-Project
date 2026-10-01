#include "AbilitySystem/knightAbilityTagRelationshipMapping.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "knightAbilityTagRelationshipMapping"

void UknightAbilityTagRelationshipMapping::GetAbilityTagsToBlockAndCancel(const FGameplayTagContainer& AbilityTags,
	FGameplayTagContainer* OutTagsToBlock, FGameplayTagContainer* OutTagsToCancel) const
{
	for (const FknightAbilityTagRelationship& Relationship : AbilityTagRelationships)
	{
		if (AbilityTags.HasTagExact(Relationship.AbilityTag))
		{
			if (OutTagsToBlock)
			{
				OutTagsToBlock->AppendTags(Relationship.AbilityTagsToBlock);
			}
			if (OutTagsToCancel)
			{
				OutTagsToCancel->AppendTags(Relationship.AbilityTagsToCancel);
			}
		}
	}
}

void UknightAbilityTagRelationshipMapping::GetRequiredAndBlockedActivationTags(const FGameplayTagContainer& AbilityTags,
	FGameplayTagContainer* OutActivationRequired, FGameplayTagContainer* OutActivationBlocked) const
{
	for (const FknightAbilityTagRelationship& Relationship : AbilityTagRelationships)
	{
		if (AbilityTags.HasTagExact(Relationship.AbilityTag))
		{
			if (OutActivationRequired)
			{
				OutActivationRequired->AppendTags(Relationship.ActivationRequiredTags);
			}
			if (OutActivationBlocked)
			{
				OutActivationBlocked->AppendTags(Relationship.ActivationBlockedTags);
			}
		}
	}
}

#if WITH_EDITOR
EDataValidationResult UknightAbilityTagRelationshipMapping::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	TSet<FGameplayTag> SeenTags;
	for (int32 Index = 0; Index < AbilityTagRelationships.Num(); ++Index)
	{
		const FGameplayTag& Tag = AbilityTagRelationships[Index].AbilityTag;
		if (!Tag.IsValid())
		{
			Context.AddError(FText::Format(LOCTEXT("EmptyTag", "Relationship {0} has no AbilityTag."), Index));
			Result = EDataValidationResult::Invalid;
		}
		else if (SeenTags.Contains(Tag))
		{
			Context.AddWarning(FText::Format(LOCTEXT("DuplicateTag", "AbilityTag {0} appears more than once; the rules are merged."),
				FText::FromName(Tag.GetTagName())));
		}
		SeenTags.Add(Tag);
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
