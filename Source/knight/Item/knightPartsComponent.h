#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/knightAbilitySet.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "knightPartsComponent.generated.h"

class UGameplayEffect;
class USkeletalMeshComponent;
class UknightPartItemDefinition;

/** Part is null when the slot was emptied. Item definitions are shared assets: listeners must not modify them. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FknightEquipmentChanged, FGameplayTag, SlotTag, UknightPartItemDefinition*, Part);

/** 적재 한도 계산 (순수 함수, 테스트: Knight.Parts.LoadCapacity). */
namespace knightPartsLoad
{
	/**
	 * 지금 장착 중량 합(TotalWeight)에서 같은 슬롯의 기존 파츠(ReplacedWeight)를 빼고 새 파츠(NewWeight)를 더했을 때
	 * 적재 한도(LoadCapacity) 안이면 true. 한도와 딱 같아도 장착할 수 있다.
	 */
	KNIGHT_API bool FitsLoadCapacity(float TotalWeight, float ReplacedWeight, float NewWeight, float LoadCapacity);
}

/** One equipped part: what it is, its mesh, and what it granted. */
USTRUCT()
struct FknightEquippedPart
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<const UknightPartItemDefinition> Definition;

	UPROPERTY()
	int32 UpgradeLevel = 0;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> MeshComponent;

	UPROPERTY()
	FknightAbilitySet_GrantedHandles GrantedHandles;

	/** 파츠 능력치(StatModifiers)로 만든 GE를 적용한 핸들. 해제할 때 이걸로 회수한다. */
	FActiveGameplayEffectHandle StatEffectHandle;
};

/**
 * Equips mech parts and the weapon: spawns each part's mesh on the mech (placement from the Mech Parts
 * Workbench data), and grants the part's ability set at its upgrade level. Unequipping removes both exactly.
 * The loadout comes from the save (UknightProgressionSubsystem), falling back to DefaultLoadout per slot.
 *
 * 중량과 적재 한도 (2026-10-07, 1차 MVP 파츠 = 모델링 없이 능력치만 있는 파츠):
 *   - 장착한 파츠 Weight의 합을 메카 Weight 스탯의 기본값으로 넣는다 → 무브먼트가 중량 계수로 속도를 낮춘다.
 *   - EquipPart는 합이 LoadCapacity 스탯을 넘으면 거부한다(CanEquipPart). 해제는 항상 된다.
 *   - 세이브·기본 로드아웃을 입히는 ApplyLoadout은 거부하지 않고 경고만 남긴다(밸런스가 바뀌어도 빈 슬롯이 생기지 않게).
 *   - 다리 파츠처럼 적재 한도를 바꾸는 파츠를 갈아 끼울 때는 '지금' 한도로 판단한다. 새 다리의 한도가 더 낮아
 *     장착 뒤 한도를 넘을 수 있는데, 그때 중량 계수는 최저값에서 멈춘다(무브먼트에서 0~1로 자름).
 * 슬롯 목록은 LoadoutSlots(BP에서 수정 가능): 필수 4부위 + 내장 3종(부스터·발전기·외장) + 무기.
 *
 * 파츠 능력치 (2026-10-07): 파츠 정의의 StatModifiers로 무한 지속 GE를 메모리에서 만들어(파츠마다 한 번, 재사용)
 * 장착할 때 적용하고 해제할 때 회수한다. 그래서 능력치만 있는 파츠는 파츠 정의 파일 하나면 된다.
 * AbilitySet은 스킬·특수 효과가 있는 파츠만 쓰고, 둘 다 있으면 둘 다 적용된다.
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightPartsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UknightPartsComponent();

	/** Equips the saved loadout. Called by the Knight once its ability system is initialized. */
	void ApplyLoadout();

	/** Equips a part into its slot, replacing what was there. 적재 한도를 넘으면 false(장착 안 함). */
	UFUNCTION(BlueprintCallable, Category="Knight|Parts")
	bool EquipPart(const UknightPartItemDefinition* Part, int32 UpgradeLevel);

	UFUNCTION(BlueprintCallable, Category="Knight|Parts")
	void UnequipSlot(UPARAM(meta=(Categories="Equipment.Slot")) FGameplayTag SlotTag);

	const UknightPartItemDefinition* GetEquippedPart(FGameplayTag SlotTag) const;

	/** 같은 슬롯의 파츠를 이것으로 바꿔도 적재 한도 안인지. 적재 한도 스탯이 없는 소유자는 제한 없음. */
	UFUNCTION(BlueprintPure, Category="Knight|Parts")
	bool CanEquipPart(const UknightPartItemDefinition* Part) const;

	/** 장착한 파츠 중량의 합. */
	UFUNCTION(BlueprintPure, Category="Knight|Parts")
	float GetTotalWeight() const;

	/** 소유자의 적재 한도(LoadCapacity 스탯). 메카 속성 세트가 없으면 -1(제한 없음). */
	UFUNCTION(BlueprintPure, Category="Knight|Parts")
	float GetLoadCapacity() const;

	const TArray<FGameplayTag>& GetLoadoutSlots() const { return LoadoutSlots; }

	/** Meshes of every equipped part (for damage visuals, dissolves, outlines). */
	void GetPartMeshes(TArray<USkeletalMeshComponent*>& OutMeshes) const;

	UPROPERTY(BlueprintAssignable, Category="Knight|Parts")
	FknightEquipmentChanged OnEquipmentChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Parts used for slots the save has nothing for (a new game's starter mech). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Parts")
	TArray<TObjectPtr<const UknightPartItemDefinition>> DefaultLoadout;

	/**
	 * 로드아웃을 입힐 때 도는 슬롯 목록 (2026-10-07, 전에는 코드에 6개로 고정).
	 * 기본: 머리·몸통·팔·다리 + 부스터·발전기·외장 + 무기. 기존 Equipment.Slot.Backpack은 부스터 슬롯으로 대체되어 빠졌다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Parts", meta=(Categories="Equipment.Slot"))
	TArray<FGameplayTag> LoadoutSlots;

private:
	/** 적재 한도 확인 없이 장착한다. EquipPart(확인 후)와 ApplyLoadout(확인 안 함)이 쓴다. */
	bool EquipPartUnchecked(const UknightPartItemDefinition* Part, int32 UpgradeLevel);

	/** 장착 중량 합을 메카 Weight 스탯 기본값에 반영한다. Weight 스탯이 없는 소유자는 아무것도 하지 않는다. */
	void UpdateWeightAttribute();

	/** 파츠 능력치 GE. 파츠마다 처음 한 번 만들고 재사용한다. 능력치가 없는 파츠는 null. */
	const UGameplayEffect* GetStatEffect(const UknightPartItemDefinition* Part);

	void RemovePart(FknightEquippedPart& Part);

	UPROPERTY(Transient)
	TMap<FGameplayTag, FknightEquippedPart> EquippedParts;

	/** 파츠 정의 → 그 능력치로 만든 GE (에셋 파일 없이 메모리에만 있다). */
	UPROPERTY(Transient)
	TMap<TObjectPtr<const UknightPartItemDefinition>, TObjectPtr<UGameplayEffect>> StatEffects;
};
