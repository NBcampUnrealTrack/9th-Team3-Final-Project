#include "Monster/knightMonsterPartComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "AnimationRuntime.h"
#include "Camera/knightCameraModifier_RoarShockwave.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "GameFramework/Character.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "Monster/knightMonsterBreakPiece.h"
#include "Monster/knightMonsterPartData.h"
#include "Sound/SoundBase.h"
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
	if (!Index || !PartStates.IsValidIndex(*Index))
	{
		return FGameplayTag();
	}
	// 드러나지 않은 약점은 덮고 있는 부위로 맞는다. 한 단계만 넘긴다.
	const FknightMonsterPartDefinition* const Definition = GetDefinition(*Index);
	if (Definition && !Definition->ExposedTags.IsEmpty() && !(BoundASC.IsValid() && BoundASC->HasAnyMatchingGameplayTags(Definition->ExposedTags)))
	{
		const int32 CoverIndex = FindPartIndex(Definition->CoveredByPart);
		return PartStates.IsValidIndex(CoverIndex) ? PartStates[CoverIndex].PartTag : FGameplayTag();
	}
	return PartStates[*Index].PartTag;
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
	const EknightPartCondition OldCondition = GetCondition(Index);
	State.DamageTaken += Damage;
	if (!Definition->bBreakable || State.bBroken)
	{
		return;
	}
	OnPartDamaged.Broadcast(State.PartTag, GetHealthRatio(Index));
	if (State.DamageTaken >= Definition->BreakDamage)
	{
		BreakPart(Index, DamageInstigator);
		return;
	}
	const EknightPartCondition NewCondition = GetCondition(Index);
	if (NewCondition != OldCondition)
	{
		OnPartConditionChanged.Broadcast(State.PartTag, NewCondition);
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
	// 남은 체력을 함께 남겨, 한 판 플레이 로그로 부위가 전투의 어느 시점에 부서지는지(BreakDamage 조정) 본다.
	const UknightCoreAttributeSet* const CoreSet = BoundASC.IsValid() ? BoundASC->GetSet<UknightCoreAttributeSet>() : nullptr;
	const float HealthRatio = CoreSet && CoreSet->GetMaxHealth() > 0.f ? CoreSet->GetHealth() / CoreSet->GetMaxHealth() : -1.f;
	UE_LOG(LogknightCombat, Log, TEXT("%s: part %s broken by %s (damage %.0f, monster health %.0f%%)."), *GetNameSafe(GetOwner()),
		*Definition->PartTag.ToString(), *GetNameSafe(BreakInstigator), PartStates[PartIndex].DamageTaken, HealthRatio * 100.f);

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
			SpawnBreakPiece(*Definition, Mesh);
			Mesh->HideBoneByName(Definition->HideBoneOnBreak, PBO_Term);
		}
	}
	if (Definition->BreakSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Definition->BreakSound, BreakLocation);
	}
	UknightCameraModifier_RoarShockwave::AddBurstForLocalPlayers(GetWorld(), BreakLocation, Definition->BreakScreenShockwave);

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
		if (PartData)
		{
			for (const FknightMonsterPartBreakCombo& Combo : PartData->BreakCombos)
			{
				if (!Combo.RequiredTags.IsEmpty() && ASC->HasAllMatchingGameplayTags(Combo.RequiredTags) && !ASC->HasAllMatchingGameplayTags(Combo.GrantedTags))
				{
					ASC->AddLooseGameplayTags(Combo.GrantedTags);
				}
			}
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

	OnPartConditionChanged.Broadcast(Definition->PartTag, EknightPartCondition::Broken);
	OnPartBroken.Broadcast(Definition->PartTag, BreakInstigator);
	OnPartBrokenNative.Broadcast(Definition->PartTag, BreakInstigator, GetOwner());
}

void UknightMonsterPartComponent::SpawnBreakPiece(const FknightMonsterPartDefinition& Definition, USkinnedMeshComponent* Mesh) const
{
	const USkinnedAsset* const Asset = Mesh ? Mesh->GetSkinnedAsset() : nullptr;
	UWorld* const World = GetWorld();
	if (!Definition.BreakPieceMesh || !Asset || !World)
	{
		return;
	}
	const FReferenceSkeleton& RefSkeleton = Asset->GetRefSkeleton();
	const int32 BoneIndex = RefSkeleton.FindBoneIndex(Definition.HideBoneOnBreak);
	if (BoneIndex == INDEX_NONE)
	{
		return;
	}

	// 조각은 기준 자세의 메시 축 그대로 잘려 있다(피벗만 관절로 옮김). 기준 자세의 뼈 회전을 되돌린 뒤 지금 뼈 변환을 씌우면
	// 숨겨지는 원래 부위와 같은 자리, 같은 자세, 같은 크기(메시 배율)가 된다.
	const FQuat RefRotation = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, BoneIndex).GetRotation();
	const FTransform BoneTransform = Mesh->GetSocketTransform(Definition.HideBoneOnBreak, RTS_World);
	const FTransform SpawnTransform = FTransform(RefRotation.Inverse()) * BoneTransform;

	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UClass* const PieceClass = Definition.BreakPieceClass ? Definition.BreakPieceClass.Get() : AknightMonsterBreakPiece::StaticClass();
	if (AknightMonsterBreakPiece* const Piece = World->SpawnActor<AknightMonsterBreakPiece>(PieceClass, SpawnTransform, Params))
	{
		Piece->Launch(Definition.BreakPieceMesh, GetOwner()->GetVelocity(), BoneTransform.GetLocation() - GetOwner()->GetActorLocation(),
			Definition.BreakPieceLandSound);
	}
}

float UknightMonsterPartComponent::GetPartHealthRatio(FGameplayTag PartTag) const
{
	return GetHealthRatio(FindPartIndex(PartTag));
}

EknightPartCondition UknightMonsterPartComponent::GetPartCondition(FGameplayTag PartTag) const
{
	return GetCondition(FindPartIndex(PartTag));
}

float UknightMonsterPartComponent::GetHealthRatio(int32 PartIndex) const
{
	const FknightMonsterPartDefinition* const Definition = GetDefinition(PartIndex);
	if (!Definition || !Definition->bBreakable)
	{
		return 1.f;
	}
	if (PartStates[PartIndex].bBroken || Definition->BreakDamage <= 0.f)
	{
		return 0.f;
	}
	return FMath::Clamp(1.f - PartStates[PartIndex].DamageTaken / Definition->BreakDamage, 0.f, 1.f);
}

EknightPartCondition UknightMonsterPartComponent::GetCondition(int32 PartIndex) const
{
	const FknightMonsterPartDefinition* const Definition = GetDefinition(PartIndex);
	if (!Definition || !Definition->bBreakable)
	{
		return EknightPartCondition::Unbreakable;
	}
	if (PartStates[PartIndex].bBroken)
	{
		return EknightPartCondition::Broken;
	}
	return GetHealthRatio(PartIndex) <= Definition->CriticalRatio ? EknightPartCondition::Critical : EknightPartCondition::Normal;
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
