#include "Item/knightPartsComponent.h"

#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "AbilitySystem/knightAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "Item/knightItemDefinitions.h"
#include "Item/knightEquipmentData.h"
#include "MechBodyRegionSet.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Progression/knightProgressionSubsystem.h"
#include "System/knightAssetManager.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

bool knightPartsLoad::FitsLoadCapacity(float TotalWeight, float ReplacedWeight, float NewWeight, float LoadCapacity)
{
	// 부동소수 오차로 '딱 한도'인 조합이 거부되지 않게 아주 작은 여유를 둔다.
	return TotalWeight - ReplacedWeight + NewWeight <= LoadCapacity + KINDA_SMALL_NUMBER;
}

UknightPartsComponent::UknightPartsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 필수 4부위 → 내장 3종 → 무기 순서로 입힌다.
	LoadoutSlots = {
		knightGameplayTags::Equipment_Slot_Head, knightGameplayTags::Equipment_Slot_Torso,
		knightGameplayTags::Equipment_Slot_Arms, knightGameplayTags::Equipment_Slot_Legs,
		knightGameplayTags::Equipment_Slot_Booster, knightGameplayTags::Equipment_Slot_Generator,
		knightGameplayTags::Equipment_Slot_Exterior, knightGameplayTags::Equipment_Slot_Weapon
	};
}

void UknightPartsComponent::ApplyLoadout()
{
	const UGameInstance* const GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UknightProgressionSubsystem* const Progression = GameInstance ? GameInstance->GetSubsystem<UknightProgressionSubsystem>() : nullptr;

	// 부위 메시를 먼저 만들어 둬야 아래 장착에서 Replace Regions가 바로 반영되고, 피격 표시도 부위 메시까지 칠한다.
	CreateBodyRegionMeshes();

	for (const FGameplayTag& Slot : LoadoutSlots)
	{
		const UknightPartItemDefinition* const Part = GetLoadoutPartForSlot(Progression, Slot);
		if (Part)
		{
			// 저장된 로드아웃은 적재 한도로 거부하지 않는다(아래에서 경고만).
			EquipPartUnchecked(Part, Progression ? Progression->GetUpgradeLevel(Part) : 0);
		}
	}

	const float LoadCapacity = GetLoadCapacity();
	if (LoadCapacity >= 0.f && GetTotalWeight() > LoadCapacity + KINDA_SMALL_NUMBER)
	{
		UE_LOG(Logknight, Warning, TEXT("%s: loadout weight %.1f exceeds load capacity %.1f."), *GetNameSafe(GetOwner()), GetTotalWeight(), LoadCapacity);
	}
}

const UknightPartItemDefinition* UknightPartsComponent::GetLoadoutPartForSlot(const UknightProgressionSubsystem* Progression, FGameplayTag Slot) const
{
	// The saved choice wins; the default fills slots the save has nothing for.
	const UknightPartItemDefinition* Part = Progression ? Progression->GetEquippedPart(Slot) : nullptr;
	if (!Part && !(Progression && Progression->IsSlotExplicitlyEmpty(Slot)))
	{
		const TObjectPtr<const UknightPartItemDefinition>* const Default = GetDefaultLoadout().FindByPredicate(
			[&Slot](const UknightPartItemDefinition* Candidate) { return Candidate && Candidate->SlotTag == Slot; });
		Part = Default ? Default->Get() : nullptr;
	}
	return Part;
}

bool UknightPartsComponent::SyncLoadoutWithSave()
{
	const UGameInstance* const GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UknightProgressionSubsystem* const Progression = GameInstance ? GameInstance->GetSubsystem<UknightProgressionSubsystem>() : nullptr;
	bool bChanged = false;
	for (const FGameplayTag& Slot : LoadoutSlots)
	{
		const UknightPartItemDefinition* const Wanted = GetLoadoutPartForSlot(Progression, Slot);
		if (GetEquippedPart(Slot) == Wanted)
		{
			// A different profile can keep the same default weapon but have another
			// saved upgrade level. Reconcile that level without respawning the mesh.
			if (const UknightWeaponItemDefinition* Weapon = Cast<UknightWeaponItemDefinition>(Wanted))
			{
				const int32 Level = Progression ? Progression->GetUpgradeLevel(Weapon) : 0;
				if (const FknightEquippedPart* Equipped = EquippedParts.Find(Slot); Equipped && Equipped->UpgradeLevel != Level)
				{
					RefreshWeaponUpgrade(Weapon, Level);
					bChanged = true;
				}
			}
			continue;
		}
		bChanged = true;
		if (Wanted)
		{
			EquipPartUnchecked(Wanted, Progression ? Progression->GetUpgradeLevel(Wanted) : 0);
		}
		else
		{
			UnequipSlot(Slot);
		}
	}
	return bChanged;
}

const TArray<TObjectPtr<const UknightPartItemDefinition>>& UknightPartsComponent::GetDefaultLoadout() const
{
	return DefaultLoadoutAsset ? DefaultLoadoutAsset->Parts : DefaultLoadout;
}

bool UknightPartsComponent::EquipPart(const UknightPartItemDefinition* Part, int32 UpgradeLevel)
{
	// 슬롯 없는 파츠는 아래 EquipPartUnchecked가 거른다. 여기서는 적재 한도만 본다.
	if (Part && Part->SlotTag.IsValid() && !CanEquipPart(Part))
	{
		const UknightPartItemDefinition* const Replaced = GetEquippedPart(Part->SlotTag);
		UE_LOG(Logknight, Log, TEXT("%s: cannot equip %s, weight %.1f -> %.1f exceeds load capacity %.1f."),
			*GetNameSafe(GetOwner()), *Part->GetName(), GetTotalWeight(),
			GetTotalWeight() - (Replaced ? Replaced->Weight : 0.f) + Part->Weight, GetLoadCapacity());
		return false;
	}
	return EquipPartUnchecked(Part, UpgradeLevel);
}

bool UknightPartsComponent::CanEquipPart(const UknightPartItemDefinition* Part) const
{
	if (!Part || !Part->SlotTag.IsValid())
	{
		return false;
	}
	const float LoadCapacity = GetLoadCapacity();
	if (LoadCapacity < 0.f)
	{
		return true;
	}
	const UknightPartItemDefinition* const Replaced = GetEquippedPart(Part->SlotTag);
	return knightPartsLoad::FitsLoadCapacity(GetTotalWeight(), Replaced ? Replaced->Weight : 0.f, Part->Weight, LoadCapacity);
}

float UknightPartsComponent::GetTotalWeight() const
{
	float TotalWeight = 0.f;
	for (const TPair<FGameplayTag, FknightEquippedPart>& Entry : EquippedParts)
	{
		TotalWeight += Entry.Value.Definition ? Entry.Value.Definition->Weight : 0.f;
	}
	return TotalWeight;
}

float UknightPartsComponent::GetLoadCapacity() const
{
	const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	const FGameplayAttribute LoadCapacity = UknightAttributeSet::GetLoadCapacityAttribute();
	return ASC && ASC->HasAttributeSetForAttribute(LoadCapacity) ? ASC->GetNumericAttribute(LoadCapacity) : -1.f;
}

void UknightPartsComponent::UpdateWeightAttribute()
{
	// 중량의 출처는 파츠뿐이라 기본값을 합으로 덮어쓴다. 다른 효과(GE)가 Weight를 바꾸면 그 위에 더해진다.
	UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	const FGameplayAttribute Weight = UknightAttributeSet::GetWeightAttribute();
	if (ASC && ASC->HasAttributeSetForAttribute(Weight))
	{
		ASC->SetNumericAttributeBase(Weight, GetTotalWeight());
	}
}

bool UknightPartsComponent::EquipPartUnchecked(const UknightPartItemDefinition* Part, int32 UpgradeLevel)
{
	ACharacter* const Character = Cast<ACharacter>(GetOwner());
	if (!Part || !Part->SlotTag.IsValid() || !Character)
	{
		return false;
	}
	UnequipSlot(Part->SlotTag);

	FknightEquippedPart& Equipped = EquippedParts.Add(Part->SlotTag);
	Equipped.Definition = Part;
	Equipped.UpgradeLevel = FMath::Clamp(UpgradeLevel, 0, Part->GetMaxUpgradeLevel());

	// 메시 컴포넌트: 스켈레탈 메시(Mesh)가 있으면 그것, 없으면 스태틱 메시(StaticMesh, 2026-10-10).
	UMeshComponent* PartMesh = nullptr;
	if (USkeletalMesh* const Mesh = UknightAssetManager::GetAsset(Part->Mesh))
	{
		USkeletalMeshComponent* const SkeletalPart = NewObject<USkeletalMeshComponent>(Character, NAME_None, RF_Transient);
		SkeletalPart->SetSkeletalMeshAsset(Mesh);
		PartMesh = SkeletalPart;
	}
	else if (UStaticMesh* const StaticMesh = UknightAssetManager::GetAsset(Part->StaticMesh))
	{
		UStaticMeshComponent* const StaticPart = NewObject<UStaticMeshComponent>(Character, NAME_None, RF_Transient);
		StaticPart->SetStaticMesh(StaticMesh);
		PartMesh = StaticPart;
	}
	if (PartMesh)
	{
		PartMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PartMesh->ComponentTags.Append(Part->MeshComponentTags);
		PartMesh->RegisterComponent();
		Part->Attachment.Apply(PartMesh, Character->GetMesh());
		Character->AddInstanceComponent(PartMesh);
		Equipped.MeshComponent = PartMesh;
	}

	UknightAbilitySystemComponent* const ASC = Cast<UknightAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character));
	if (Part->AbilitySet && ASC)
	{
		Part->AbilitySet->GiveToAbilitySystem(ASC, &Equipped.GrantedHandles, const_cast<UknightPartItemDefinition*>(Part), Equipped.UpgradeLevel);
	}
	// 파츠 능력치: 메모리에서 만든 GE를 적용하고 핸들을 들고 있다가 해제할 때 회수한다.
	if (const UGameplayEffect* const StatEffect = ASC ? GetStatEffect(Part) : nullptr)
	{
		Equipped.StatEffectHandle = ASC->ApplyGameplayEffectToSelf(StatEffect, 1.f, ASC->MakeEffectContext());
	}
	if (const UknightWeaponItemDefinition* Weapon = Cast<UknightWeaponItemDefinition>(Part))
	{
		ApplyWeaponUpgradeAttack(Equipped, Weapon);

		// (2026-10-27) 무기 상시 이펙트: 무기 메시에 붙여서 뽑기·넣기(소켓 이동)를 따라가게 한다. 전용 서버는 그리지 않는다.
		UNiagaraSystem* const Aura = UknightAssetManager::GetAsset(Weapon->WeaponAuraEffect);
		if (Aura && PartMesh && Character->GetNetMode() != NM_DedicatedServer)
		{
			Equipped.AuraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(Aura, static_cast<USceneComponent*>(PartMesh), NAME_None,
				FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector, EAttachLocation::KeepRelativeOffset, false, ENCPoolMethod::None, true, true);
		}
	}

	UpdateWeightAttribute();
	RefreshSetBonuses();
	RefreshBodyRegionVisibility();

	UE_LOG(Logknight, Verbose, TEXT("%s equipped %s (level %d) in %s."), *GetNameSafe(Character), *Part->GetName(), Equipped.UpgradeLevel, *Part->SlotTag.ToString());
	OnEquipmentChanged.Broadcast(Part->SlotTag, const_cast<UknightPartItemDefinition*>(Part));
	return true;
}

void UknightPartsComponent::RefreshWeaponUpgrade(const UknightWeaponItemDefinition* Weapon, int32 NewLevel)
{
	if (!Weapon) return;
	FknightEquippedPart* Equipped = EquippedParts.Find(Weapon->SlotTag);
	if (!Equipped || Equipped->Definition != Weapon) return;
	Equipped->UpgradeLevel = FMath::Clamp(NewLevel, 0, Weapon->GetMaxUpgradeLevel());
	ApplyWeaponUpgradeAttack(*Equipped, Weapon);
}

void UknightPartsComponent::ApplyWeaponUpgradeAttack(FknightEquippedPart& Part, const UknightWeaponItemDefinition* Weapon)
{
	UknightAbilitySystemComponent* ASC = Cast<UknightAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()));
	if (!ASC || !Weapon) return;
	if (Part.UpgradeAttackEffectHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(Part.UpgradeAttackEffectHandle);
		Part.UpgradeAttackEffectHandle.Invalidate();
	}
	Part.UpgradeAttackEffect = nullptr;
	const float Bonus = Weapon->GetUpgradeAttackBonus(Part.UpgradeLevel);
	if (Bonus <= 0.f) return;
	UGameplayEffect* Effect = NewObject<UGameplayEffect>(this, NAME_None, RF_Transient);
	Effect->DurationPolicy = EGameplayEffectDurationType::Infinite;
	FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UknightCoreAttributeSet::GetAttackPowerAttribute();
	Modifier.ModifierOp = EGameplayModOp::AddBase;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Bonus));
	Part.UpgradeAttackEffect = Effect;
	Part.UpgradeAttackEffectHandle = ASC->ApplyGameplayEffectToSelf(Effect, 1.f, ASC->MakeEffectContext());
}

void UknightPartsComponent::UnequipSlot(FGameplayTag SlotTag)
{
	FknightEquippedPart Removed;
	if (EquippedParts.RemoveAndCopyValue(SlotTag, Removed))
	{
		RemovePart(Removed);
		UpdateWeightAttribute();
		RefreshSetBonuses();
		RefreshBodyRegionVisibility();
		OnEquipmentChanged.Broadcast(SlotTag, nullptr);
	}
}

const UGameplayEffect* UknightPartsComponent::GetStatEffect(const UknightPartItemDefinition* Part)
{
	if (!Part || Part->StatModifiers.IsEmpty())
	{
		return nullptr;
	}
	if (const TObjectPtr<UGameplayEffect>* const Cached = StatEffects.Find(Part))
	{
		return *Cached;
	}

	// 장착하는 동안 유지되는(무한 지속) GE를 파츠의 능력치 목록대로 만든다. 에셋으로 저장하지 않는다.
	const FName EffectName = MakeUniqueObjectName(this, UGameplayEffect::StaticClass(), *FString::Printf(TEXT("GE_PartStats_%s"), *Part->GetName()));
	UGameplayEffect* const Effect = NewObject<UGameplayEffect>(this, EffectName, RF_Transient);
	Effect->DurationPolicy = EGameplayEffectDurationType::Infinite;
	for (const FknightPartStatModifier& Stat : Part->StatModifiers)
	{
		if (!Stat.Attribute.IsValid())
		{
			continue;
		}
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Stat.Attribute;
		Modifier.ModifierOp = Stat.ModifierOp;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Stat.Value));
		Effect->Modifiers.Add(Modifier);
	}
	StatEffects.Add(Part, Effect);
	return Effect;
}

const UGameplayEffect* UknightPartsComponent::GetSetStatEffect(const FString& Key, const TArray<FknightPartStatModifier>& Modifiers)
{
	if (Modifiers.IsEmpty()) return nullptr;
	if (const TObjectPtr<UGameplayEffect>* Cached = SetStatEffects.Find(Key)) return *Cached;
	UGameplayEffect* Effect = NewObject<UGameplayEffect>(this, NAME_None, RF_Transient);
	Effect->DurationPolicy = EGameplayEffectDurationType::Infinite;
	for (const FknightPartStatModifier& Stat : Modifiers)
	{
		if (!Stat.Attribute.IsValid()) continue;
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Stat.Attribute;
		Modifier.ModifierOp = Stat.ModifierOp;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Stat.Value));
		Effect->Modifiers.Add(Modifier);
	}
	SetStatEffects.Add(Key, Effect);
	return Effect;
}

void UknightPartsComponent::RefreshSetBonuses()
{
	UknightAbilitySystemComponent* const ASC = Cast<UknightAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()));
	if (!ASC) return;
	for (FActiveGameplayEffectHandle& Handle : ActiveSetHandles)
	{
		if (Handle.IsValid()) ASC->RemoveActiveGameplayEffect(Handle);
	}
	ActiveSetHandles.Reset();
	const UknightMaintenanceCatalog* Catalog = UknightEquipmentSettings::LoadCatalog();
	if (!Catalog) return;
	TMap<FGameplayTag, int32> Counts;
	for (const auto& Entry : EquippedParts)
	{
		const FGameplayTag Slot = Entry.Key;
		if (Slot != knightGameplayTags::Equipment_Slot_Head && Slot != knightGameplayTags::Equipment_Slot_Torso
			&& Slot != knightGameplayTags::Equipment_Slot_Arms && Slot != knightGameplayTags::Equipment_Slot_Legs
			&& Slot != knightGameplayTags::Equipment_Slot_Weapon) continue;
		const UknightPartItemDefinition* Part = Entry.Value.Definition.Get();
		if (Part && Part->SetTag.IsValid()) ++Counts.FindOrAdd(Part->SetTag);
	}
	for (const UknightEquipmentSetDefinition* Set : Catalog->Sets)
	{
		if (!Set || !Set->SetTag.IsValid()) continue;
		const int32 Count = Counts.FindRef(Set->SetTag);
		const FknightSetTier* Tiers[] = { &Set->TwoPiece, &Set->FourPiece };
		for (int32 TierIndex = 0; TierIndex < 2; ++TierIndex)
		{
			const FknightSetTier& Tier = *Tiers[TierIndex];
			if (Count < Tier.RequiredCount || Tier.StatModifiers.IsEmpty()) continue;
			const FString Key = FString::Printf(TEXT("%s_%d"), *Set->GetPathName(), TierIndex);
			if (const UGameplayEffect* Effect = GetSetStatEffect(Key, Tier.StatModifiers))
			{
				ActiveSetHandles.Add(ASC->ApplyGameplayEffectToSelf(Effect, 1.f, ASC->MakeEffectContext()));
			}
		}
	}
}

void UknightPartsComponent::RemovePart(FknightEquippedPart& Part)
{
	if (UknightAbilitySystemComponent* const ASC = Cast<UknightAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner())))
	{
		Part.GrantedHandles.TakeFromAbilitySystem(ASC);
		if (Part.StatEffectHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(Part.StatEffectHandle);
			Part.StatEffectHandle.Invalidate();
		}
		if (Part.UpgradeAttackEffectHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(Part.UpgradeAttackEffectHandle);
			Part.UpgradeAttackEffectHandle.Invalidate();
		}
		Part.UpgradeAttackEffect = nullptr;
	}
	if (UNiagaraComponent* const Aura = Part.AuraComponent.Get())
	{
		Aura->DestroyComponent();
	}
	Part.AuraComponent = nullptr;
	if (UMeshComponent* const Mesh = Part.MeshComponent.Get())
	{
		if (AActor* const Owner = GetOwner())
		{
			Owner->RemoveInstanceComponent(Mesh);
		}
		Mesh->DestroyComponent();
	}
	Part.MeshComponent = nullptr;
}

const UknightPartItemDefinition* UknightPartsComponent::GetEquippedPart(FGameplayTag SlotTag) const
{
	const FknightEquippedPart* const Part = EquippedParts.Find(SlotTag);
	return Part ? Part->Definition.Get() : nullptr;
}

void UknightPartsComponent::GetPartMeshes(TArray<UMeshComponent*>& OutMeshes) const
{
	for (const TPair<FGameplayTag, FknightEquippedPart>& Entry : EquippedParts)
	{
		if (Entry.Value.MeshComponent)
		{
			OutMeshes.Add(Entry.Value.MeshComponent);
		}
	}
	for (USkeletalMeshComponent* const RegionMesh : BodyRegionMeshes)
	{
		if (RegionMesh)
		{
			OutMeshes.Add(RegionMesh);
		}
	}
}

void UknightPartsComponent::GetMeshesForBodyRegion(const int32 RegionFlags, const FGameplayTag SlotTag, TArray<UMeshComponent*>& OutMeshes) const
{
	for (const TPair<FGameplayTag, FknightEquippedPart>& Entry : EquippedParts)
	{
		const UknightPartItemDefinition* const Part = Entry.Value.Definition.Get();
		if (Entry.Value.MeshComponent && Part && (Entry.Key == SlotTag || (Part->Attachment.ReplaceRegions & RegionFlags) != 0))
		{
			OutMeshes.AddUnique(Entry.Value.MeshComponent);
		}
	}
	for (int32 Index = 0; Index < BodyRegionMeshes.Num(); ++Index)
	{
		if (BodyRegionMeshes[Index] && (BodyRegionFlags[Index] & RegionFlags) != 0)
		{
			OutMeshes.AddUnique(BodyRegionMeshes[Index]);
		}
	}
}

void UknightPartsComponent::AttachWeaponToSocket(UMeshComponent* WeaponMesh, USkeletalMeshComponent* Body, FName Socket)
{
	if (!WeaponMesh || !Body)
	{
		return;
	}
	const AActor* const Owner = Body->GetOwner();
	if (const UknightPartsComponent* const Parts = Owner ? Owner->FindComponentByClass<UknightPartsComponent>() : nullptr)
	{
		for (const TPair<FGameplayTag, FknightEquippedPart>& Entry : Parts->EquippedParts)
		{
			const UknightPartItemDefinition* const Part = Entry.Value.Definition.Get();
			if (Part && Entry.Value.MeshComponent == WeaponMesh && Part->Attachment.AttachSocket == Socket)
			{
				Part->Attachment.Apply(WeaponMesh, Body);
				return;
			}
		}
	}
	WeaponMesh->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
}

void UknightPartsComponent::CreateBodyRegionMeshes()
{
	ACharacter* const Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* const Body = Character ? Character->GetMesh() : nullptr;
	if (!BodyRegions || !Body || !BodyRegionMeshes.IsEmpty())
	{
		return;
	}
	// 다른 메시용으로 나눈 세트면 부위가 어긋나므로 쓰지 않는다.
	if (BodyRegions->SourceMesh.ToSoftObjectPath() != FSoftObjectPath(Body->GetSkeletalMeshAsset()))
	{
		UE_LOG(Logknight, Warning, TEXT("%s: Body Regions %s was made from %s, but the body mesh is %s. Showing the body as is."),
			*GetNameSafe(Character), *BodyRegions->GetName(), *BodyRegions->SourceMesh.ToString(), *GetNameSafe(Body->GetSkeletalMeshAsset()));
		return;
	}

	for (const FMechBodyRegionMesh& Region : BodyRegions->Regions)
	{
		USkeletalMesh* const Mesh = UknightAssetManager::GetAsset(Region.Mesh);
		if (!Mesh)
		{
			continue;
		}
		USkeletalMeshComponent* const RegionMesh = NewObject<USkeletalMeshComponent>(Character, NAME_None, RF_Transient);
		UMechBodyRegionSet::SetupRegionComponent(RegionMesh, Mesh, Body);
		RegionMesh->RegisterComponent();
		Character->AddInstanceComponent(RegionMesh);
		BodyRegionMeshes.Add(RegionMesh);
		BodyRegionFlags.Add(static_cast<int32>(Region.Region));
	}
	if (!BodyRegionMeshes.IsEmpty())
	{
		UMechBodyRegionSet::SetMechBodyHidden(Body, true);
	}
}

void UknightPartsComponent::RefreshBodyRegionVisibility()
{
	int32 HiddenRegions = 0;
	for (const TPair<FGameplayTag, FknightEquippedPart>& Entry : EquippedParts)
	{
		if (const UknightPartItemDefinition* const Part = Entry.Value.Definition.Get())
		{
			HiddenRegions |= Part->Attachment.ReplaceRegions;
		}
	}
	for (int32 Index = 0; Index < BodyRegionMeshes.Num(); ++Index)
	{
		if (BodyRegionMeshes[Index])
		{
			BodyRegionMeshes[Index]->SetVisibility((BodyRegionFlags[Index] & HiddenRegions) == 0);
		}
	}
}

void UknightPartsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UknightAbilitySystemComponent* ASC = Cast<UknightAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner())))
	{
		for (FActiveGameplayEffectHandle& Handle : ActiveSetHandles)
		{
			if (Handle.IsValid()) ASC->RemoveActiveGameplayEffect(Handle);
		}
	}
	ActiveSetHandles.Reset();
	for (TPair<FGameplayTag, FknightEquippedPart>& Entry : EquippedParts)
	{
		RemovePart(Entry.Value);
	}
	EquippedParts.Reset();
	for (USkeletalMeshComponent* const RegionMesh : BodyRegionMeshes)
	{
		if (RegionMesh)
		{
			if (AActor* const Owner = GetOwner())
			{
				Owner->RemoveInstanceComponent(RegionMesh);
			}
			RegionMesh->DestroyComponent();
		}
	}
	BodyRegionMeshes.Reset();
	BodyRegionFlags.Reset();
	Super::EndPlay(EndPlayReason);
}
