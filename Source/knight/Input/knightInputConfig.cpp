#include "Input/knightInputConfig.h"

#include "InputAction.h"
#include "knight.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "knightInputConfig"

namespace
{
	const UInputAction* FindActionForTag(const TArray<FknightInputAction>& Actions, const FGameplayTag& InputTag)
	{
		for (const FknightInputAction& Action : Actions)
		{
			if (Action.InputAction && Action.InputTag == InputTag)
			{
				return Action.InputAction;
			}
		}
		return nullptr;
	}
}

const UInputAction* UknightInputConfig::FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	const UInputAction* const Found = FindActionForTag(NativeInputActions, InputTag);
	if (!Found && bLogNotFound)
	{
		UE_LOG(Logknight, Error, TEXT("%s has no native input action for tag %s."), *GetNameSafe(this), *InputTag.ToString());
	}
	return Found;
}

const UInputAction* UknightInputConfig::FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	const UInputAction* const Found = FindActionForTag(AbilityInputActions, InputTag);
	if (!Found && bLogNotFound)
	{
		UE_LOG(Logknight, Error, TEXT("%s has no ability input action for tag %s."), *GetNameSafe(this), *InputTag.ToString());
	}
	return Found;
}

#if WITH_EDITOR
EDataValidationResult UknightInputConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	TSet<FGameplayTag> SeenTags;
	auto Validate = [&](const TArray<FknightInputAction>& Actions, const FText& ListName)
	{
		for (int32 Index = 0; Index < Actions.Num(); ++Index)
		{
			const FknightInputAction& Action = Actions[Index];
			if (!Action.InputAction || !Action.InputTag.IsValid())
			{
				Context.AddError(FText::Format(LOCTEXT("Incomplete", "{0}[{1}] needs both an InputAction and an InputTag."), ListName, Index));
				Result = EDataValidationResult::Invalid;
				continue;
			}
			if (SeenTags.Contains(Action.InputTag))
			{
				Context.AddError(FText::Format(LOCTEXT("DuplicateTag", "{0}[{1}] reuses InputTag {2}."), ListName, Index,
					FText::FromName(Action.InputTag.GetTagName())));
				Result = EDataValidationResult::Invalid;
			}
			SeenTags.Add(Action.InputTag);
		}
	};
	Validate(NativeInputActions, LOCTEXT("Native", "NativeInputActions"));
	Validate(AbilityInputActions, LOCTEXT("Ability", "AbilityInputActions"));
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
