#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "MechPartAttachment.h"
#include "knightItemDefinition.h"
#include "knightItemDefinitions.generated.h"

class USkeletalMesh;
class UStaticMesh;
class UknightAbilitySet;
class UknightMonsterDefinition;

/** Items and amounts consumed by one upgrade step. */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightItemCost
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cost")
	TObjectPtr<const UknightItemDefinition> Item;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cost", meta=(ClampMin=1))
	int32 Count = 1;
};

USTRUCT(BlueprintType)
struct KNIGHT_API FknightUpgradeStep
{
	GENERATED_BODY()

	/** Target level must equal this step's one-based index. Weapon upgrades use all 15 steps. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Upgrade", meta=(ClampMin=1, ClampMax=15))
	int32 TargetLevel = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Upgrade", meta=(ClampMin=0))
	int64 MoneyCost = 0;

	/** 0..1; only this base rate is shown to players. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Upgrade", meta=(ClampMin=0, ClampMax=1))
	float BaseSuccessChance = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Upgrade", meta=(ClampMin=0, ClampMax=1))
	float FailureBonusPerAttempt = 0.05f;

	/** Flat weapon AttackPower added on success; accumulated through the current level. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Upgrade", meta=(ClampMin=0))
	float AttackIncrease = 0.f;

	/** Success-only gate materials. Empty for ordinary levels. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Upgrade", meta=(TitleProperty="Item"))
	TArray<FknightItemCost> Costs;
};

/**
 * 파츠 능력치 한 줄 (2026-10-07): 어떤 스탯을 얼마나 바꾸는지.
 * 장착하면 UknightPartsComponent가 파츠의 목록으로 무한 지속 GE를 메모리에서 만들어 적용하고, 해제하면 회수한다.
 * 능력치만 있는 파츠는 이것만 채우면 되고 GE·AbilitySet 에셋을 따로 만들지 않는다.
 */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightPartStatModifier
{
	GENERATED_BODY()

	/** 바꿀 스탯 (knightCoreAttributeSet·knightAttributeSet의 속성). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stat")
	FGameplayAttribute Attribute;

	/** 계산 방식. 대부분 Add (Base) = 기본값에 더하기. 속도처럼 배율(기본 1.0)인 스탯도 +0.1처럼 더한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stat")
	TEnumAsByte<EGameplayModOp::Type> ModifierOp = EGameplayModOp::AddBase;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stat")
	float Value = 0.f;
};

/** Monster material (horn, scale, core). Obtained from raid rewards, spent on upgrades. */
UCLASS(BlueprintType, Const)
class KNIGHT_API UknightMaterialItemDefinition : public UknightItemDefinition
{
	GENERATED_BODY()

public:
	UknightMaterialItemDefinition();

	/** Inventory name color only. Rarity 1 is common; rarity 2+ is rare. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Material")
	bool bHardModeExclusive = false;

	/** Explicit per-item sale policy. A new material is not sellable until its price is authored. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Material|Sale")
	bool bSellable = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Material|Sale", meta=(ClampMin=0))
	int32 SellPrice = 0;

	/** Monster that drops it, for the item description. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Material")
	TObjectPtr<const UknightMonsterDefinition> SourceMonster;
};

/** Socket consumable/data item. Socket effects are intentionally outside the phase-one inventory. */
UCLASS(BlueprintType, Const)
class KNIGHT_API UknightSocketItemDefinition : public UknightItemDefinition
{
	GENERATED_BODY()
public:
	UknightSocketItemDefinition();
};

/**
 * Mech part: a mesh placed by the Mech Parts Workbench, and the stats and abilities it grants.
 * Upgrading raises the level passed to the ability set, so stat effects scale through curve tables.
 */
UCLASS(BlueprintType, Const)
class KNIGHT_API UknightPartItemDefinition : public UknightItemDefinition
{
	GENERATED_BODY()

public:
	UknightPartItemDefinition();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** Highest upgrade level (the number of upgrade steps). */
	int32 GetMaxUpgradeLevel() const { return UpgradeSteps.Num(); }

	/**
	 * 장착 부위. (2026-10-11) AssetRegistrySearchable: 값이 에셋 목록 정보에 저장되어, Mech Parts Workbench가
	 * 파츠를 열지 않고 부위별로 걸러 보여 준다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AssetRegistrySearchable, Category="Part", meta=(Categories="Equipment.Slot"))
	FGameplayTag SlotTag;

	/** Exact set identity. Only Head, Torso, Arms, Legs and Weapon count toward set tiers. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Part", meta=(Categories="Equipment.Set"))
	FGameplayTag SetTag;

	/**
	 * 파츠 중량 (2026-10-07). 장착한 파츠 중량의 합이 메카의 Weight 스탯이 되어 이동·퀵스텝 속도를 낮추고,
	 * 합이 적재 한도(LoadCapacity 스탯)를 넘는 장착은 거부된다 (UknightPartsComponent::CanEquipPart).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Part", meta=(ClampMin=0))
	float Weight = 0.f;

	/** 스켈레탈 메시 파츠(SK_). 나이트 뼈대를 따라 변형되게 붙일 수 있다(Attachment LeaderPose). StaticMesh보다 먼저 쓴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Part")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	/**
	 * 스태틱 메시 파츠(SM_, 2026-10-10). 팀원이 모델링한 파츠는 대부분 이것이다. Mesh가 비어 있을 때 쓴다.
	 * 뼈대가 없어서 Attachment는 Socket 모드로(붙일 뼈·소켓 + 위치·회전·크기) 지정한다. 값은 Mech Parts Workbench에서 맞춘다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Part")
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	/** Written by the Mech Parts Workbench. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Part")
	FMechPartAttachment Attachment;

	/** Tags added to the spawned mesh component. "Weapon" lets melee trace notifies find the weapon mesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Part")
	TArray<FName> MeshComponentTags;

	/**
	 * 장착하는 동안의 능력치 (2026-10-07). 능력치는 여기에 적고, 아래 AbilitySet은 스킬·특수 효과가 있는 파츠만 쓴다.
	 * 무기 강화 공격력은 이 기본 목록과 별도로 장착·미리보기 양쪽에 합성한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Part", meta=(TitleProperty="Attribute"))
	TArray<FknightPartStatModifier> StatModifiers;

	/** Stat effects and abilities while equipped. Granted at level 1 + upgrade level. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Part")
	TObjectPtr<const UknightAbilitySet> AbilitySet;

	/** UpgradeSteps[N] is the cost of going from level N to N + 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Upgrade")
	TArray<FknightUpgradeStep> UpgradeSteps;
};

/** The greatsword and its variants. A part in the weapon slot whose mesh carries the "Weapon" tag. */
UCLASS(BlueprintType, Const)
class KNIGHT_API UknightWeaponItemDefinition : public UknightPartItemDefinition
{
	GENERATED_BODY()

public:
	UknightWeaponItemDefinition();
	float GetUpgradeAttackBonus(int32 Level) const;
	float GetWeaponAttackAtLevel(int32 Level) const;
};
