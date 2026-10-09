#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "knightMonsterDefinition.generated.h"

class UGameplayEffect;
class UknightAbilitySet;
class UknightAbilityTagRelationshipMapping;
class UknightMonsterPartData;

/**
 * Everything that defines a monster species, separate from its Blueprint (mesh, anim BP, StateTree).
 * Starting attributes (Health, AttackPower, Poise, Rage limits) come from an init effect in an ability set.
 */
UCLASS(BlueprintType, Const)
class KNIGHT_API UknightMonsterDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster")
	FText DisplayName;

	/** Attack moves, reactions and the attribute init effect. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Abilities")
	TArray<TObjectPtr<const UknightAbilitySet>> AbilitySets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Abilities")
	TObjectPtr<UknightAbilityTagRelationshipMapping> TagRelationshipMapping;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Parts")
	TObjectPtr<const UknightMonsterPartData> PartData;

	/** Applied when Rage fills: grants State.Monster.Enraged for a duration and resets Rage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="States")
	TSubclassOf<UGameplayEffect> EnrageEffect;

	/** Applied when Stamina runs out: grants State.Monster.Exhausted for a duration and restores Stamina. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="States")
	TSubclassOf<UGameplayEffect> ExhaustEffect;

	/**
	 * 퍼펙트 가드를 당할 때마다 깎이는 경직도. 최대 경직도에 대한 비율(0.25 = 4번에 다운). 0이면 깎지 않는다.
	 * 일반 타격의 경직도 피해와 같은 게이지를 쓰므로 섞어서 쌓인다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="States", meta=(ClampMin=0, ClampMax=1))
	float ParriedPoiseDamageRatio = 0.f;

	/** Rough radius for lock-on camera framing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Targeting", meta=(ClampMin=0, Units="cm"))
	float TargetRadius = 600.f;

	/** Lock-on point when PartData defines none. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Targeting")
	FName FallbackLockOnSocket = TEXT("spine_03");
};
