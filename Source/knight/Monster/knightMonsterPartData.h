#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "knightMonsterPartData.generated.h"

class AknightMonsterBreakPiece;
class UGameplayEffect;
class UStaticMesh;

/** One hittable region of a monster (head, foreleg, tail) and its hitzone and break rules. */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightMonsterPartDefinition
{
	GENERATED_BODY()

	/** Identity under Monster.Part (for example Monster.Part.Wyvern.Head). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Part", meta=(Categories="Monster.Part"))
	FGameplayTag PartTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Part")
	FText DisplayName;

	/** Physics asset body bones that belong to this part. A hit's BoneName is matched against these. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Part")
	TArray<FName> Bones;

	/**
	 * Component tags that belong to this part. A hit on a component carrying one of these tags resolves to
	 * this part. For separate primitives (cube stand-ins in test maps, detachable armor meshes); bones win
	 * when a hit matches both.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Part")
	TArray<FName> ComponentTags;

	/** Hitzone multiplier (1 = neutral). Weak points are simply high values; the hit cue switches at the attack's threshold. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hitzone", meta=(ClampMin=0))
	float DamageMultiplier = 1.f;

	/** Socket or bone used as a lock-on point. None means this part is not a lock-on point. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lock On")
	FName LockOnSocket;

	/** Lock on to the first component with one of ComponentTags (its origin). For parts without a skeleton socket. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lock On")
	bool bLockOnComponent = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Break")
	bool bBreakable = false;

	/** Damage this part must take (after hitzone scaling) before it breaks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Break", meta=(EditCondition="bBreakable", ClampMin=1))
	float BreakDamage = 500.f;

	/** Remaining durability ratio (BreakDamage left / BreakDamage) at or below which the part counts as critical (near break). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Break", meta=(EditCondition="bBreakable", ClampMin=0, ClampMax=1))
	float CriticalRatio = 0.3f;

	/** Hitzone multiplier after the break (a broken horn often becomes softer). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Break", meta=(EditCondition="bBreakable", ClampMin=0))
	float BrokenDamageMultiplier = 1.f;

	/** Applied to the monster on break (weaken an attack, grant a tag the AI reacts to). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Break", meta=(EditCondition="bBreakable"))
	TSubclassOf<UGameplayEffect> BreakEffect;

	/**
	 * Loose tags added to the monster's ability system on break (for example Monster.State.Broken.ForelegL).
	 * Attacks list these in Selection.BlockedOwnerTags to stop being picked, without a GE asset per part.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Break", meta=(EditCondition="bBreakable", Categories="Monster.State.Broken"))
	FGameplayTagContainer GrantedTagsOnBreak;

	/** Bone hidden on break, including its children (tail cut, horn snapped). None hides nothing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Break", meta=(EditCondition="bBreakable"))
	FName HideBoneOnBreak;

	/** Hide the part's tagged components and turn off their collision on break. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Break", meta=(EditCondition="bBreakable"))
	bool bHideComponentsOnBreak = false;

	/**
	 * 파괴되면 HideBoneOnBreak 자리에서 떨어지는 조각(잘린 꼬리). 원본 메시의 기준 자세에서 잘라 낸 메시로, 피벗은 그 뼈의 관절,
	 * 크기는 원본 메시와 같아야 한다. 비우면 떨어뜨리지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Break", meta=(EditCondition="bBreakable"))
	TObjectPtr<UStaticMesh> BreakPieceMesh;

	/** 조각을 만드는 액터. 남는 시간, 튕겨 나가는 속도, 절단면이 식는 시간은 이 클래스(또는 Blueprint 자식)에서 조정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Break", meta=(EditCondition="bBreakable"))
	TSubclassOf<AknightMonsterBreakPiece> BreakPieceClass;
};

/** 여러 부위가 모두 부서졌을 때만 붙는 태그(두 앞발이 다 부서져야 봉인되는 공격 등). */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightMonsterPartBreakCombo
{
	GENERATED_BODY()

	/** 이 태그가 몬스터에 모두 있으면(보통 각 부위의 GrantedTagsOnBreak) GrantedTags를 붙인다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Break", meta=(Categories="Monster.State.Broken"))
	FGameplayTagContainer RequiredTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Break", meta=(Categories="Monster.State.Broken"))
	FGameplayTagContainer GrantedTags;
};

/** Every part of one monster species. Shared by all instances of that monster. */
UCLASS(BlueprintType, Const)
class KNIGHT_API UknightMonsterPartData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Parts", meta=(TitleProperty="PartTag"))
	TArray<FknightMonsterPartDefinition> Parts;

	/** 부위가 부서질 때마다 확인한다. 부위 하나의 봉인은 GrantedTagsOnBreak로 충분하다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Parts")
	TArray<FknightMonsterPartBreakCombo> BreakCombos;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
