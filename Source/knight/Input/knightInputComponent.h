#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "Input/knightInputConfig.h"
#include "knightInputComponent.generated.h"

/**
 * Enhanced input component that binds by InputTag through a UknightInputConfig.
 * Set as DefaultInputComponentClass in DefaultInput.ini; it is a drop-in UEnhancedInputComponent.
 */
UCLASS(Config=Input)
class KNIGHT_API UknightInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()

public:
	template<class UserClass, typename FuncType>
	void BindNativeAction(const UknightInputConfig* InputConfig, const FGameplayTag& InputTag, ETriggerEvent TriggerEvent,
		UserClass* Object, FuncType Func, bool bLogIfNotFound = true);

	/**
	 * Binds every ability action. PressedFunc(FGameplayTag) fires once on press (Started);
	 * ReleasedFunc(FGameplayTag) fires on release or cancel.
	 */
	template<class UserClass, typename PressedFuncType, typename ReleasedFuncType>
	void BindAbilityActions(const UknightInputConfig* InputConfig, UserClass* Object,
		PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, TArray<uint32>& OutBindHandles);

	void RemoveBinds(TArray<uint32>& BindHandles);
};

template<class UserClass, typename FuncType>
void UknightInputComponent::BindNativeAction(const UknightInputConfig* InputConfig, const FGameplayTag& InputTag, ETriggerEvent TriggerEvent,
	UserClass* Object, FuncType Func, bool bLogIfNotFound)
{
	check(InputConfig);
	if (const UInputAction* const Action = InputConfig->FindNativeInputActionForTag(InputTag, bLogIfNotFound))
	{
		BindAction(Action, TriggerEvent, Object, Func);
	}
}

template<class UserClass, typename PressedFuncType, typename ReleasedFuncType>
void UknightInputComponent::BindAbilityActions(const UknightInputConfig* InputConfig, UserClass* Object,
	PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, TArray<uint32>& OutBindHandles)
{
	check(InputConfig);
	for (const FknightInputAction& Action : InputConfig->GetAbilityInputActions())
	{
		if (!Action.InputAction || !Action.InputTag.IsValid())
		{
			continue;
		}
		if (PressedFunc)
		{
			OutBindHandles.Add(BindAction(Action.InputAction, ETriggerEvent::Started, Object, PressedFunc, Action.InputTag).GetHandle());
		}
		if (ReleasedFunc)
		{
			OutBindHandles.Add(BindAction(Action.InputAction, ETriggerEvent::Completed, Object, ReleasedFunc, Action.InputTag).GetHandle());
			OutBindHandles.Add(BindAction(Action.InputAction, ETriggerEvent::Canceled, Object, ReleasedFunc, Action.InputTag).GetHandle());
		}
	}
}
