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
		if (Part.Bones.IsEmpty())
		{
			Context.AddWarning(FText::Format(LOCTEXT("NoBones", "Part {0} lists no bones, so no hit can resolve to it."), FText::FromName(Part.PartTag.GetTagName())));
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
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
