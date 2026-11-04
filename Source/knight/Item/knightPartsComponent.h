#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/knightAbilitySet.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "knightPartsComponent.generated.h"

class UGameplayEffect;
class UMechBodyRegionSet;
class UMeshComponent;
class UNiagaraComponent;
class USkeletalMeshComponent;
class UknightPartItemDefinition;
class UknightWeaponItemDefinition;
class UknightLoadoutDefinition;
class UknightProgressionSubsystem;
struct FknightPartStatModifier;

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

	/** 파츠 메시 컴포넌트. 스켈레탈 또는 스태틱(2026-10-10) 메시 컴포넌트. */
	UPROPERTY()
	TObjectPtr<UMeshComponent> MeshComponent;

	UPROPERTY()
	FknightAbilitySet_GrantedHandles GrantedHandles;

	/** 파츠 능력치(StatModifiers)로 만든 GE를 적용한 핸들. 해제할 때 이걸로 회수한다. */
	FActiveGameplayEffectHandle StatEffectHandle;
	FActiveGameplayEffectHandle UpgradeAttackEffectHandle;
	UPROPERTY(Transient)
	TObjectPtr<UGameplayEffect> UpgradeAttackEffect;

	/** (2026-10-27) 무기 상시 이펙트(UknightWeaponItemDefinition::WeaponAuraEffect). 무기 메시에 붙어 있고 해제할 때 지운다. */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> AuraComponent;
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
 *
 * 원본 부위 교체 (2026-10-13): BodyRegions(원본을 부위별 메시로 나눈 세트)를 넣으면 나이트 본체 대신 부위 메시가 그려지고,
 * 파츠 Attachment의 Replace Regions에 든 부위는 그 파츠를 끼는 동안 숨는다. 본체는 뼈 계산·충돌·피격 판정만 한다.
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightPartsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UknightPartsComponent();

	/** Equips the saved loadout. Called by the Knight once its ability system is initialized. */
	void ApplyLoadout();

	/**
	 * (2026-10-26) 저장된 장착(저장에 없는 칸은 기본 장착)과 다른 슬롯만 다시 입거나 벗는다. ApplyLoadout 뒤에 쓴다.
	 * 메뉴의 나이트(AKnightMenuKnight)가 정비 화면에서 바꾼 장비를 따라갈 때 부른다. 바뀐 슬롯이 있으면 true.
	 */
	bool SyncLoadoutWithSave();

	/** Equips a part into its slot, replacing what was there. 적재 한도를 넘으면 false(장착 안 함). */
	UFUNCTION(BlueprintCallable, Category="Knight|Parts")
	bool EquipPart(const UknightPartItemDefinition* Part, int32 UpgradeLevel);

	UFUNCTION(BlueprintCallable, Category="Knight|Parts")
	void UnequipSlot(UPARAM(meta=(Categories="Equipment.Slot")) FGameplayTag SlotTag);
	/** Refresh only the attack granted by the currently equipped weapon after an upgrade transaction. */
	void RefreshWeaponUpgrade(const UknightWeaponItemDefinition* Weapon, int32 NewLevel);

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

	/** 기본 장착. DefaultLoadoutAsset이 있으면 그 목록, 없으면 DefaultLoadout 배열. (2026-10-20) */
	const TArray<TObjectPtr<const UknightPartItemDefinition>>& GetDefaultLoadout() const;

	/**
	 * Meshes of every equipped part (for damage visuals, dissolves, outlines). 스켈레탈·스태틱 메시 컴포넌트가 섞여 있다.
	 * (2026-10-13) 원본 부위 메시(BodyRegions)도 넣는다. 본체 대신 그려지는 메시라 본체와 같은 효과를 받아야 한다.
	 */
	void GetPartMeshes(TArray<UMeshComponent*>& OutMeshes) const;

	/**
	 * 몸의 한 부위를 그리는 메시: RegionFlags(EMechBodyRegion 비트)에 걸리는 원본 부위 메시(BodyRegions)와,
	 * SlotTag 슬롯에 장착한 파츠 또는 그 부위를 덮는(Replace Regions) 파츠의 메시. 부위 파괴 연출(그을림)에 쓴다.
	 * 원본 부위 메시가 없으면(BodyRegions 미설정) 본체는 몸 전체라 넣지 않는다.
	 */
	void GetMeshesForBodyRegion(int32 RegionFlags, FGameplayTag SlotTag, TArray<UMeshComponent*>& OutMeshes) const;

	/** 원본 부위 메시(BodyRegions)로 몸을 그리고 있는지. 아니면 본체 메시 하나가 몸 전체를 그린다. */
	bool HasBodyRegionMeshes() const { return !BodyRegionMeshes.IsEmpty(); }

	/**
	 * (2026-10-26) 무기 메시를 몸 메시의 소켓으로 옮긴다(검 뽑기·넣기 노티파이, 자동으로 검 넣기).
	 * 그 소켓이 장착한 무기 파츠의 붙일 소켓(Attachment.AttachSocket, 보통 손 weapon_r)이면 워크벤치에서 맞춘 위치·회전·크기로 붙이고,
	 * 다른 소켓(등 weapon_back)이면 지금처럼 소켓에 딱 붙인다. 예전에는 뽑을 때도 딱 붙여서 워크벤치 값이 사라졌다.
	 */
	static void AttachWeaponToSocket(UMeshComponent* WeaponMesh, USkeletalMeshComponent* Body, FName Socket);

	UPROPERTY(BlueprintAssignable, Category="Knight|Parts")
	FknightEquipmentChanged OnEquipmentChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * (2026-10-20) 기본 장착 에셋. 넣으면 아래 DefaultLoadout 배열 대신 이 목록을 입는다.
	 * 정비 카탈로그(DA_MaintenanceCatalog의 Default Loadout)에도 같은 에셋을 넣어야 정비소 화면과 실제 장착이 같아진다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Parts")
	TObjectPtr<const UknightLoadoutDefinition> DefaultLoadoutAsset;

	/**
	 * Parts used for slots the save has nothing for (a new game's starter mech).
	 * (2026-10-20) DefaultLoadoutAsset이 비어 있을 때만 쓴다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Parts")
	TArray<TObjectPtr<const UknightPartItemDefinition>> DefaultLoadout;

	/**
	 * 로드아웃을 입힐 때 도는 슬롯 목록 (2026-10-07, 전에는 코드에 6개로 고정).
	 * 기본: 머리·몸통·팔·다리 + 부스터·발전기·외장 + 무기. 기존 Equipment.Slot.Backpack은 부스터 슬롯으로 대체되어 빠졌다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Parts", meta=(Categories="Equipment.Slot"))
	TArray<FGameplayTag> LoadoutSlots;

	/**
	 * (2026-10-13) 원본 메카를 부위별 메시로 나눈 세트. Mech Parts Workbench 목록 우클릭 '원본을 부위별로 나누기'로 만든다.
	 * 넣으면 본체 대신 부위 메시가 보이고, 파츠 Attachment의 Replace Regions에 든 부위는 그 파츠를 끼는 동안 숨는다.
	 * 비우면 예전처럼 본체가 그대로 보이고 Replace Regions는 무시된다. 세트의 Source Mesh가 본체 메시와 다르면 쓰지 않는다(경고).
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Parts")
	TObjectPtr<const UMechBodyRegionSet> BodyRegions;

private:
	/** (2026-10-13) BodyRegions의 부위 메시를 한 번 만들어 본체에 붙이고 본체는 숨긴다. ApplyLoadout이 부른다. */
	void CreateBodyRegionMeshes();

	/** (2026-10-13) 장착한 파츠들의 Replace Regions를 합쳐 부위 메시를 켜고 끈다. 장착·해제 때마다 부른다. */
	void RefreshBodyRegionVisibility();

	/** 적재 한도 확인 없이 장착한다. EquipPart(확인 후)와 ApplyLoadout(확인 안 함)이 쓴다. */
	bool EquipPartUnchecked(const UknightPartItemDefinition* Part, int32 UpgradeLevel);

	/** (2026-10-26) 이 슬롯에 입을 파츠: 저장된 장착이 우선, 저장에 없고 일부러 비운 칸이 아니면 기본 장착(ApplyLoadout·SyncLoadoutWithSave 공용). */
	const UknightPartItemDefinition* GetLoadoutPartForSlot(const UknightProgressionSubsystem* Progression, FGameplayTag Slot) const;

	/** 장착 중량 합을 메카 Weight 스탯 기본값에 반영한다. Weight 스탯이 없는 소유자는 아무것도 하지 않는다. */
	void UpdateWeightAttribute();

	/** 파츠 능력치 GE. 파츠마다 처음 한 번 만들고 재사용한다. 능력치가 없는 파츠는 null. */
	const UGameplayEffect* GetStatEffect(const UknightPartItemDefinition* Part);
	void ApplyWeaponUpgradeAttack(FknightEquippedPart& Part, const UknightWeaponItemDefinition* Weapon);

	void RemovePart(FknightEquippedPart& Part);
	void RefreshSetBonuses();
	const UGameplayEffect* GetSetStatEffect(const FString& Key, const TArray<FknightPartStatModifier>& Modifiers);

	UPROPERTY(Transient)
	TMap<FGameplayTag, FknightEquippedPart> EquippedParts;

	/** 파츠 정의 → 그 능력치로 만든 GE (에셋 파일 없이 메모리에만 있다). */
	UPROPERTY(Transient)
	TMap<TObjectPtr<const UknightPartItemDefinition>, TObjectPtr<UGameplayEffect>> StatEffects;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UGameplayEffect>> SetStatEffects;
	TArray<FActiveGameplayEffectHandle> ActiveSetHandles;

	/** (2026-10-13) 원본 부위 메시 컴포넌트와, 같은 순서의 부위 비트(EMechBodyRegion). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USkeletalMeshComponent>> BodyRegionMeshes;
	TArray<int32> BodyRegionFlags;
};
