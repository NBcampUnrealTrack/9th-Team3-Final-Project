#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Engine/DataAsset.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "knightAbilitySet.generated.h"

class UAttributeSet;
class UGameplayEffect;
class UknightAbilitySystemComponent;
class UknightGameplayAbility;

USTRUCT(BlueprintType)
struct FknightAbilitySet_GameplayAbility
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category="Ability")
	TSubclassOf<UknightGameplayAbility> Ability;

	UPROPERTY(EditDefaultsOnly, Category="Ability")
	int32 AbilityLevel = 1;

	/** Leave empty for abilities activated by events or OnSpawn. */
	UPROPERTY(EditDefaultsOnly, Category="Ability", meta=(Categories="InputTag"))
	FGameplayTag InputTag;
};

USTRUCT(BlueprintType)
struct FknightAbilitySet_GameplayEffect
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category="Effect")
	TSubclassOf<UGameplayEffect> GameplayEffect;

	UPROPERTY(EditDefaultsOnly, Category="Effect")
	float EffectLevel = 1.f;
};

USTRUCT(BlueprintType)
struct FknightAbilitySet_AttributeSet
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category="Attributes")
	TSubclassOf<UAttributeSet> AttributeSet;
};

/** Everything a set granted, so it can be taken back exactly (unequipping a part, changing phase). */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightAbilitySet_GrantedHandles
{
	GENERATED_BODY()

	void AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle);
	void AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle);
	void AddAttributeSet(UAttributeSet* Set);

	void TakeFromAbilitySystem(UknightAbilitySystemComponent* ASC);

	bool IsEmpty() const { return AbilitySpecHandles.IsEmpty() && GameplayEffectHandles.IsEmpty() && GrantedAttributeSets.IsEmpty(); }

protected:
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> GameplayEffectHandles;

	UPROPERTY()
	TArray<TObjectPtr<UAttributeSet>> GrantedAttributeSets;
};

/**
 * Bundle of abilities, effects and attribute sets granted together. Used for the Knight's base kit,
 * each part and weapon (via UPartsComponent), and each monster's move set.
 */
UCLASS(BlueprintType, Const)
class KNIGHT_API UknightAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** SourceObject is stored on each ability spec (for example the part item that granted it). */
	void GiveToAbilitySystem(UknightAbilitySystemComponent* ASC, FknightAbilitySet_GrantedHandles* OutGrantedHandles, UObject* SourceObject = nullptr) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	UPROPERTY(EditDefaultsOnly, Category="Gameplay Abilities", meta=(TitleProperty=Ability))
	TArray<FknightAbilitySet_GameplayAbility> GrantedGameplayAbilities;

	UPROPERTY(EditDefaultsOnly, Category="Gameplay Effects", meta=(TitleProperty=GameplayEffect))
	TArray<FknightAbilitySet_GameplayEffect> GrantedGameplayEffects;

	UPROPERTY(EditDefaultsOnly, Category="Attribute Sets", meta=(TitleProperty=AttributeSet))
	TArray<FknightAbilitySet_AttributeSet> GrantedAttributes;
};
