#include "Character/knightPartHealthComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "Item/knightPartsComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "MechPartAttachment.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "System/knightGameplayTags.h"
#include "TimerManager.h"
#include "knight.h"

// ---- 순수 계산 (테스트 대상) ----

namespace knightPartHealth
{
	void DistributeDamage(float (&InOutHealth)[PartCount], float Damage, int32 HitPart)
	{
		if (Damage <= 0.f)
		{
			return;
		}

		// 부위를 아는 피해: 그 부위에서만 뺀다.
		if (HitPart >= 0 && HitPart < PartCount)
		{
			InOutHealth[HitPart] = FMath::Max(0.f, InOutHealth[HitPart] - Damage);
			return;
		}

		// 부위를 모르는 피해: 파괴되지 않은(체력 > 0) 부위에 똑같이 나눈다.
		int32 AliveCount = 0;
		for (const float Health : InOutHealth)
		{
			AliveCount += Health > 0.f ? 1 : 0;
		}
		if (AliveCount == 0)
		{
			return;
		}
		const float Share = Damage / AliveCount;
		for (float& Health : InOutHealth)
		{
			if (Health > 0.f)
			{
				Health = FMath::Max(0.f, Health - Share);
			}
		}
	}

	void DistributeRepair(float (&InOutHealth)[PartCount], const float (&MaxHealth)[PartCount], float Amount)
	{
		if (Amount <= 0.f)
		{
			return;
		}

		// 채우기 전의 체력 비율로 순서를 정한다. 비율이 같으면 앞 순서가 먼저(삽입 정렬이라 순서 유지).
		auto Ratio = [&InOutHealth, &MaxHealth](int32 Index)
		{
			return MaxHealth[Index] > 0.f ? InOutHealth[Index] / MaxHealth[Index] : 1.f;
		};
		int32 Order[PartCount];
		for (int32 Index = 0; Index < PartCount; ++Index)
		{
			int32 Slot = Index;
			while (Slot > 0 && Ratio(Order[Slot - 1]) > Ratio(Index))
			{
				Order[Slot] = Order[Slot - 1];
				--Slot;
			}
			Order[Slot] = Index;
		}

		// 낮은 부위부터 최대치까지 채우고, 남은 양은 다음 부위로 넘긴다.
		for (const int32 Index : Order)
		{
			const float Added = FMath::Min(Amount, FMath::Max(0.f, MaxHealth[Index] - InOutHealth[Index]));
			InOutHealth[Index] += Added;
			Amount -= Added;
			if (Amount <= 0.f)
			{
				break;
			}
		}
	}
}

// ---- 부위 ↔ 속성·태그 ----

namespace
{
	FGameplayAttribute GetPartHealthAttribute(EknightBodyPart Part)
	{
		switch (Part)
		{
		case EknightBodyPart::Head:  return UknightAttributeSet::GetHeadHealthAttribute();
		case EknightBodyPart::Torso: return UknightAttributeSet::GetTorsoHealthAttribute();
		case EknightBodyPart::Arms:  return UknightAttributeSet::GetArmsHealthAttribute();
		default:                     return UknightAttributeSet::GetLegsHealthAttribute();
		}
	}

	FGameplayAttribute GetPartMaxHealthAttribute(EknightBodyPart Part)
	{
		switch (Part)
		{
		case EknightBodyPart::Head:  return UknightAttributeSet::GetMaxHeadHealthAttribute();
		case EknightBodyPart::Torso: return UknightAttributeSet::GetMaxTorsoHealthAttribute();
		case EknightBodyPart::Arms:  return UknightAttributeSet::GetMaxArmsHealthAttribute();
		default:                     return UknightAttributeSet::GetMaxLegsHealthAttribute();
		}
	}

	EknightBodyPart PartFromIndex(int32 Index)
	{
		return static_cast<EknightBodyPart>(Index);
	}
}

FGameplayTag UknightPartHealthComponent::GetPartTag(EknightBodyPart Part)
{
	switch (Part)
	{
	case EknightBodyPart::Head:  return knightGameplayTags::Knight_Part_Head;
	case EknightBodyPart::Torso: return knightGameplayTags::Knight_Part_Torso;
	case EknightBodyPart::Arms:  return knightGameplayTags::Knight_Part_Arms;
	default:                     return knightGameplayTags::Knight_Part_Legs;
	}
}

FGameplayTag UknightPartHealthComponent::GetBrokenTag(EknightBodyPart Part)
{
	switch (Part)
	{
	case EknightBodyPart::Head:  return knightGameplayTags::State_PartBroken_Head;
	case EknightBodyPart::Torso: return knightGameplayTags::State_PartBroken_Torso;
	case EknightBodyPart::Arms:  return knightGameplayTags::State_PartBroken_Arms;
	default:                     return knightGameplayTags::State_PartBroken_Legs;
	}
}

// ---- 컴포넌트 ----

UknightPartHealthComponent::UknightPartHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 나이트 메시(/Game/Knight/Character/Knights/Knight, 마네킹식 본) 기준 본 → 부위 기본값.
	// BP_Knight에 저장해 두었더니 BP를 다른 버전으로 덮어쓸 때 사라져서 C++ 기본값으로 옮겼다.
	// 실제로 맞는 본은 물리 에셋(Knight_PhysicsAsset)에 바디가 있는 본뿐이고, 여기 없는 본은 DefaultHitPart(몸통).
	// 어깨(clavicle)는 팔로 친다. 손가락·root·attach·가상 본(VB)은 바디가 없어 뺐다.
	auto Map = [this](std::initializer_list<const TCHAR*> Bones, EknightBodyPart Part)
	{
		for (const TCHAR* const Bone : Bones)
		{
			BoneToPart.Add(FName(Bone), Part);
		}
	};
	Map({ TEXT("neck_01"), TEXT("head") }, EknightBodyPart::Head);
	Map({ TEXT("pelvis"), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"), TEXT("spine_04"), TEXT("spine_05"),
		TEXT("breast_l"), TEXT("breast_r") }, EknightBodyPart::Torso);
	Map({ TEXT("clavicle_l"), TEXT("upperarm_l"), TEXT("upperarm_twist_01_l"), TEXT("lowerarm_l"), TEXT("lowerarm_twist_01_l"), TEXT("hand_l"),
		TEXT("clavicle_r"), TEXT("upperarm_r"), TEXT("upperarm_twist_01_r"), TEXT("lowerarm_r"), TEXT("lowerarm_twist_01_r"), TEXT("hand_r") }, EknightBodyPart::Arms);
	Map({ TEXT("pelvis_l"), TEXT("thigh_l"), TEXT("thigh_twist_01_l"), TEXT("calf_l"), TEXT("calf_twist_01_l"), TEXT("foot_l"), TEXT("ball_l"),
		TEXT("pelvis_r"), TEXT("thigh_r"), TEXT("thigh_twist_01_r"), TEXT("calf_r"), TEXT("calf_twist_01_r"), TEXT("foot_r"), TEXT("ball_r") }, EknightBodyPart::Legs);

	// 파괴 연출 위치: 머리, 가슴, 양쪽 팔뚝, 양쪽 종아리.
	auto Bones = [this](EknightBodyPart Part, std::initializer_list<const TCHAR*> Names)
	{
		FknightPartEffectBones& Entry = PartEffectBones.Add(Part);
		for (const TCHAR* const Name : Names)
		{
			Entry.Bones.Add(FName(Name));
		}
	};
	Bones(EknightBodyPart::Head, { TEXT("head") });
	Bones(EknightBodyPart::Torso, { TEXT("spine_03") });
	Bones(EknightBodyPart::Arms, { TEXT("lowerarm_l"), TEXT("lowerarm_r") });
	Bones(EknightBodyPart::Legs, { TEXT("calf_l"), TEXT("calf_r") });
}

void UknightPartHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* const World = GetWorld())
	{
		for (FTimerHandle& Timer : SparkTimers)
		{
			World->GetTimerManager().ClearTimer(Timer);
		}
	}
	for (const TWeakObjectPtr<UMeshComponent>& Mesh : OverlaidMeshes)
	{
		if (Mesh.IsValid() && Mesh->GetOverlayMaterial() == BrokenOverlayMaterial)
		{
			Mesh->SetOverlayMaterial(nullptr);
		}
	}
	OverlaidMeshes.Reset();
	Super::EndPlay(EndPlayReason);
}

void UknightPartHealthComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InASC)
{
	// 처음 한 번만 연결한다 (재빙의로 다시 불려도 중복 연결하지 않음).
	if (!InASC || ASC.IsValid())
	{
		return;
	}
	ASC = InASC;

	if (const UknightCoreAttributeSet* const CoreSet = InASC->GetSet<UknightCoreAttributeSet>())
	{
		CoreSet->OnDamaged.AddUObject(this, &ThisClass::HandleDamaged);
	}
	for (int32 Index = 0; Index < knightPartHealth::PartCount; ++Index)
	{
		const EknightBodyPart Part = PartFromIndex(Index);
		InASC->GetGameplayAttributeValueChangeDelegate(GetPartHealthAttribute(Part)).AddUObject(this, &ThisClass::HandlePartHealthChanged, Part);
		InASC->GetGameplayAttributeValueChangeDelegate(GetPartMaxHealthAttribute(Part)).AddWeakLambda(this,
			[this](const FOnAttributeChangeData&) { SyncMaxHealth(); });
	}
	SyncMaxHealth();
}

FGameplayTag UknightPartHealthComponent::ResolveHitPartTag(const FHitResult& Hit) const
{
	// 캡슐 히트도 가까운 본으로 부위를 준다. 그래서 같은 프레임에 캡슐과 몸체가 함께 맞으면
	// SelectBestHitPerActor가 먼저 온 쪽(보통 바깥의 캡슐)을 고르지만, 가까운 본 결과라 부위는 거의 같다.
	EknightBodyPart Part = DefaultHitPart;
	FName Bone;
	return FindHitPart(Hit, Part, Bone) ? GetPartTag(Part) : FGameplayTag();
}

bool UknightPartHealthComponent::FindHitPart(const FHitResult& Hit, EknightBodyPart& OutPart, FName& OutBone) const
{
	OutBone = Hit.BoneName;
	if (const EknightBodyPart* const Found = BoneToPart.Find(Hit.BoneName))
	{
		OutPart = *Found;
		return true;
	}

	// 본이 없는 히트(이동 캡슐): 맞은 지점에서 가장 가까운 몸체 본(물리 에셋에 바디가 있는 본)으로 정한다.
	const ACharacter* const Character = Cast<ACharacter>(GetOwner());
	const USkeletalMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return false;
	}
	OutBone = Mesh->FindClosestBone(Hit.ImpactPoint, nullptr, 0.f, true);
	if (const EknightBodyPart* const Closest = BoneToPart.Find(OutBone))
	{
		OutPart = *Closest;
		return true;
	}
	return false;
}

float UknightPartHealthComponent::GetDamageMultiplier(const FGameplayTag& PartTag) const
{
	float Multiplier = 1.f;
	for (int32 Index = 0; Index < knightPartHealth::PartCount; ++Index)
	{
		const EknightBodyPart Part = PartFromIndex(Index);
		// 파괴된 부위를 또 맞으면 크리티컬.
		if (PartTag == GetPartTag(Part) && IsPartBroken(Part))
		{
			Multiplier *= BrokenPartHitMultiplier;
		}
	}
	// 머리가 파괴돼 있으면 어디를 맞든 피해 증가.
	if (IsPartBroken(EknightBodyPart::Head))
	{
		Multiplier *= HeadBrokenDamageMultiplier;
	}
	return Multiplier;
}

bool UknightPartHealthComponent::IsPartBroken(EknightBodyPart Part) const
{
	return ASC.IsValid() && ASC->HasMatchingGameplayTag(GetBrokenTag(Part));
}

int32 UknightPartHealthComponent::GetBrokenPartCount() const
{
	int32 Count = 0;
	for (int32 Index = 0; Index < knightPartHealth::PartCount; ++Index)
	{
		Count += IsPartBroken(PartFromIndex(Index)) ? 1 : 0;
	}
	return Count;
}

void UknightPartHealthComponent::ApplyRepair(float Amount)
{
	UAbilitySystemComponent* const OwnerASC = ASC.Get();
	if (!OwnerASC || Amount <= 0.f)
	{
		return;
	}

	// 부위 체력은 기본값(Base)을 바꾸고, 최대치는 파츠 효과가 반영된 현재값을 쓴다(속성 세트의 상한과 같은 기준).
	float Health[knightPartHealth::PartCount];
	float MaxHealth[knightPartHealth::PartCount];
	for (int32 Index = 0; Index < knightPartHealth::PartCount; ++Index)
	{
		const EknightBodyPart Part = PartFromIndex(Index);
		Health[Index] = OwnerASC->GetNumericAttributeBase(GetPartHealthAttribute(Part));
		MaxHealth[Index] = OwnerASC->GetNumericAttribute(GetPartMaxHealthAttribute(Part));
	}
	const float Before[knightPartHealth::PartCount] = { Health[0], Health[1], Health[2], Health[3] };

	knightPartHealth::DistributeRepair(Health, MaxHealth, Amount);

	for (int32 Index = 0; Index < knightPartHealth::PartCount; ++Index)
	{
		if (Health[Index] != Before[Index])
		{
			// 파괴된 부위가 1 이상이 되면 HandlePartHealthChanged가 복구한다.
			OwnerASC->SetNumericAttributeBase(GetPartHealthAttribute(PartFromIndex(Index)), Health[Index]);
		}
	}
	UE_LOG(Logknight, Log, TEXT("%s: part repair %.1f. Head %.1f  Torso %.1f  Arms %.1f  Legs %.1f"),
		*GetNameSafe(GetOwner()), Amount, Health[0], Health[1], Health[2], Health[3]);
}

bool UknightPartHealthComponent::HasDamagedPart() const
{
	const UAbilitySystemComponent* const OwnerASC = ASC.Get();
	if (!OwnerASC)
	{
		return false;
	}
	for (int32 Index = 0; Index < knightPartHealth::PartCount; ++Index)
	{
		const EknightBodyPart Part = PartFromIndex(Index);
		if (OwnerASC->GetNumericAttribute(GetPartHealthAttribute(Part)) < OwnerASC->GetNumericAttribute(GetPartMaxHealthAttribute(Part)))
		{
			return true;
		}
	}
	return false;
}

void UknightPartHealthComponent::HandleDamaged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* EffectSpec,
	float Magnitude, float OldValue, float NewValue)
{
	UAbilitySystemComponent* const OwnerASC = ASC.Get();
	if (!OwnerASC)
	{
		return;
	}

	// 히트 정보가 있으면 맞은 부위, 없으면 -1(나눠 받기).
	int32 HitPart = -1;
	FString HitText = TEXT("no hit info -> spread");
	if (const FHitResult* const Hit = EffectSpec ? EffectSpec->GetContext().GetHitResult() : nullptr)
	{
		EknightBodyPart Part = DefaultHitPart;
		FName Bone;
		const bool bFound = FindHitPart(*Hit, Part, Bone);
		HitPart = static_cast<int32>(bFound ? Part : DefaultHitPart);
		HitText = FString::Printf(TEXT("bone %s%s -> %s%s"), *Hit->BoneName.ToString(),
			bFound && Bone != Hit->BoneName ? *FString::Printf(TEXT(" (closest %s)"), *Bone.ToString()) : TEXT(""),
			*GetPartTag(bFound ? Part : DefaultHitPart).ToString(), bFound ? TEXT("") : TEXT(" (default)"));
	}

	float Health[knightPartHealth::PartCount];
	for (int32 Index = 0; Index < knightPartHealth::PartCount; ++Index)
	{
		Health[Index] = OwnerASC->GetNumericAttributeBase(GetPartHealthAttribute(PartFromIndex(Index)));
	}
	const float Before[knightPartHealth::PartCount] = { Health[0], Health[1], Health[2], Health[3] };

	// Magnitude = 이번 피격으로 실제 줄어든 전체 체력. 같은 양을 부위에서 뺀다.
	knightPartHealth::DistributeDamage(Health, Magnitude, HitPart);

	for (int32 Index = 0; Index < knightPartHealth::PartCount; ++Index)
	{
		if (Health[Index] != Before[Index])
		{
			// 파괴·복구 판정은 값이 바뀔 때 HandlePartHealthChanged가 한다.
			OwnerASC->SetNumericAttributeBase(GetPartHealthAttribute(PartFromIndex(Index)), Health[Index]);
		}
	}

	// 부위 판정 확인용: 맞은 본 → 부위, 피해량, 피해 뒤 부위 체력. 본이 없거나 매핑에 없는 본이면 (default) = DefaultHitPart.
	UE_LOG(Logknight, Log, TEXT("%s: part damage %.1f by %s, %s. Head %.1f  Torso %.1f  Arms %.1f  Legs %.1f"),
		*GetNameSafe(GetOwner()), Magnitude, *GetNameSafe(DamageInstigator), *HitText, Health[0], Health[1], Health[2], Health[3]);
}

void UknightPartHealthComponent::HandlePartHealthChanged(const FOnAttributeChangeData& Data, EknightBodyPart Part)
{
	const bool bBroken = IsPartBroken(Part);
	if (Data.NewValue <= 0.f && !bBroken)
	{
		BreakPart(Part);
	}
	else if (Data.NewValue >= 1.f && bBroken)
	{
		RepairPart(Part);
	}
}

void UknightPartHealthComponent::SyncMaxHealth()
{
	UAbilitySystemComponent* const OwnerASC = ASC.Get();
	if (!OwnerASC || !OwnerASC->GetSet<UknightCoreAttributeSet>())
	{
		return;
	}
	float Sum = 0.f;
	for (int32 Index = 0; Index < knightPartHealth::PartCount; ++Index)
	{
		Sum += OwnerASC->GetNumericAttribute(GetPartMaxHealthAttribute(PartFromIndex(Index)));
	}
	OwnerASC->SetNumericAttributeBase(UknightCoreAttributeSet::GetMaxHealthAttribute(), Sum);
}

void UknightPartHealthComponent::BreakPart(EknightBodyPart Part)
{
	UAbilitySystemComponent* const OwnerASC = ASC.Get();
	if (!OwnerASC)
	{
		return;
	}
	OwnerASC->AddLooseGameplayTag(GetBrokenTag(Part));
	if (const UGameplayEffect* const Penalty = GetPenaltyEffect(Part))
	{
		PenaltyHandles[static_cast<int32>(Part)] = OwnerASC->ApplyGameplayEffectToSelf(Penalty, 1.f, OwnerASC->MakeEffectContext());
	}
	UE_LOG(Logknight, Log, TEXT("%s: part %s broken (%d broken)."), *GetNameSafe(GetOwner()), *GetPartTag(Part).ToString(), GetBrokenPartCount());
	PlayBreakFeedback(Part);
	StartBrokenVisuals(Part);
}

void UknightPartHealthComponent::RepairPart(EknightBodyPart Part)
{
	UAbilitySystemComponent* const OwnerASC = ASC.Get();
	if (!OwnerASC)
	{
		return;
	}
	OwnerASC->RemoveLooseGameplayTag(GetBrokenTag(Part));
	FActiveGameplayEffectHandle& Handle = PenaltyHandles[static_cast<int32>(Part)];
	if (Handle.IsValid())
	{
		OwnerASC->RemoveActiveGameplayEffect(Handle);
		Handle.Invalidate();
	}
	UE_LOG(Logknight, Log, TEXT("%s: part %s repaired (%d broken)."), *GetNameSafe(GetOwner()), *GetPartTag(Part).ToString(), GetBrokenPartCount());
	StopBrokenVisuals(Part);
}

void UknightPartHealthComponent::PlayBreakFeedback(EknightBodyPart Part) const
{
	const ACharacter* const Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
	UWorld* const World = GetWorld();
	if (!Mesh || !World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	FVector SoundLocation = Character->GetActorLocation();
	if (const FknightPartEffectBones* const Bones = PartEffectBones.Find(Part))
	{
		for (const FName Bone : Bones->Bones)
		{
			if (!Mesh->DoesSocketExist(Bone))
			{
				continue;
			}
			SoundLocation = Mesh->GetSocketLocation(Bone);
			if (BreakEffect)
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, BreakEffect, SoundLocation, Character->GetActorRotation(),
					FVector(BreakEffectScale), true, true, ENCPoolMethod::AutoRelease);
			}
		}
	}
	if (BreakSound && BreakSoundVolume > 0.f)
	{
		UGameplayStatics::PlaySoundAtLocation(World, BreakSound, SoundLocation, BreakSoundVolume);
	}
	if (BreakShake && BreakShakeScale > 0.f && Character->IsLocallyControlled())
	{
		const APlayerController* const PC = Character->GetController<APlayerController>();
		if (PC && PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraShake(BreakShake, BreakShakeScale);
		}
	}
}

void UknightPartHealthComponent::StartBrokenVisuals(EknightBodyPart Part)
{
	RefreshBrokenOverlay();
	UWorld* const World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(SparkTimers[static_cast<int32>(Part)]);
	}
	if (BrokenSparkEffect && World && World->GetNetMode() != NM_DedicatedServer)
	{
		World->GetTimerManager().SetTimer(SparkTimers[static_cast<int32>(Part)], FTimerDelegate::CreateUObject(this, &ThisClass::SpawnBrokenSpark, Part),
			FMath::FRandRange(BrokenSparkInterval.X, FMath::Max(BrokenSparkInterval.X, BrokenSparkInterval.Y)), false);
	}
}

void UknightPartHealthComponent::RefreshBrokenOverlay()
{
	// 메시마다 재질이 켤 칸: 비트 0~3 = 머리·몸통·팔·다리(재질이 메시 위치로 그 부위만 그을림), 비트 4 = 메시 전체.
	constexpr uint8 WholeMesh = 1 << knightPartHealth::PartCount;
	TMap<UMeshComponent*, uint8> Wanted;
	const ACharacter* const Character = Cast<ACharacter>(GetOwner());
	const UknightPartsComponent* const Parts = Character ? Character->FindComponentByClass<UknightPartsComponent>() : nullptr;
	// 원본 부위 메시가 있으면 그 메시들이 몸을 그리고, 없으면(부위 분할 데이터가 메시와 안 맞음) 본체 한 덩어리가 그린다.
	const bool bRegionMeshes = Parts && Parts->HasBodyRegionMeshes();
	for (int32 Index = 0; Index < knightPartHealth::PartCount; ++Index)
	{
		const EknightBodyPart Part = PartFromIndex(Index);
		if (!IsPartBroken(Part))
		{
			continue;
		}
		TArray<UMeshComponent*> Meshes;
		GetMeshesForPart(Part, Meshes);
		for (UMeshComponent* const Mesh : Meshes)
		{
			Wanted.FindOrAdd(Mesh) |= WholeMesh;
		}
		if (!bRegionMeshes && Character && Character->GetMesh())
		{
			Wanted.FindOrAdd(Character->GetMesh()) |= static_cast<uint8>(1 << Index);
		}
	}

	// 더 그을릴 필요가 없는 메시는 되돌린다.
	for (const TWeakObjectPtr<UMeshComponent>& Mesh : OverlaidMeshes)
	{
		if (Mesh.IsValid() && !Wanted.Contains(Mesh.Get()) && Mesh->GetOverlayMaterial() == BrokenOverlayMaterial)
		{
			Mesh->SetOverlayMaterial(nullptr);
		}
	}
	OverlaidMeshes.Reset();
	if (!BrokenOverlayMaterial)
	{
		return;
	}
	for (const TPair<UMeshComponent*, uint8>& Entry : Wanted)
	{
		UMeshComponent* const Mesh = Entry.Key;
		// 다른 연출이 겹친 재질을 쓰고 있으면 덮지 않는다.
		if (!Mesh || (Mesh->GetOverlayMaterial() && Mesh->GetOverlayMaterial() != BrokenOverlayMaterial))
		{
			continue;
		}
		for (int32 Bit = 0; Bit <= knightPartHealth::PartCount; ++Bit)
		{
			Mesh->SetCustomPrimitiveDataFloat(OverlayDataIndex + Bit, (Entry.Value & (1 << Bit)) ? 1.f : 0.f);
		}
		Mesh->SetOverlayMaterial(BrokenOverlayMaterial);
		OverlaidMeshes.Add(Mesh);
	}
}

void UknightPartHealthComponent::StopBrokenVisuals(EknightBodyPart Part)
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SparkTimers[static_cast<int32>(Part)]);
	}
	RefreshBrokenOverlay();
}

void UknightPartHealthComponent::SpawnBrokenSpark(EknightBodyPart Part)
{
	UWorld* const World = GetWorld();
	const ACharacter* const Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
	if (!World || !Mesh || !BrokenSparkEffect || !IsPartBroken(Part))
	{
		return;
	}
	// 그 부위 뼈 중 하나를 골라 붙여서 튀긴다(움직이는 동안에도 그 자리에서 나오게).
	const FknightPartEffectBones* const Bones = PartEffectBones.Find(Part);
	if (Bones && !Bones->Bones.IsEmpty())
	{
		const FName Bone = Bones->Bones[FMath::RandRange(0, Bones->Bones.Num() - 1)];
		if (Mesh->DoesSocketExist(Bone))
		{
			UNiagaraFunctionLibrary::SpawnSystemAttached(BrokenSparkEffect, Mesh, Bone, FVector::ZeroVector, FRotator::ZeroRotator,
				FVector(BrokenSparkScale), EAttachLocation::SnapToTarget, true, ENCPoolMethod::AutoRelease, true, true);
			if (BrokenSparkSound && BrokenSparkSoundVolume > 0.f)
			{
				UGameplayStatics::PlaySoundAtLocation(World, BrokenSparkSound, Mesh->GetSocketLocation(Bone), BrokenSparkSoundVolume);
			}
		}
	}
	World->GetTimerManager().SetTimer(SparkTimers[static_cast<int32>(Part)], FTimerDelegate::CreateUObject(this, &ThisClass::SpawnBrokenSpark, Part),
		FMath::FRandRange(BrokenSparkInterval.X, FMath::Max(BrokenSparkInterval.X, BrokenSparkInterval.Y)), false);
}

void UknightPartHealthComponent::GetMeshesForPart(EknightBodyPart Part, TArray<UMeshComponent*>& OutMeshes) const
{
	const UknightPartsComponent* const Parts = GetOwner() ? GetOwner()->FindComponentByClass<UknightPartsComponent>() : nullptr;
	if (!Parts)
	{
		return;
	}
	EMechBodyRegion Regions;
	FGameplayTag Slot;
	switch (Part)
	{
	case EknightBodyPart::Head:  Regions = EMechBodyRegion::Head; Slot = knightGameplayTags::Equipment_Slot_Head; break;
	case EknightBodyPart::Torso: Regions = EMechBodyRegion::Torso; Slot = knightGameplayTags::Equipment_Slot_Torso; break;
	case EknightBodyPart::Arms:  Regions = EMechBodyRegion::ArmLeft | EMechBodyRegion::ArmRight; Slot = knightGameplayTags::Equipment_Slot_Arms; break;
	default:                     Regions = EMechBodyRegion::LegLeft | EMechBodyRegion::LegRight; Slot = knightGameplayTags::Equipment_Slot_Legs; break;
	}
	Parts->GetMeshesForBodyRegion(static_cast<int32>(Regions), Slot, OutMeshes);
}

const UGameplayEffect* UknightPartHealthComponent::GetPenaltyEffect(EknightBodyPart Part)
{
	// 머리는 스탯이 아니라 받는 피해 배율(GetDamageMultiplier)로 처리하므로 GE가 없다.
	if (Part == EknightBodyPart::Head)
	{
		return nullptr;
	}

	PenaltyEffects.SetNum(knightPartHealth::PartCount);
	TObjectPtr<UGameplayEffect>& Effect = PenaltyEffects[static_cast<int32>(Part)];
	if (Effect)
	{
		return Effect;
	}

	// 무한 지속 GE를 코드에서 한 번 만들어 재사용한다. 스탯에 (1 - 페널티)를 곱한다.
	auto MakePenalty = [this](const TCHAR* Name, std::initializer_list<FGameplayAttribute> Attributes)
	{
		UGameplayEffect* const NewEffect = NewObject<UGameplayEffect>(this, Name);
		NewEffect->DurationPolicy = EGameplayEffectDurationType::Infinite;
		for (const FGameplayAttribute& Attribute : Attributes)
		{
			FGameplayModifierInfo Modifier;
			Modifier.Attribute = Attribute;
			Modifier.ModifierOp = EGameplayModOp::MultiplyCompound;
			Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.f - BrokenStatPenalty));
			NewEffect->Modifiers.Add(Modifier);
		}
		return NewEffect;
	};

	switch (Part)
	{
	case EknightBodyPart::Torso:
		Effect = MakePenalty(TEXT("GE_PartBroken_Torso"), { UknightCoreAttributeSet::GetMaxPoiseAttribute() });
		break;
	case EknightBodyPart::Arms:
		Effect = MakePenalty(TEXT("GE_PartBroken_Arms"), { UknightCoreAttributeSet::GetAttackPowerAttribute() });
		break;
	default:
		Effect = MakePenalty(TEXT("GE_PartBroken_Legs"), { UknightAttributeSet::GetWalkSpeedAttribute(), UknightAttributeSet::GetRunSpeedAttribute() });
		break;
	}
	return Effect;
}
