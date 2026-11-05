#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "knightMonsterPartData.generated.h"

class AknightMonsterBreakPiece;
class UGameplayEffect;
class USoundBase;
class UStaticMesh;
class USkeletalMesh;

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

	/**
	 * 비어 있지 않으면 몬스터에 이 태그 중 하나가 있을 때만 이 부위로 맞는다(그로기 중에만 드러나는 약점).
	 * 그 밖에는 CoveredByPart로 맞고, 이 부위의 배율과 내구도는 쓰지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hitzone", meta=(Categories="State"))
	FGameplayTagContainer ExposedTags;

	/** ExposedTags가 없을 때 대신 맞는 부위(약점을 덮고 있는 몸통 등). 비워 두면 부위 없이 맞는다(배율 1). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hitzone", meta=(Categories="Monster.Part", EditCondition="!ExposedTags.IsEmpty()"))
	FGameplayTag CoveredByPart;

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

	/** 부서지는 순간 부위 자리에서 한 번 재생한다(잔향 가슴 약점). 비우면 소리 없음. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Break", meta=(EditCondition="bBreakable"))
	TObjectPtr<USoundBase> BreakSound;

	/** 부서지는 순간 부위 자리에서 화면에 퍼지는 충격파 세기(1 = 기본, 0 = 없음). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Break", meta=(EditCondition="bBreakable", ClampMin=0, ClampMax=3))
	float BreakScreenShockwave = 1.f;

	/** 떨어진 조각(BreakPieceMesh)이 처음 땅에 닿을 때 한 번 재생한다. 비우면 소리 없음. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Break", meta=(EditCondition="bBreakable"))
	TObjectPtr<USoundBase> BreakPieceLandSound;
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

	/**
	 * 부위 와이어프레임 셸 메시. 몬스터 메시를 단순화해 파괴 가능한 부위만 남긴 스켈레탈 메시로, 몬스터 메시를 리더 포즈로 따라간다.
	 * knightPartShellBaker.BakePartShell로 굽는다. 비어 있으면 셸을 그리지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Part Shell")
	TSoftObjectPtr<USkeletalMesh> PartShellMesh;

	/** 셸 메시의 부위 순서. 셸 메시 UV1.x가 이 배열의 인덱스다(최대 8개). 굽기가 채운다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Part Shell", meta=(Categories="Monster.Part"))
	TArray<FGameplayTag> PartShellParts;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
