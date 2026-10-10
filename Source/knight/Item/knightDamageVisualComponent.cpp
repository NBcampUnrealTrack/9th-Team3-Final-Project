#include "Item/knightDamageVisualComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Item/knightPartsComponent.h"

UknightDamageVisualComponent::UknightDamageVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UknightDamageVisualComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		HealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UknightCoreAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HandleHealthChanged);
		MaxHealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UknightCoreAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &ThisClass::HandleHealthChanged);
	}
	if (UknightPartsComponent* const Parts = GetOwner()->FindComponentByClass<UknightPartsComponent>())
	{
		Parts->OnEquipmentChanged.AddUniqueDynamic(this, &ThisClass::HandleEquipmentChanged);
	}
	Refresh();
}

void UknightDamageVisualComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UknightCoreAttributeSet::GetHealthAttribute()).Remove(HealthHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UknightCoreAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthHandle);
	}
	if (UknightPartsComponent* const Parts = GetOwner()->FindComponentByClass<UknightPartsComponent>())
	{
		Parts->OnEquipmentChanged.RemoveDynamic(this, &ThisClass::HandleEquipmentChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void UknightDamageVisualComponent::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	Refresh();
}

void UknightDamageVisualComponent::HandleEquipmentChanged(FGameplayTag SlotTag, UknightPartItemDefinition* Part)
{
	// New part meshes start pristine; force a rewrite.
	AppliedDamage = -1.f;
	Refresh();
}

float UknightDamageVisualComponent::ComputeDamage() const
{
	const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	if (!ASC)
	{
		return 0.f;
	}
	bool bFound = false;
	const float Health = ASC->GetGameplayAttributeValue(UknightCoreAttributeSet::GetHealthAttribute(), bFound);
	const float MaxHealth = ASC->GetGameplayAttributeValue(UknightCoreAttributeSet::GetMaxHealthAttribute(), bFound);
	const float Damage = MaxHealth > 0.f ? FMath::Clamp(1.f - Health / MaxHealth, 0.f, 1.f) : 0.f;
	if (DamageSteps <= 0)
	{
		return Damage;
	}
	// Damage is split into Steps + 1 bands; the first band is pristine. With 3 steps:
	// under 25% -> 0, 25-50% -> 1/3, 50-75% -> 2/3, 75% and above -> 1.
	const int32 Step = FMath::Min(FMath::FloorToInt(Damage * (DamageSteps + 1)), DamageSteps);
	return static_cast<float>(Step) / static_cast<float>(DamageSteps);
}

void UknightDamageVisualComponent::Refresh()
{
	const float Damage = ComputeDamage();
	if (FMath::IsNearlyEqual(Damage, AppliedDamage))
	{
		return;
	}
	AppliedDamage = Damage;

	// 나이트 본체(스켈레탈) + 파츠 메시(스켈레탈·스태틱, 2026-10-10).
	TArray<UMeshComponent*> Meshes;
	if (const ACharacter* const Character = Cast<ACharacter>(GetOwner()))
	{
		Meshes.Add(Character->GetMesh());
	}
	if (const UknightPartsComponent* const Parts = GetOwner()->FindComponentByClass<UknightPartsComponent>())
	{
		Parts->GetPartMeshes(Meshes);
	}
	for (UMeshComponent* const Mesh : Meshes)
	{
		if (Mesh)
		{
			Mesh->SetScalarParameterForCustomPrimitiveData(DamageParameterName, Damage);
		}
	}
}
