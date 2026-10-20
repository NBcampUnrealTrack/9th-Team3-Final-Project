#include "AbilitySystem/Abilities/knightGameplayAbility_BoostLunge.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_StanceSwitch.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraShakeBase.h"
#include "Combat/knightCombatLibrary.h"
#include "Combat/knightGroggyFeedbackComponent.h"
#include "Combat/knightTargetLockComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/RootMotionSource.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "System/knightGameplayTags.h"
#include "TimerManager.h"
#include "knight.h"

UknightGameplayAbility_BoostLunge::UknightGameplayAbility_BoostLunge(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EknightAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = EknightAbilityActivationGroup::Exclusive_Replaceable;
	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_Attack));
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_HitReact);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Airborne);
	// 돌진 + 베기라 일반 공격보다 조금 더 쓴다. (2026-10-12 임시값)
	StaminaCost = 20.f;
	BackThrusterOffsets = { FVector(-15.f, -12.f, 4.f), FVector(-15.f, 12.f, 4.f) };
	FootThrusterSockets = { TEXT("thruster_l"), TEXT("thruster_r") };
	// 공중 돌진이면 체공을 끝내고 날아간다(내려찍기와 같은 규칙). 땅에서는 체공이 없어 아무 일도 없다.
	CancelAbilitiesWithTag.AddTag(knightGameplayTags::Ability_Boost_Flight);
	// (2026-10-15) 부스터를 쓰는 동안 State.Boosting → 전투 자세(CombatStance 트리거)로 들어간다.
	// 없으면 몽타주의 Attach Weapon으로 칼만 손에 들리고 비전투 자세가 되어, 칼이 뜨고 넣지도 않았다.
	ActivationOwnedTags.AddTag(knightGameplayTags::State_Boosting);
}

UAnimMontage* UknightGameplayAbility_BoostLunge::PickStanceMontage(UAnimMontage* const TwoHanded, UAnimMontage* const OneHanded) const
{
	return OneHanded && !UknightGameplayAbility_StanceSwitch::IsTwoHanded(GetAbilitySystemComponentFromActorInfo()) ? OneHanded : TwoHanded;
}

void UknightGameplayAbility_BoostLunge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!StrikeMontage)
	{
		UE_LOG(LogknightAbility, Error, TEXT("%s has no StrikeMontage."), *GetName());
	}

	// 락온 대상 앞 도착 지점. 락온만 쓴다(소프트 타깃 없음).
	FVector Destination;
	FRotator FaceRotation;
	float Travel = 0.f;
	if (!Character || !Movement || !StrikeMontage
		|| !UknightCombatLibrary::ComputeAttackWarp(Character, MaxLungeDistance, StopDistance, false, 0.f, 0.f, Destination, FaceRotation, Travel)
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bStrikeStarted = false;
	Character->SetActorRotation(FRotator(0.f, FaceRotation.Yaw, 0.f));

	FVector ToDestination = Destination - Character->GetActorLocation();
	ToDestination.Z = 0.f;
	if (bAirLunge)
	{
		// 공중: 락온 지점 높이 쪽으로도 날아간다(오르내리는 양은 MaxAirHeightChange까지). 땅 아래로는 가지 않도록 움직임이 막는다.
		const UknightTargetLockComponent* const TargetLock = Character->FindComponentByClass<UknightTargetLockComponent>();
		FVector TargetPoint;
		if (TargetLock && TargetLock->GetLockedTargetLocation(TargetPoint))
		{
			ToDestination.Z = FMath::Clamp(TargetPoint.Z + AirTargetHeightOffset - Character->GetActorLocation().Z, -MaxAirHeightChange, MaxAirHeightChange);
		}
	}
	const float Distance = ToDestination.Size();
	LungeDirection = Distance > KINDA_SMALL_NUMBER ? ToDestination / Distance : Character->GetActorForwardVector().GetSafeNormal2D();

	UAbilityTask_WaitGameplayEvent* const HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, knightGameplayTags::Event_Melee_Hit, nullptr, false, true);
	HitTask->EventReceived.AddDynamic(this, &ThisClass::OnMeleeHitEvent);
	HitTask->ReadyForActivation();

	// 이미 가까우면(락온 태그 갱신 직후 등) 돌진 없이 바로 벤다.
	if (Distance < 50.f)
	{
		OnLungeFinished();
		return;
	}

	ActiveDashMontage = PickStanceMontage(DashMontage, OneHandedDashMontage);
	if (UAnimMontage* const Dash = ActiveDashMontage)
	{
		if (UAnimInstance* const Anim = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
		{
			Anim->Montage_Play(Dash, DashPlayRate * GetAttackSpeed());
			// 돌진 자세는 베기가 시작될 때까지 반복한다. 공중 돌진 자세(Plunge_Hold)는 0.33초짜리라
			// 돌진(최대 0.6초)보다 먼저 끝나서 중간에 기본 공중 자세로 끊겼다. (2026-10-13)
			const FName DashSection = Dash->GetSectionName(0);
			if (DashSection != NAME_None)
			{
				Anim->Montage_SetNextSection(DashSection, DashSection, Dash);
			}
		}
	}

	const float Duration = FMath::Clamp(Distance / LungeSpeed, MinLungeTime, MaxLungeTime);
	UAbilityTask_ApplyRootMotionConstantForce* const LungeTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("BoostLunge"), LungeDirection, Distance / Duration, Duration, false, nullptr,
		ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, Movement->GetMaxSpeed(), false);
	LungeTask->OnFinish.AddDynamic(this, &ThisClass::OnLungeFinished);
	LungeTask->ReadyForActivation();

	StartBackThrusters(*Character);
	if (bAirLunge)
	{
		StartFootThrusters(*Character);
	}
	if (LungeSound)
	{
		UGameplayStatics::PlaySoundAtLocation(Character, LungeSound, Character->GetActorLocation());
	}
	if (LungeShake)
	{
		if (APlayerController* const PC = Cast<APlayerController>(Character->GetController()))
		{
			PC->ClientStartCameraShake(LungeShake, LungeShakeScale);
		}
	}
}

void UknightGameplayAbility_BoostLunge::OnLungeFinished()
{
	if (!IsActive() || bStrikeStarted)
	{
		return;
	}
	bStrikeStarted = true;

	if (UWorld* const World = GetWorld(); World && BackThrusterLinger > 0.f && !BackThrusters.IsEmpty())
	{
		World->GetTimerManager().SetTimer(BackThrusterStopTimer, this, &ThisClass::StopBackThrusters, BackThrusterLinger, false);
	}
	else
	{
		StopBackThrusters();
	}

	// 도착 순간 대상 쪽으로 다시 맞춘다(대상이 움직였을 수 있다). 몽타주의 워프 구간이 방향만 돌린다.
	StopFootThrusters();

	// 공중: 베는 동안 잠깐 그 자리에 멈춘다(중력 0). AirHangTime이 지나면 다시 떨어진다.
	if (bAirLunge)
	{
		const ACharacter* const HangCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
		UCharacterMovementComponent* const Movement = HangCharacter ? HangCharacter->GetCharacterMovement() : nullptr;
		UWorld* const World = GetWorld();
		if (Movement && World && Movement->IsFalling() && AirHangTime > 0.f)
		{
			SavedGravityScale = Movement->GravityScale;
			bGravitySaved = true;
			Movement->GravityScale = 0.f;
			Movement->Velocity = FVector::ZeroVector;
			World->GetTimerManager().SetTimer(AirHangTimer, this, &ThisClass::RestoreGravity, AirHangTime, false);
		}
	}

	UknightCombatLibrary::SetAttackWarpTarget(GetAvatarActorFromActorInfo(), WarpTargetName, StopDistance, StopDistance, false, 0.f, 0.f);

	UAnimMontage* const Strike = PickStanceMontage(StrikeMontage, OneHandedStrikeMontage);
	UAbilityTask_PlayMontageAndWait* const StrikeTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, TEXT("LungeStrike"), Strike, StrikePlayRate * GetAttackSpeed());
	StrikeTask->OnCompleted.AddDynamic(this, &ThisClass::OnStrikeFinished);
	StrikeTask->OnBlendOut.AddDynamic(this, &ThisClass::OnStrikeFinished);
	StrikeTask->OnInterrupted.AddDynamic(this, &ThisClass::OnStrikeFinished);
	StrikeTask->OnCancelled.AddDynamic(this, &ThisClass::OnStrikeFinished);
	StrikeTask->ReadyForActivation();
	if (AActor* const Avatar = GetAvatarActorFromActorInfo())
	{
		if (UknightGroggyFeedbackComponent* const Feedback = Avatar->FindComponentByClass<UknightGroggyFeedbackComponent>())
		{
			Feedback->WatchAttack(this, Strike, GetStanceScaledHitParams(HitParams));
		}
	}
}

void UknightGameplayAbility_BoostLunge::OnStrikeFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UknightGameplayAbility_BoostLunge::OnMeleeHitEvent(FGameplayEventData Payload)
{
	const FHitResult* const Hit = Payload.TargetData.Num() > 0 && Payload.TargetData.Get(0) ? Payload.TargetData.Get(0)->GetHitResult() : nullptr;
	if (Hit)
	{
		UknightCombatLibrary::ApplyMeleeHit(GetAvatarActorFromActorInfo(), *Hit, GetStanceScaledHitParams(HitParams), this);
	}
}

void UknightGameplayAbility_BoostLunge::StartBackThrusters(const ACharacter& Character)
{
	StopBackThrusters();
	UWorld* const World = GetWorld();
	if (!BackThrusterEffect || !World)
	{
		return;
	}
	for (int32 Index = 0; Index < BackThrusterOffsets.Num(); ++Index)
	{
		// 대시 등 부스터와 같은 방식: 월드에 만든 뒤 위치·회전을 매 프레임 직접 정한다.
		UNiagaraComponent* const Jet = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, BackThrusterEffect, Character.GetActorLocation(),
			Character.GetActorRotation(), FVector(BackThrusterScale), true, true, ENCPoolMethod::None);
		if (Jet)
		{
			BackThrusters.Add(Jet);
		}
	}
	UpdateBackThrusters();
	World->GetTimerManager().SetTimer(BackThrusterTimer, this, &ThisClass::UpdateBackThrusters, 1.f / 60.f, true);
}

void UknightGameplayAbility_BoostLunge::UpdateBackThrusters()
{
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const USkeletalMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}
	// 발 추진기: 돌진 반대 방향(위아래 포함)으로 뿜는다. 소켓에 붙어 있고 회전만 월드 기준.
	if (!FootThrusters.IsEmpty())
	{
		const FVector FootEmit = LungeDirection.IsNearlyZero() ? -Character->GetActorForwardVector() : -LungeDirection;
		const FRotator FootRotation = FRotationMatrix::MakeFromZ(-FootEmit.GetSafeNormal()).Rotator();
		for (UNiagaraComponent* const Jet : FootThrusters)
		{
			if (Jet)
			{
				Jet->SetWorldRotation(FootRotation);
			}
		}
	}
	if (BackThrusters.IsEmpty())
	{
		return;
	}
	const FVector Anchor = Mesh->DoesSocketExist(BackThrusterBone) ? Mesh->GetSocketLocation(BackThrusterBone) : Character->GetActorLocation();
	const FQuat ActorQuat = Character->GetActorQuat();
	// 돌진 반대 방향으로, 조금 아래로 뿜는다. 이펙트는 로컬 -Z로 뿜으므로 로컬 Z를 뿜는 방향의 반대로 둔다.
	const FVector Back = LungeDirection.IsNearlyZero() ? -Character->GetActorForwardVector() : -LungeDirection;
	const float Down = FMath::DegreesToRadians(BackThrusterDownAngle);
	const FVector Emit = (Back.GetSafeNormal2D() * FMath::Cos(Down) - FVector::UpVector * FMath::Sin(Down)).GetSafeNormal();
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

void UknightGameplayAbility_BoostLunge::StartFootThrusters(const ACharacter& Character)
{
	StopFootThrusters();
	USkeletalMeshComponent* const Mesh = Character.GetMesh();
	if (!FootThrusterEffect || !Mesh)
	{
		return;
	}
	for (const FName Socket : FootThrusterSockets)
	{
		if (!Mesh->DoesSocketExist(Socket))
		{
			continue;
		}
		UNiagaraComponent* const Jet = UNiagaraFunctionLibrary::SpawnSystemAttached(FootThrusterEffect, Mesh, Socket,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
		if (Jet)
		{
			Jet->SetUsingAbsoluteRotation(true);
			Jet->SetWorldScale3D(FVector(FootThrusterScale));
			FootThrusters.Add(Jet);
		}
	}
	// 등 부스터가 없어도 회전을 갱신하도록 타이머를 돌린다.
	UpdateBackThrusters();
	if (UWorld* const World = GetWorld(); World && !World->GetTimerManager().IsTimerActive(BackThrusterTimer))
	{
		World->GetTimerManager().SetTimer(BackThrusterTimer, this, &ThisClass::UpdateBackThrusters, 1.f / 60.f, true);
	}
}

void UknightGameplayAbility_BoostLunge::StopFootThrusters()
{
	for (UNiagaraComponent* const Jet : FootThrusters)
	{
		if (Jet)
		{
			Jet->Deactivate();
		}
	}
	FootThrusters.Reset();
}

void UknightGameplayAbility_BoostLunge::RestoreGravity()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AirHangTimer);
	}
	if (!bGravitySaved)
	{
		return;
	}
	bGravitySaved = false;
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr)
	{
		Movement->GravityScale = SavedGravityScale;
	}
}

void UknightGameplayAbility_BoostLunge::StopBackThrusters()
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

void UknightGameplayAbility_BoostLunge::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AActor* const Avatar = GetAvatarActorFromActorInfo())
	{
		if (UknightGroggyFeedbackComponent* const Feedback = Avatar->FindComponentByClass<UknightGroggyFeedbackComponent>())
		{
			Feedback->EndPredictionForAttack(this);
		}
	}
	StopBackThrusters();
	StopFootThrusters();
	RestoreGravity();
	if (IsActive() && ActiveDashMontage && !bStrikeStarted)
	{
		// 돌진 도중 끊기면(피격 등) 돌진 자세도 내린다.
		const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
		if (UAnimInstance* const Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
		{
			Anim->Montage_Stop(0.2f, ActiveDashMontage);
		}
	}
	ActiveDashMontage = nullptr;
	bStrikeStarted = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
