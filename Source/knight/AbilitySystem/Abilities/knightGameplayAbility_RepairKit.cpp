#include "AbilitySystem/Abilities/knightGameplayAbility_RepairKit.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "AbilitySystem/Effects/knightGameplayEffect_Heal.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Character/knightMovementComponent.h"
#include "Character/knightPartHealthComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "System/knightGameplayTags.h"
#include "TimerManager.h"
#include "knight.h"
#include "knightAudioSubsystem.h"

namespace knightRepairFX
{
	/** 부위 체력 비율(현재 / 최대). 부위 속성이 없으면 1(가득). knightPartHealthComponent.cpp의 부위 ↔ 속성 대응과 같다. */
	float GetPartHealthRatio(const UAbilitySystemComponent* ASC, EknightBodyPart Part)
	{
		FGameplayAttribute Health;
		FGameplayAttribute MaxHealth;
		switch (Part)
		{
		case EknightBodyPart::Head:
			Health = UknightAttributeSet::GetHeadHealthAttribute();
			MaxHealth = UknightAttributeSet::GetMaxHeadHealthAttribute();
			break;
		case EknightBodyPart::Torso:
			Health = UknightAttributeSet::GetTorsoHealthAttribute();
			MaxHealth = UknightAttributeSet::GetMaxTorsoHealthAttribute();
			break;
		case EknightBodyPart::Arms:
			Health = UknightAttributeSet::GetArmsHealthAttribute();
			MaxHealth = UknightAttributeSet::GetMaxArmsHealthAttribute();
			break;
		default:
			Health = UknightAttributeSet::GetLegsHealthAttribute();
			MaxHealth = UknightAttributeSet::GetMaxLegsHealthAttribute();
			break;
		}
		if (!ASC || !ASC->HasAttributeSetForAttribute(Health))
		{
			return 1.f;
		}
		const float Max = ASC->GetNumericAttribute(MaxHealth);
		return Max > 0.f ? ASC->GetNumericAttribute(Health) / Max : 1.f;
	}
}

UknightGameplayAbility_RepairKit::UknightGameplayAbility_RepairKit(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EknightAbilityActivationPolicy::OnInputTriggered;
	// 교체 가능 그룹: 공격 도중에는 캔슬 창에서만 나가고, 수리 중에는 캔슬 창을 열지 않으므로 공격·가드·퀵스텝이 막힌다.
	ActivationGroup = EknightAbilityActivationGroup::Exclusive_Replaceable;
	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_Item_RepairKit));
	ActivationOwnedTags.AddTag(knightGameplayTags::State_Repairing);
	// 맞아도 계속(사용자 결정): 작은 피격 경직을 막는다. 피격 경직 어빌리티는 이 어빌리티를 취소하지 않는다.
	ActivationOwnedTags.AddTag(knightGameplayTags::State_SuperArmor);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_HitReact);

	HealEffect = UknightGameplayEffect_Heal::StaticClass();

	// 용접·증기 자리 기본값: 나이트 메시(마네킹식 본). 메시가 바뀌면 BP에서 수정. 없는 본은 건너뛴다.
	auto AddWeld = [this](EknightBodyPart Part, std::initializer_list<const TCHAR*> Bones)
	{
		for (const TCHAR* const Bone : Bones)
		{
			FknightRepairWeldPoint& Point = WeldPoints.AddDefaulted_GetRef();
			Point.Part = Part;
			Point.Bone = FName(Bone);
		}
	};
	AddWeld(EknightBodyPart::Head, { TEXT("head"), TEXT("neck_01") });
	AddWeld(EknightBodyPart::Torso, { TEXT("spine_03"), TEXT("spine_05"), TEXT("pelvis") });
	AddWeld(EknightBodyPart::Arms, { TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("upperarm_r"), TEXT("lowerarm_r") });
	AddWeld(EknightBodyPart::Legs, { TEXT("thigh_l"), TEXT("calf_l"), TEXT("thigh_r"), TEXT("calf_r") });

	RebootSteamBones = { FName(TEXT("clavicle_l")), FName(TEXT("clavicle_r")), FName(TEXT("calf_l")), FName(TEXT("calf_r")) };
}

bool UknightGameplayAbility_RepairKit::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	// 풀피면 쓰지 않는다(2026-10-09 사용자 결정, 키트 낭비 방지). 전체 체력이 가득 차 있어도 깎인 부위가 있으면 쓸 수 있다.
	const UAbilitySystemComponent* const ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UknightCoreAttributeSet* const CoreSet = ASC ? ASC->GetSet<UknightCoreAttributeSet>() : nullptr;
	if (!CoreSet || CoreSet->GetHealth() < CoreSet->GetMaxHealth())
	{
		return true;
	}
	const AActor* const Avatar = ActorInfo->AvatarActor.Get();
	const UknightPartHealthComponent* const PartHealth = Avatar ? Avatar->FindComponentByClass<UknightPartHealthComponent>() : nullptr;
	return PartHealth && PartHealth->HasDamagedPart();
}

bool UknightGameplayAbility_RepairKit::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}
	const UAbilitySystemComponent* const ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAttribute Attribute = UknightAttributeSet::GetRepairKitCountAttribute();
	const bool bHasKit = ASC && ASC->HasAttributeSetForAttribute(Attribute) && ASC->GetNumericAttribute(Attribute) >= 1.f;
	if (!bHasKit && OptionalRelevantTags)
	{
		OptionalRelevantTags->AddTag(knightGameplayTags::Ability_ActivateFail_Cost);
	}
	return bHasKit;
}

void UknightGameplayAbility_RepairKit::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	UAbilitySystemComponent* const ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAttribute Attribute = UknightAttributeSet::GetRepairKitCountAttribute();
	if (ASC && ASC->HasAttributeSetForAttribute(Attribute))
	{
		// 개수는 정수로 쓴다. 속성 세트가 0 아래로 내려가지 않게 자른다.
		ASC->SetNumericAttributeBase(Attribute, FMath::FloorToFloat(ASC->GetNumericAttributeBase(Attribute)) - 1.f);
	}
}

void UknightGameplayAbility_RepairKit::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 걷기만: 달리기 키를 누르고 있어도 걷기 속도.
	if (UknightMovementComponent* const Movement = Cast<UknightMovementComponent>(Character->GetCharacterMovement()))
	{
		Movement->SetRunLocked(true);
		bRunLockApplied = true;
	}

	// 사용 시간 = 기본 + 파괴된 부위 수 x 부위당 시간. 시작할 때 정한다(사용 중에 부위가 파괴돼도 늘지 않음).
	const UknightPartHealthComponent* const PartHealth = Character->FindComponentByClass<UknightPartHealthComponent>();
	const int32 BrokenParts = PartHealth ? PartHealth->GetBrokenPartCount() : 0;
	const float UseTime = BaseUseTime + BrokenParts * TimePerBrokenPart;

	// 연출: 수리 중 반복 큐. 어빌리티가 끝나면(사망으로 취소될 때 포함) 자동으로 지워진다. 에셋은 GCN_Repair_Loop(임시 이펙트).
	K2_AddGameplayCue(knightGameplayTags::GameplayCue_Repair_Loop, MakeEffectContext(Handle, ActorInfo), true);

	if (RepairMontage)
	{
		UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, RepairMontage, 1.f, NAME_None, true);
		MontageTask->ReadyForActivation();
	}

	StartWeld(Character, UseTime);
	StartScan(Character, UseTime);

	UAbilityTask_WaitDelay* const DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, UseTime);
	DelayTask->OnFinish.AddDynamic(this, &ThisClass::OnRepairFinished);
	DelayTask->ReadyForActivation();

	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	UE_LOG(LogknightAbility, Log, TEXT("%s: repair kit started, use time %.2fs (%d broken parts), kits left %.0f."),
		*GetNameSafe(Character), UseTime, BrokenParts,
		ASC ? ASC->GetNumericAttribute(UknightAttributeSet::GetRepairKitCountAttribute()) : 0.f);
}

void UknightGameplayAbility_RepairKit::OnRepairFinished()
{
	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	const UknightCoreAttributeSet* const CoreSet = ASC ? ASC->GetSet<UknightCoreAttributeSet>() : nullptr;
	if (CoreSet)
	{
		const FGameplayAttribute EfficiencyAttribute = UknightAttributeSet::GetRepairEfficiencyAttribute();
		const float Efficiency = ASC->HasAttributeSetForAttribute(EfficiencyAttribute) ? ASC->GetNumericAttribute(EfficiencyAttribute) : 1.f;
		const float Amount = CoreSet->GetMaxHealth() * HealRatio * Efficiency;

		// 전체 체력: 회복 이펙트(Healing 메타 속성 → Health). 부위가 가득 차 있어도 채운다(기획 문서).
		if (HealEffect && Amount > 0.f)
		{
			const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(HealEffect);
			if (SpecHandle.IsValid())
			{
				SpecHandle.Data->SetSetByCallerMagnitude(knightGameplayTags::SetByCaller_Healing, Amount);
				ApplyGameplayEffectSpecToOwner(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), SpecHandle);
			}
		}
		// 부위 체력: 같은 양을 체력 비율이 낮은 부위부터.
		const AActor* const Avatar = GetAvatarActorFromActorInfo();
		if (UknightPartHealthComponent* const PartHealth = Avatar ? Avatar->FindComponentByClass<UknightPartHealthComponent>() : nullptr)
		{
			PartHealth->ApplyRepair(Amount);
		}
		UE_LOG(LogknightAbility, Log, TEXT("%s: repair kit finished, healed %.1f (max health %.1f x %.2f x efficiency %.2f), health %.1f."),
			*GetNameSafe(Avatar), Amount, CoreSet->GetMaxHealth(), HealRatio, Efficiency, CoreSet->GetHealth());

		// 연출: 회복 순간 한 번. 에셋은 GCN_Repair_Finish(임시 이펙트).
		if (Avatar)
		{
			FGameplayCueParameters CueParams;
			CueParams.Location = Avatar->GetActorLocation();
			CueParams.Normal = FVector::UpVector;
			CueParams.Instigator = GetAvatarActorFromActorInfo();
			CueParams.RawMagnitude = Amount;
			K2_ExecuteGameplayCueWithParams(knightGameplayTags::GameplayCue_Repair_Finish, CueParams);
		}
		StopWeld();
		StopScan();
		PlayReboot(Cast<ACharacter>(GetAvatarActorFromActorInfo()));
	}
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UknightGameplayAbility_RepairKit::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 취소(사망)로 끝나도 불꽃·섬광·스캔 링이 남지 않게.
	StopWeld();
	StopScan();

	if (bRunLockApplied)
	{
		bRunLockApplied = false;
		const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
		if (UknightMovementComponent* const Movement = Character ? Cast<UknightMovementComponent>(Character->GetCharacterMovement()) : nullptr)
		{
			Movement->SetRunLocked(false);
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---- 연출 ① 부위별 용접 ----

void UknightGameplayAbility_RepairKit::StartWeld(ACharacter* Character, float UseTime)
{
	StopWeld();
	UWorld* const World = Character ? Character->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer || UseTime <= 0.f)
	{
		return;
	}

	// 순서: 깎인 부위를 체력 비율이 낮은 순으로(같으면 머리 → 몸통 → 팔 → 다리). ApplyRepair가 채우는 순서와 같다.
	// 부위는 다 차 있고 전체 체력만 깎였으면 몸통만.
	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	TArray<TPair<float, EknightBodyPart>> Damaged;
	for (int32 Index = 0; Index < knightPartHealth::PartCount; ++Index)
	{
		const EknightBodyPart Part = static_cast<EknightBodyPart>(Index);
		const float Ratio = knightRepairFX::GetPartHealthRatio(ASC, Part);
		if (Ratio < 1.f - KINDA_SMALL_NUMBER)
		{
			Damaged.Emplace(Ratio, Part);
		}
	}
	Damaged.StableSort([](const TPair<float, EknightBodyPart>& A, const TPair<float, EknightBodyPart>& B) { return A.Key < B.Key; });
	for (const TPair<float, EknightBodyPart>& Entry : Damaged)
	{
		WeldOrder.Add(Entry.Value);
	}
	if (WeldOrder.IsEmpty())
	{
		WeldOrder.Add(EknightBodyPart::Torso);
	}

	// 사용 시간을 부위 수로 나눈다. 끝에 0.1초는 비워 재가동 연출과 겹치지 않게.
	WeldStartTime = World->GetTimeSeconds();
	WeldSlotTime = FMath::Max(UseTime - 0.1f, 0.1f) / WeldOrder.Num();

	if (WeldLightIntensity > 0.f)
	{
		if (USkeletalMeshComponent* const Mesh = Character->GetMesh())
		{
			WeldLight = NewObject<UPointLightComponent>(Character, NAME_None, RF_Transient);
			WeldLight->SetIntensityUnits(ELightUnits::Candelas);
			WeldLight->SetIntensity(WeldLightIntensity);
			WeldLight->SetLightColor(WeldLightColor);
			WeldLight->SetAttenuationRadius(WeldLightRadius);
			WeldLight->SetCastShadows(false);
			WeldLight->SetVisibility(false);
			WeldLight->SetupAttachment(Mesh);
			WeldLight->RegisterComponent();
		}
	}

	// 치지지직: 수리 내내 하나로 이어지는 소리. 몸에 붙여 걸어도 따라온다.
	if (WeldLoopSound)
	{
		WeldLoopAudio = UGameplayStatics::SpawnSoundAttached(WeldLoopSound, Character->GetRootComponent(), NAME_None,
			FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true, WeldLoopVolume);
	}

	// 첫 불꽃은 바로.
	PlayWeldSpark();
}

void UknightGameplayAbility_RepairKit::PlayWeldSpark()
{
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
	UWorld* const World = Character ? Character->GetWorld() : nullptr;
	if (!Mesh || !World || WeldOrder.IsEmpty())
	{
		return;
	}

	// 지금 용접 중인 부위 = 경과 시간 / 부위당 시간.
	const float Elapsed = static_cast<float>(World->GetTimeSeconds() - WeldStartTime);
	const int32 Slot = FMath::Clamp(FMath::FloorToInt(Elapsed / FMath::Max(WeldSlotTime, KINDA_SMALL_NUMBER)), 0, WeldOrder.Num() - 1);
	const EknightBodyPart Part = WeldOrder[Slot];

	TArray<FName, TInlineAllocator<8>> Bones;
	for (const FknightRepairWeldPoint& Point : WeldPoints)
	{
		if (Point.Part == Part && Mesh->DoesSocketExist(Point.Bone))
		{
			Bones.Add(Point.Bone);
		}
	}
	if (!Bones.IsEmpty())
	{
		const FName Bone = Bones[FMath::RandRange(0, Bones.Num() - 1)];
		// 본 중심에서 몸 바깥쪽으로 살짝 띄워 표면에서 튀는 것처럼. 방향은 몸 중심 → 본 + 무작위.
		const FVector BoneLocation = Mesh->GetSocketLocation(Bone);
		const FVector Center = Character->GetActorLocation();
		FVector Out = BoneLocation - Center;
		Out.Z *= 0.3f;
		Out = (Out.GetSafeNormal() + FMath::VRand() * 0.6f).GetSafeNormal();
		if (Out.IsNearlyZero())
		{
			Out = Character->GetActorForwardVector();
		}
		const FVector SparkLocation = BoneLocation + Out * 12.f;

		if (WeldSparkEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, WeldSparkEffect, SparkLocation, Out.Rotation(),
				FVector(WeldSparkScale), true, true, ENCPoolMethod::AutoRelease);
		}
		if (WeldSound)
		{
			UGameplayStatics::PlaySoundAtLocation(World, WeldSound, SparkLocation, WeldSoundVolume, FMath::FRandRange(0.9f, 1.15f));
		}
		if (WeldLight)
		{
			// 깜빡임: 매번 밝기를 흔들고 잠깐 켰다 끈다.
			WeldLight->SetWorldLocation(SparkLocation);
			WeldLight->SetIntensity(WeldLightIntensity * FMath::FRandRange(0.5f, 1.f));
			WeldLight->SetVisibility(true);
			TWeakObjectPtr<UPointLightComponent> WeakLight = WeldLight;
			World->GetTimerManager().SetTimer(WeldLightTimer, FTimerDelegate::CreateWeakLambda(this, [WeakLight]()
			{
				if (UPointLightComponent* const Light = WeakLight.Get())
				{
					Light->SetVisibility(false);
				}
			}), WeldLightDuration, false);
		}
	}
	ScheduleNextWeldSpark();
}

void UknightGameplayAbility_RepairKit::ScheduleNextWeldSpark()
{
	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Delay = FMath::Max(0.02f, WeldInterval * FMath::FRandRange(1.f - WeldIntervalJitter, 1.f + WeldIntervalJitter));
	World->GetTimerManager().SetTimer(WeldSparkTimer, this, &ThisClass::PlayWeldSpark, Delay, false);
}

void UknightGameplayAbility_RepairKit::StopWeld()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WeldSparkTimer);
		World->GetTimerManager().ClearTimer(WeldLightTimer);
	}
	if (WeldLight)
	{
		WeldLight->DestroyComponent();
		WeldLight = nullptr;
	}
	if (WeldLoopAudio)
	{
		// 뚝 끊기지 않게 아주 짧게 줄여 끝낸다(…직).
		if (WeldLoopFadeOut > 0.f)
		{
			WeldLoopAudio->FadeOut(WeldLoopFadeOut, 0.f);
		}
		else
		{
			WeldLoopAudio->Stop();
		}
		WeldLoopAudio = nullptr;
	}
	WeldOrder.Reset();
}

// ---- 연출 ⓪ 진행도 스캔 링 ----

void UknightGameplayAbility_RepairKit::StartScan(ACharacter* Character, float UseTime)
{
	StopScan();
	UWorld* const World = Character ? Character->GetWorld() : nullptr;
	const UCapsuleComponent* const Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!RepairScanEffect || !World || !Capsule || World->GetNetMode() == NM_DedicatedServer || UseTime <= 0.f)
	{
		return;
	}

	// 캡슐(루트) 기준 상대 위치. 발바닥 = -HalfHeight.
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	ScanBottomZ = -HalfHeight;
	ScanHeight = HalfHeight * 2.f * RepairScanHeightScale;
	ScanDuration = UseTime;
	ScanStartTime = World->GetTimeSeconds();

	const float RadiusScale = Capsule->GetScaledCapsuleRadius() * RepairScanRadiusScale / FMath::Max(RepairScanEffectRadius, 1.f);
	ScanComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(RepairScanEffect, Character->GetRootComponent(), NAME_None,
		FVector(0.f, 0.f, ScanBottomZ), FRotator::ZeroRotator, FVector(RadiusScale, RadiusScale, 1.f),
		EAttachLocation::KeepRelativeOffset, false, ENCPoolMethod::None, true, true);

	// 매 프레임 가까이 높이를 갱신한다(경과 시간 기준이라 프레임이 떨어져도 끝 시점은 같다).
	World->GetTimerManager().SetTimer(ScanTimer, this, &ThisClass::UpdateScan, 1.f / 60.f, true);
}

void UknightGameplayAbility_RepairKit::UpdateScan()
{
	const UWorld* const World = GetWorld();
	if (!World || !ScanComponent)
	{
		return;
	}
	const float Alpha = FMath::Clamp(static_cast<float>(World->GetTimeSeconds() - ScanStartTime) / FMath::Max(ScanDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
	FVector Location = ScanComponent->GetRelativeLocation();
	Location.Z = ScanBottomZ + ScanHeight * Alpha;
	ScanComponent->SetRelativeLocation(Location);
}

void UknightGameplayAbility_RepairKit::StopScan()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ScanTimer);
	}
	if (ScanComponent)
	{
		// 남은 입자는 수명(0.25초)대로 사라지고 컴포넌트는 스스로 지워진다.
		ScanComponent->SetAutoDestroy(true);
		ScanComponent->Deactivate();
		ScanComponent = nullptr;
	}
}

// ---- 연출 ② 재가동 ----

void UknightGameplayAbility_RepairKit::PlayReboot(ACharacter* Character)
{
	UWorld* const World = Character ? Character->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	USkeletalMeshComponent* const Mesh = Character->GetMesh();

	// 푸슉: 관절에서 냉각 증기. 본에 붙여 움직여도 따라간다. 방향은 몸 바깥쪽 + 위.
	if (RebootSteamEffect && Mesh)
	{
		for (const FName Bone : RebootSteamBones)
		{
			if (!Mesh->DoesSocketExist(Bone))
			{
				continue;
			}
			const FVector BoneLocation = Mesh->GetSocketLocation(Bone);
			FVector Out = BoneLocation - Character->GetActorLocation();
			Out.Z = 0.f;
			Out = (Out.GetSafeNormal() + FVector::UpVector * 0.7f).GetSafeNormal();
			// KeepWorldPosition: 위치·회전은 월드 기준으로 주고, 이후로는 본을 따라간다.
			UNiagaraFunctionLibrary::SpawnSystemAttached(RebootSteamEffect, Mesh, Bone, BoneLocation, Out.Rotation(),
				FVector(RebootSteamScale), EAttachLocation::KeepWorldPosition, true, ENCPoolMethod::AutoRelease, true, true);
		}
	}

	const FVector SoundLocation = Character->GetActorLocation();
	if (RebootSteamSound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, RebootSteamSound, SoundLocation, RebootSoundVolume);
	}
	// 철컥: 증기 뒤 잠깐. 어빌리티는 곧 끝나므로 캐릭터에 묶어 월드 타이머로 예약한다.
	if (RebootClankSound)
	{
		if (RebootClankDelay <= 0.f)
		{
			UGameplayStatics::PlaySoundAtLocation(World, RebootClankSound, SoundLocation, RebootSoundVolume);
		}
		else
		{
			TWeakObjectPtr<ACharacter> WeakCharacter = Character;
			USoundBase* const Clank = RebootClankSound;
			const float Volume = RebootSoundVolume;
			FTimerHandle ClankTimer;
			World->GetTimerManager().SetTimer(ClankTimer, FTimerDelegate::CreateWeakLambda(Character, [WeakCharacter, Clank, Volume]()
			{
				if (ACharacter* const Owner = WeakCharacter.Get())
				{
					UGameplayStatics::PlaySoundAtLocation(Owner, Clank, Owner->GetActorLocation(), Volume);
				}
			}), RebootClankDelay, false);
		}
	}

	// 파일럿 음성·흔들림은 조종하는 플레이어만.
	APlayerController* const PC = Cast<APlayerController>(Character->GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	if (RebootCameraShake && RebootCameraShakeScale > 0.f)
	{
		PC->ClientStartCameraShake(RebootCameraShake, RebootCameraShakeScale);
	}
	if (RebootVoice)
	{
		UGameInstance* const GameInstance = World->GetGameInstance();
		if (UknightAudioSubsystem* const Audio = GameInstance ? GameInstance->GetSubsystem<UknightAudioSubsystem>() : nullptr)
		{
			Audio->PlayVoice(RebootVoice);
		}
	}
}
