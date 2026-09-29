#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "knightInputConfig.generated.h"

class UInputAction;

USTRUCT(BlueprintType)
struct FknightInputAction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input")
	TObjectPtr<const UInputAction> InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(Categories="InputTag"))
	FGameplayTag InputTag;
};

/**
 * Maps Enhanced Input actions to InputTags.
 *
 * Native actions (move, look) are bound to C++ handlers. Ability actions are forwarded to the ASC by tag,
 * and UknightAbilitySet binds abilities to the same tags. Ability actions should have no triggers:
 * hold, charge and release timing belong to the ability, not the input action.
 */
UCLASS(BlueprintType, Const)
class KNIGHT_API UknightInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	const UInputAction* FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true) const;
	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true) const;

	const TArray<FknightInputAction>& GetNativeInputActions() const { return NativeInputActions; }
	const TArray<FknightInputAction>& GetAbilityInputActions() const { return AbilityInputActions; }

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	UPROPERTY(EditDefaultsOnly, Category="Input", meta=(TitleProperty="InputTag"))
	TArray<FknightInputAction> NativeInputActions;

	UPROPERTY(EditDefaultsOnly, Category="Input", meta=(TitleProperty="InputTag"))
	TArray<FknightInputAction> AbilityInputActions;
};
