#include "Monster/knightMonsterPartData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "knightMonsterPartData"

#if WITH_EDITOR
EDataValidationResult UknightMonsterPartData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	TSet<FGameplayTag> SeenTags;
	TMap<FName, FGameplayTag> BoneOwners;
	TMap<FName, FGameplayTag> ComponentTagOwners;
	for (int32 Index = 0; Index < Parts.Num(); ++Index)
	{
		const FknightMonsterPartDefinition& Part = Parts[Index];
		if (!Part.PartTag.IsValid())
		{
			Context.AddError(FText::Format(LOCTEXT("NoTag", "Parts[{0}] has no PartTag."), Index));
			Result = EDataValidationResult::Invalid;
			continue;
		}
		if (SeenTags.Contains(Part.PartTag))
		{
			Context.AddError(FText::Format(LOCTEXT("DuplicateTag", "PartTag {0} is used by more than one part."), FText::FromName(Part.PartTag.GetTagName())));
			Result = EDataValidationResult::Invalid;
		}
		SeenTags.Add(Part.PartTag);
		if (Part.CoveredByPart.IsValid() && (Part.CoveredByPart == Part.PartTag
			|| !Parts.ContainsByPredicate([&Part](const FknightMonsterPartDefinition& Other) { return Other.PartTag == Part.CoveredByPart; })))
		{
			Context.AddError(FText::Format(LOCTEXT("BadCover", "Part {0}: CoveredByPart {1} must be another part of this monster."),
				FText::FromName(Part.PartTag.GetTagName()), FText::FromName(Part.CoveredByPart.GetTagName())));
			Result = EDataValidationResult::Invalid;
		}
		if (Part.Bones.IsEmpty() && Part.ComponentTags.IsEmpty())
		{
			Context.AddWarning(FText::Format(LOCTEXT("NoBones", "Part {0} lists no bones or component tags, so no hit can resolve to it."), FText::FromName(Part.PartTag.GetTagName())));
		}
		for (const FName& ComponentTag : Part.ComponentTags)
		{
			if (const FGameplayTag* const Owner = ComponentTagOwners.Find(ComponentTag))
			{
				Context.AddError(FText::Format(LOCTEXT("SharedComponentTag", "Component tag {0} is in both {1} and {2}."),
					FText::FromName(ComponentTag), FText::FromName(Owner->GetTagName()), FText::FromName(Part.PartTag.GetTagName())));
				Result = EDataValidationResult::Invalid;
			}
			ComponentTagOwners.Add(ComponentTag, Part.PartTag);
		}
		for (const FName& Bone : Part.Bones)
		{
			if (const FGameplayTag* const Owner = BoneOwners.Find(Bone))
			{
				Context.AddError(FText::Format(LOCTEXT("SharedBone", "Bone {0} is in both {1} and {2}."),
					FText::FromName(Bone), FText::FromName(Owner->GetTagName()), FText::FromName(Part.PartTag.GetTagName())));
				Result = EDataValidationResult::Invalid;
			}
			BoneOwners.Add(Bone, Part.PartTag);
		}
	}

	// 빈 RequiredTags는 HasAll이 항상 참이라 코드에서 걸러 낸다. 그래서 비어 있으면 조합이 영원히 발동하지 않는다.
	FGameplayTagContainer BreakTags;
	for (const FknightMonsterPartDefinition& Part : Parts)
	{
		BreakTags.AppendTags(Part.GrantedTagsOnBreak);
	}
	for (int32 Index = 0; Index < BreakCombos.Num(); ++Index)
	{
		const FknightMonsterPartBreakCombo& Combo = BreakCombos[Index];
		if (Combo.RequiredTags.IsEmpty())
		{
			Context.AddError(FText::Format(LOCTEXT("ComboNoRequired", "BreakCombos[{0}] has no RequiredTags, so it never fires."), Index));
			Result = EDataValidationResult::Invalid;
		}
		if (Combo.GrantedTags.IsEmpty())
		{
			Context.AddError(FText::Format(LOCTEXT("ComboNoGranted", "BreakCombos[{0}] has no GrantedTags, so firing it does nothing."), Index));
			Result = EDataValidationResult::Invalid;
		}
		// BreakEffect GE가 붙이는 태그일 수도 있어 경고로만 남긴다.
		for (const FGameplayTag& Tag : Combo.RequiredTags)
		{
			if (!BreakTags.HasTagExact(Tag))
			{
				Context.AddWarning(FText::Format(LOCTEXT("ComboUnknownTag", "BreakCombos[{0}]: RequiredTags {1} is not in any part's GrantedTagsOnBreak."),
					Index, FText::FromName(Tag.GetTagName())));
			}
		}
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
