#include "Item/knightPartsComponent.h"

#include "AbilitySystem/knightAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "Item/knightItemDefinitions.h"
#include "Progression/knightProgressionSubsystem.h"
#include "System/knightAssetManager.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

UknightPartsComponent::UknightPartsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UknightPartsComponent::ApplyLoadout()
{
	const UGameInstance* const GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UknightProgressionSubsystem* const Progression = GameInstance ? GameInstance->GetSubsystem<UknightProgressionSubsystem>() : nullptr;

	const FGameplayTag Slots[] = {
		knightGameplayTags::Equipment_Slot_Head, knightGameplayTags::Equipment_Slot_Torso,
		knightGameplayTags::Equipment_Slot_Arms, knightGameplayTags::Equipment_Slot_Legs,
		knightGameplayTags::Equipment_Slot_Backpack, knightGameplayTags::Equipment_Slot_Weapon
	};
	for (const FGameplayTag& Slot : Slots)
	{
		// The saved choice wins; the default fills slots the save has nothing for.
		const UknightPartItemDefinition* Part = Progression ? Progression->GetEquippedPart(Slot) : nullptr;
		if (!Part)
		{
			const TObjectPtr<const UknightPartItemDefinition>* const Default = DefaultLoadout.FindByPredicate(
				[&Slot](const UknightPartItemDefinition* Candidate) { return Candidate && Candidate->SlotTag == Slot; });
			Part = Default ? Default->Get() : nullptr;
		}
		if (Part)
		{
			EquipPart(Part, Progression ? Progression->GetUpgradeLevel(Part) : 0);
		}
	}
}

bool UknightPartsComponent::EquipPart(const UknightPartItemDefinition* Part, int32 UpgradeLevel)
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

	if (USkeletalMesh* const Mesh = UknightAssetManager::GetAsset(Part->Mesh))
	{
		USkeletalMeshComponent* const PartMesh = NewObject<USkeletalMeshComponent>(Character, NAME_None, RF_Transient);
		PartMesh->SetSkeletalMeshAsset(Mesh);
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

	UE_LOG(Logknight, Verbose, TEXT("%s equipped %s (level %d) in %s."), *GetNameSafe(Character), *Part->GetName(), Equipped.UpgradeLevel, *Part->SlotTag.ToString());
	OnEquipmentChanged.Broadcast(Part->SlotTag, const_cast<UknightPartItemDefinition*>(Part));
	return true;
}

void UknightPartsComponent::UnequipSlot(FGameplayTag SlotTag)
{
	FknightEquippedPart Removed;
	if (EquippedParts.RemoveAndCopyValue(SlotTag, Removed))
	{
		RemovePart(Removed);
		OnEquipmentChanged.Broadcast(SlotTag, nullptr);
	}
}

void UknightPartsComponent::RemovePart(FknightEquippedPart& Part)
{
	if (UknightAbilitySystemComponent* const ASC = Cast<UknightAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner())))
	{
		Part.GrantedHandles.TakeFromAbilitySystem(ASC);
	}
	if (USkeletalMeshComponent* const Mesh = Part.MeshComponent.Get())
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

void UknightPartsComponent::GetPartMeshes(TArray<USkeletalMeshComponent*>& OutMeshes) const
{
	for (const TPair<FGameplayTag, FknightEquippedPart>& Entry : EquippedParts)
	{
		if (Entry.Value.MeshComponent)
		{
			OutMeshes.Add(Entry.Value.MeshComponent);
		}
	}
}

void UknightPartsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (TPair<FGameplayTag, FknightEquippedPart>& Entry : EquippedParts)
	{
		RemovePart(Entry.Value);
	}
	EquippedParts.Reset();
	Super::EndPlay(EndPlayReason);
}
