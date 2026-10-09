#include "Monster/knightMonsterPartComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "Monster/knightMonsterPartData.h"
#include "System/knightGameplayTags.h"
#include "knight.h"
#include "knightPerfCounters.h"

UknightMonsterPartComponent::UknightMonsterPartComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UknightMonsterPartComponent::InitializeParts(const UknightMonsterPartData* InPartData, UAbilitySystemComponent* InASC)
{
	PartData = InPartData;
	PartStates.Reset();
	BoneToPart.Reset();
	ComponentTagToPart.Reset();
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
			for (const FName& ComponentTag : Definition.ComponentTags)
			{
				ComponentTagToPart.Add(ComponentTag, Index);
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
	const int32* Index = Hit.BoneName.IsNone() ? nullptr : BoneToPart.Find(Hit.BoneName);
	if (!Index && !ComponentTagToPart.IsEmpty())
	{
		if (const UPrimitiveComponent* const Component = Hit.GetComponent())
		{
			for (const FName& ComponentTag : Component->ComponentTags)
			{
				Index = ComponentTagToPart.Find(ComponentTag);
				if (Index)
				{
					break;
				}
			}
		}
	}
	return Index && PartStates.IsValidIndex(*Index) ? PartStates[*Index].PartTag : FGameplayTag();
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
	TArray<UPrimitiveComponent*> PartComponents;
	for (int32 Index = 0; Index < PartStates.Num(); ++Index)
	{
		const FknightMonsterPartDefinition* const Definition = GetDefinition(Index);
		if (!Definition)
		{
			continue;
		}
		// 잘려 나간 부위(숨겨진 본이나 컴포넌트)는 더 이상 락온 대상이 아니다.
		if (PartStates[Index].bBroken && (!Definition->HideBoneOnBreak.IsNone() || Definition->bHideComponentsOnBreak))
		{
			continue;
		}

		USceneComponent* PointComponent = nullptr;
		FName PointSocket = NAME_None;
		if (Definition->bLockOnComponent)
		{
			PartComponents.Reset();
			GetPartComponents(Index, PartComponents);
			PointComponent = PartComponents.Num() > 0 ? PartComponents[0] : nullptr;
		}
		else if (!Definition->LockOnSocket.IsNone())
		{
			PointComponent = Mesh;
			PointSocket = Definition->LockOnSocket;
		}
		if (!PointComponent)
		{
			continue;
		}
		FknightTargetPoint& Point = OutPoints.AddDefaulted_GetRef();
		Point.Component = PointComponent;
		Point.SocketName = PointSocket;
		Point.PartTag = Definition->PartTag;
	}
}

bool UknightMonsterPartComponent::GetPartLocation(const FGameplayTag& PartTag, FVector& OutLocation) const
{
	const int32 Index = FindPartIndex(PartTag);
	const FknightMonsterPartDefinition* const Definition = GetDefinition(Index);
	if (!Definition)
	{
		return false;
	}
	TArray<UPrimitiveComponent*> PartComponents;
	GetPartComponents(Index, PartComponents);
	if (PartComponents.Num() > 0)
	{
		OutLocation = PartComponents[0]->GetComponentLocation();
		return true;
	}
	const ACharacter* const Character = Cast<ACharacter>(GetOwner());
	USkinnedMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
	if (Mesh && Definition->Bones.Num() > 0)
	{
		OutLocation = Mesh->GetSocketLocation(Definition->Bones[0]);
		return true;
	}
	return false;
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
		KNIGHT_PERF_COUNT("Combat.PartHits", 1);
		ApplyPartDamage(PartTag, Magnitude, DamageInstigator);
	}
	else
	{
		// 어느 부위에도 속하지 않는 곳에 맞은 타격(주로 이동 캡슐).
		KNIGHT_PERF_COUNT("Combat.UnresolvedPartHits", 1);
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

	TArray<UPrimitiveComponent*> PartComponents;
	GetPartComponents(PartIndex, PartComponents);
	if (PartComponents.Num() > 0)
	{
		BreakLocation = PartComponents[0]->GetComponentLocation();
	}
	if (Definition->bHideComponentsOnBreak)
	{
		for (UPrimitiveComponent* const Component : PartComponents)
		{
			Component->SetVisibility(false, true);
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

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
		if (!Definition->GrantedTagsOnBreak.IsEmpty())
		{
			ASC->AddLooseGameplayTags(Definition->GrantedTagsOnBreak);
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

void UknightMonsterPartComponent::GetPartComponents(int32 PartIndex, TArray<UPrimitiveComponent*>& OutComponents) const
{
	const FknightMonsterPartDefinition* const Definition = GetDefinition(PartIndex);
	if (!Definition || Definition->ComponentTags.IsEmpty() || !GetOwner())
	{
		return;
	}
	TArray<UPrimitiveComponent*> Components;
	GetOwner()->GetComponents<UPrimitiveComponent>(Components);
	for (UPrimitiveComponent* const Component : Components)
	{
		for (const FName& ComponentTag : Definition->ComponentTags)
		{
			if (Component->ComponentHasTag(ComponentTag))
			{
				OutComponents.Add(Component);
				break;
			}
		}
	}
}
