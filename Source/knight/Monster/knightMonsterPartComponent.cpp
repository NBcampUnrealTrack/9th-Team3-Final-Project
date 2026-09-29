#include "Monster/knightMonsterPartComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Components/SkinnedMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "Monster/knightMonsterPartData.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

UknightMonsterPartComponent::UknightMonsterPartComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UknightMonsterPartComponent::InitializeParts(const UknightMonsterPartData* InPartData, UAbilitySystemComponent* InASC)
{
	PartData = InPartData;
	PartStates.Reset();
	BoneToPart.Reset();
	if (PartData)
	{
		for (int32 Index = 0; Index < PartData->Parts.Num(); ++Index)
		{
			const FknightMonsterPartDefinition& Definition = PartData->Parts[Index];
			FknightMonsterPartState& State = PartStates.AddDefaulted_GetRef();
			State.PartTag = Definition.PartTag;
			for (const FName& Bone : Definition.Bones)
			{
				BoneToPart.Add(Bone, Index);
			}
		}
	}

	if (BoundASC.IsValid())
	{
		if (const UknightCoreAttributeSet* const OldSet = BoundASC->GetSet<UknightCoreAttributeSet>())
		{
			OldSet->OnDamaged.RemoveAll(this);
		}
	}
	BoundASC = InASC;
	if (const UknightCoreAttributeSet* const CoreSet = InASC ? InASC->GetSet<UknightCoreAttributeSet>() : nullptr)
	{
		CoreSet->OnDamaged.AddUObject(this, &ThisClass::HandleDamaged);
	}
	else
	{
		UE_LOG(LogknightCombat, Warning, TEXT("%s: part damage needs an ASC with UknightCoreAttributeSet."), *GetNameSafe(GetOwner()));
	}
}

void UknightMonsterPartComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BoundASC.IsValid())
	{
		if (const UknightCoreAttributeSet* const CoreSet = BoundASC->GetSet<UknightCoreAttributeSet>())
		{
			CoreSet->OnDamaged.RemoveAll(this);
		}
	}
	BoundASC.Reset();
	Super::EndPlay(EndPlayReason);
}

FGameplayTag UknightMonsterPartComponent::ResolvePart(const FHitResult& Hit) const
{
	if (const int32* const Index = BoneToPart.Find(Hit.BoneName))
	{
		return PartStates.IsValidIndex(*Index) ? PartStates[*Index].PartTag : FGameplayTag();
	}
	return FGameplayTag();
}

float UknightMonsterPartComponent::GetDamageMultiplier(const FGameplayTag& PartTag) const
{
	const int32 Index = FindPartIndex(PartTag);
	const FknightMonsterPartDefinition* const Definition = GetDefinition(Index);
	if (!Definition)
	{
		return 1.f;
	}
	return PartStates[Index].bBroken ? Definition->BrokenDamageMultiplier : Definition->DamageMultiplier;
}

bool UknightMonsterPartComponent::IsPartBroken(FGameplayTag PartTag) const
{
	const int32 Index = FindPartIndex(PartTag);
	return PartStates.IsValidIndex(Index) && PartStates[Index].bBroken;
}

void UknightMonsterPartComponent::GetTargetPoints(TArray<FknightTargetPoint>& OutPoints) const
{
	const ACharacter* const Character = Cast<ACharacter>(GetOwner());
	USkinnedMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}
	for (int32 Index = 0; Index < PartStates.Num(); ++Index)
	{
		const FknightMonsterPartDefinition* const Definition = GetDefinition(Index);
		if (!Definition || Definition->LockOnSocket.IsNone())
		{
			continue;
		}
		// A severed part (hidden bone) is no longer something to lock onto.
		if (PartStates[Index].bBroken && !Definition->HideBoneOnBreak.IsNone())
		{
			continue;
		}
		FknightTargetPoint& Point = OutPoints.AddDefaulted_GetRef();
		Point.Component = Mesh;
		Point.SocketName = Definition->LockOnSocket;
		Point.PartTag = Definition->PartTag;
	}
}

void UknightMonsterPartComponent::ApplyPartDamage(FGameplayTag PartTag, float Damage, AActor* DamageInstigator)
{
	const int32 Index = FindPartIndex(PartTag);
	const FknightMonsterPartDefinition* const Definition = GetDefinition(Index);
	if (!Definition || Damage <= 0.f)
	{
		return;
	}
	FknightMonsterPartState& State = PartStates[Index];
	State.DamageTaken += Damage;
	if (Definition->bBreakable && !State.bBroken && State.DamageTaken >= Definition->BreakDamage)
	{
		BreakPart(Index, DamageInstigator);
	}
}

void UknightMonsterPartComponent::HandleDamaged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* EffectSpec,
	float Magnitude, float OldValue, float NewValue)
{
	const FHitResult* const Hit = EffectSpec ? EffectSpec->GetEffectContext().GetHitResult() : nullptr;
	if (!Hit)
	{
		return;
	}
	const FGameplayTag PartTag = ResolvePart(*Hit);
	if (PartTag.IsValid())
	{
		ApplyPartDamage(PartTag, Magnitude, DamageInstigator);
	}
}

void UknightMonsterPartComponent::BreakPart(int32 PartIndex, AActor* BreakInstigator)
{
	const FknightMonsterPartDefinition* const Definition = GetDefinition(PartIndex);
	if (!Definition)
	{
		return;
	}
	PartStates[PartIndex].bBroken = true;
	UE_LOG(LogknightCombat, Log, TEXT("%s: part %s broken by %s."), *GetNameSafe(GetOwner()), *Definition->PartTag.ToString(), *GetNameSafe(BreakInstigator));

	const ACharacter* const Character = Cast<ACharacter>(GetOwner());
	USkinnedMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
	FVector BreakLocation = GetOwner()->GetActorLocation();
	if (Mesh)
	{
		const FName LocationBone = !Definition->HideBoneOnBreak.IsNone() ? Definition->HideBoneOnBreak
			: (Definition->Bones.Num() > 0 ? Definition->Bones[0] : NAME_None);
		if (!LocationBone.IsNone())
		{
			BreakLocation = Mesh->GetSocketLocation(LocationBone);
		}
		if (!Definition->HideBoneOnBreak.IsNone())
		{
			Mesh->HideBoneByName(Definition->HideBoneOnBreak, PBO_Term);
		}
	}

	if (UAbilitySystemComponent* const ASC = BoundASC.Get())
	{
		if (Definition->BreakEffect)
		{
			ASC->ApplyGameplayEffectToSelf(Definition->BreakEffect->GetDefaultObject<UGameplayEffect>(), 1.f, ASC->MakeEffectContext());
		}

		FGameplayCueParameters CueParams;
		CueParams.Location = BreakLocation;
		CueParams.Instigator = BreakInstigator;
		CueParams.AggregatedSourceTags.AddTag(Definition->PartTag);
		ASC->ExecuteGameplayCue(knightGameplayTags::GameplayCue_PartBreak, CueParams);

		FGameplayEventData Payload;
		Payload.EventTag = knightGameplayTags::Event_PartBroken;
		Payload.Instigator = BreakInstigator;
		Payload.Target = GetOwner();
		Payload.TargetTags.AddTag(Definition->PartTag);
		ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
	}

	OnPartBroken.Broadcast(Definition->PartTag, BreakInstigator);
}

int32 UknightMonsterPartComponent::FindPartIndex(const FGameplayTag& PartTag) const
{
	if (!PartTag.IsValid())
	{
		return INDEX_NONE;
	}
	return PartStates.IndexOfByPredicate([&PartTag](const FknightMonsterPartState& State) { return State.PartTag == PartTag; });
}

const FknightMonsterPartDefinition* UknightMonsterPartComponent::GetDefinition(int32 PartIndex) const
{
	return PartData && PartData->Parts.IsValidIndex(PartIndex) && PartStates.IsValidIndex(PartIndex) ? &PartData->Parts[PartIndex] : nullptr;
}
