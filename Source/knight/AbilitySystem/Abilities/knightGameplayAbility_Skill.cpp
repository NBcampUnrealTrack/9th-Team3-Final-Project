#include "AbilitySystem/Abilities/knightGameplayAbility_Skill.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Combat/knightCombatLibrary.h"
#include "Combat/knightTargetLockComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "System/knightGameplayTags.h"
#include "TimerManager.h"

UknightGameplayAbility_Skill::UknightGameplayAbility_Skill(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EknightAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = EknightAbilityActivationGroup::Exclusive_Replaceable;
	// 전투 자세·공격 규칙(캔슬 창 등)이 공격과 같게 Ability.Attack을 단다. 스킬별 태그는 BP의 AssetTags에 더한다.
	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_Attack));
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_HitReact);
	// 비용·쿨타임 (2026-10-12 사용자 결정: 부스트 게이지 + 개별 쿨타임). 값은 스킬 BP마다.
	StaminaCost = 25.f;
	bRequireFullStaminaCost = true;
	BackThrusterOffsets = { FVector(-15.f, -12.f, 4.f), FVector(-15.f, 12.f, 4.f) };
}

float UknightGameplayAbility_Skill::GetCooldownRemaining() const
{
	const UWorld* const World = GetWorld();
	return World ? FMath::Max(0.f, CooldownEndTime - static_cast<float>(World->GetTimeSeconds())) : 0.f;
}

bool UknightGameplayAbility_Skill::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (GetCooldownRemaining() > 0.f)
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(knightGameplayTags::Ability_ActivateFail_Cooldown);
		}
		return false;
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UknightGameplayAbility_Skill::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (const UWorld* const World = GetWorld())
	{
		CooldownEndTime = static_cast<float>(World->GetTimeSeconds()) + Cooldown;
	}
	HitActors.Reset();

	if (!FaceLockedTarget())
	{
		FaceMoveInputDirection();
	}
	if (CastSound)
	{
		UGameplayStatics::PlaySoundAtLocation(Character, CastSound, Character->GetActorLocation());
	}
	if (CastShake)
	{
		if (APlayerController* const PC = Cast<APlayerController>(Character->GetController()))
		{
			PC->ClientStartCameraShake(CastShake, CastShakeScale);
		}
	}
	StartSkill();
}

void UknightGameplayAbility_Skill::PlaySkillMontage(UAnimMontage* Montage, float Rate)
{
	if (!Montage)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
		return;
	}
	UAbilityTask_PlayMontageAndWait* const Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, TEXT("Skill"), Montage, FMath::Max(Rate, 0.01f) * GetActionSpeed());
	Task->OnCompleted.AddDynamic(this, &ThisClass::OnSkillMontageFinished);
	Task->OnBlendOut.AddDynamic(this, &ThisClass::OnSkillMontageFinished);
	Task->OnInterrupted.AddDynamic(this, &ThisClass::OnSkillMontageFinished);
	Task->OnCancelled.AddDynamic(this, &ThisClass::OnSkillMontageFinished);
	Task->ReadyForActivation();
}

void UknightGameplayAbility_Skill::OnSkillMontageFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

bool UknightGameplayAbility_Skill::FaceLockedTarget() const
{
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UknightTargetLockComponent* const TargetLock = Character ? Character->FindComponentByClass<UknightTargetLockComponent>() : nullptr;
	FVector TargetLocation;
	if (!TargetLock || !TargetLock->GetLockedTargetLocation(TargetLocation))
	{
		return false;
	}
	const FVector ToTarget = (TargetLocation - Character->GetActorLocation()).GetSafeNormal2D();
	if (ToTarget.IsNearlyZero())
	{
		return false;
	}
	Character->SetActorRotation(FRotator(0.f, ToTarget.Rotation().Yaw, 0.f));
	return true;
}

void UknightGameplayAbility_Skill::ListenForMeleeHits(bool bOncePerActor)
{
	bHitOnceThisCast = bOncePerActor;
	UAbilityTask_WaitGameplayEvent* const HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, knightGameplayTags::Event_Melee_Hit, nullptr, false, true);
	HitTask->EventReceived.AddDynamic(this, &ThisClass::OnMeleeHitEvent);
	HitTask->ReadyForActivation();
}

void UknightGameplayAbility_Skill::OnMeleeHitEvent(FGameplayEventData Payload)
{
	const FHitResult* const Hit = Payload.TargetData.Num() > 0 && Payload.TargetData.Get(0) ? Payload.TargetData.Get(0)->GetHitResult() : nullptr;
	if (!Hit)
	{
		return;
	}
	if (bHitOnceThisCast)
	{
		const TWeakObjectPtr<AActor> Target = Hit->GetActor();
		if (HitActors.Contains(Target))
		{
			return;
		}
		HitActors.Add(Target);
	}
	const EknightHitOutcome Outcome = UknightCombatLibrary::ApplyMeleeHit(GetAvatarActorFromActorInfo(), *Hit, HitParams);
	OnSkillHit(*Hit, Outcome);
}

void UknightGameplayAbility_Skill::StartBackThrusters(const FVector& Direction)
{
	ClearBackThrusters();
	const AActor* const Avatar = GetAvatarActorFromActorInfo();
	UWorld* const World = GetWorld();
	if (!BackThrusterEffect || !Avatar || !World)
	{
		return;
	}
	ThrusterDirection = Direction.GetSafeNormal2D();
	for (int32 Index = 0; Index < BackThrusterOffsets.Num(); ++Index)
	{
		// 대시 등 부스터와 같은 방식: 월드에 만든 뒤 위치·회전을 매 프레임 직접 정한다.
		UNiagaraComponent* const Jet = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, BackThrusterEffect, Avatar->GetActorLocation(),
			Avatar->GetActorRotation(), FVector(BackThrusterScale), true, true, ENCPoolMethod::None);
		if (Jet)
		{
			BackThrusters.Add(Jet);
		}
	}
	UpdateBackThrusters();
	World->GetTimerManager().SetTimer(BackThrusterTimer, this, &ThisClass::UpdateBackThrusters, 1.f / 60.f, true);
}

void UknightGameplayAbility_Skill::UpdateBackThrusters()
{
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const USkeletalMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh || BackThrusters.IsEmpty())
	{
		return;
	}
	const FVector Anchor = Mesh->DoesSocketExist(BackThrusterBone) ? Mesh->GetSocketLocation(BackThrusterBone) : Character->GetActorLocation();
	const FQuat ActorQuat = Character->GetActorQuat();
	const FVector Back = ThrusterDirection.IsNearlyZero() ? -Character->GetActorForwardVector().GetSafeNormal2D() : -ThrusterDirection;
	const float Down = FMath::DegreesToRadians(BackThrusterDownAngle);
	const FVector Emit = (Back * FMath::Cos(Down) - FVector::UpVector * FMath::Sin(Down)).GetSafeNormal();
	// 이펙트는 로컬 -Z로 뿜으므로 로컬 Z를 뿜는 방향의 반대로 둔다.
	const FRotator Rotation = FRotationMatrix::MakeFromZ(-Emit).Rotator();
	for (int32 Index = 0; Index < BackThrusters.Num(); ++Index)
	{
		if (UNiagaraComponent* const Jet = BackThrusters[Index])
		{
			const FVector Offset = BackThrusterOffsets.IsValidIndex(Index) ? BackThrusterOffsets[Index] : FVector::ZeroVector;
			Jet->SetWorldLocationAndRotation(Anchor + ActorQuat.RotateVector(Offset), Rotation);
		}
	}
}

void UknightGameplayAbility_Skill::StopBackThrusters(float Linger)
{
	UWorld* const World = GetWorld();
	if (Linger > 0.f && World && !BackThrusters.IsEmpty())
	{
		World->GetTimerManager().SetTimer(BackThrusterStopTimer, this, &ThisClass::ClearBackThrusters, Linger, false);
		return;
	}
	ClearBackThrusters();
}

void UknightGameplayAbility_Skill::ClearBackThrusters()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BackThrusterTimer);
		World->GetTimerManager().ClearTimer(BackThrusterStopTimer);
	}
	for (UNiagaraComponent* const Jet : BackThrusters)
	{
		if (Jet)
		{
			Jet->Deactivate();
		}
	}
	BackThrusters.Reset();
}

void UknightGameplayAbility_Skill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	ClearBackThrusters();
	HitActors.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
