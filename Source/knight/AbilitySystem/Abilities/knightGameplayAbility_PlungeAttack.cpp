#include "AbilitySystem/Abilities/knightGameplayAbility_PlungeAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraShakeBase.h"
#include "Combat/knightCombatLibrary.h"
#include "Combat/knightHitStopComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "System/knightGameplayTags.h"
#include "TimerManager.h"
#include "knight.h"

namespace
{
	constexpr float PlungeTickInterval = 1.f / 60.f;
}

UknightGameplayAbility_PlungeAttack::UknightGameplayAbility_PlungeAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EknightAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = EknightAbilityActivationGroup::Exclusive_Replaceable;
	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_Attack));
	ActivationRequiredTags.AddTag(knightGameplayTags::State_Airborne);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_HitReact);
	// 체공 중이면 체공을 끝내고 내려찍는다.
	CancelAbilitiesWithTag.AddTag(knightGameplayTags::Ability_Boost_Flight);
	StaminaCost = 20.f;
	ThrusterSockets = { TEXT("thruster_l"), TEXT("thruster_r") };
}

void UknightGameplayAbility_PlungeAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
	UWorld* const World = GetWorld();
	if (!StartMontage || !LoopMontage || !EndMontage)
	{
		UE_LOG(LogknightAbility, Error, TEXT("%s: StartMontage / LoopMontage / EndMontage must all be set."), *GetName());
	}
	if (!Movement || !World || !StartMontage || !LoopMontage || !EndMontage || !Movement->IsFalling() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FaceMoveInputDirection();

	// 낙하 모드는 그대로 두고 중력·공중 조작을 끈다(애님은 공중 자세 유지). 속도는 Tick이 정한다.
	SavedGravityScale = Movement->GravityScale;
	SavedAirControl = Movement->AirControl;
	bMovementSaved = true;
	Movement->GravityScale = 0.f;
	Movement->AirControl = 0.f;
	Movement->Velocity = FVector(Movement->Velocity.X * 0.3f, Movement->Velocity.Y * 0.3f, BoostUpSpeed);

	UAbilityTask_WaitGameplayEvent* const HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, knightGameplayTags::Event_Melee_Hit, nullptr, false, true);
	HitTask->EventReceived.AddDynamic(this, &ThisClass::OnMeleeHitEvent);
	HitTask->ReadyForActivation();

	// 반동: 발 추진기가 아래로 짧게 터지며 살짝 떠오른다.
	Phase = EPhase::Boost;
	PhaseStartTime = World->GetTimeSeconds();
	PlayMontage(StartMontage, false);
	bThrustersReversed = false;
	SpawnThrusters(BoostThrusterScale);
	if (BoostSound)
	{
		UGameplayStatics::PlaySoundAtLocation(Character, BoostSound, Character->GetActorLocation());
	}
	if (BoostShake)
	{
		if (APlayerController* const PC = Cast<APlayerController>(Character->GetController()))
		{
			PC->ClientStartCameraShake(BoostShake, BoostShakeScale);
		}
	}
	World->GetTimerManager().SetTimer(PhaseTimer, this, &ThisClass::StartDive, BoostUpTime, false);
	World->GetTimerManager().SetTimer(TickTimer, this, &ThisClass::TickPlunge, PlungeTickInterval, true);
}

void UknightGameplayAbility_PlungeAttack::PlayMontage(UAnimMontage* Montage, bool bEndOnFinish)
{
	const float Rate = PlayRate * GetAttackSpeed();
	if (bEndOnFinish)
	{
		// 마지막 동작만 태스크로 재생해서, 끝나면(또는 끊기면) 어빌리티를 끝낸다.
		UAbilityTask_PlayMontageAndWait* const Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, TEXT("PlungeEnd"), Montage, Rate);
		Task->OnCompleted.AddDynamic(this, &ThisClass::OnEndMontageFinished);
		Task->OnBlendOut.AddDynamic(this, &ThisClass::OnEndMontageFinished);
		Task->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageInterrupted);
		Task->OnCancelled.AddDynamic(this, &ThisClass::OnMontageInterrupted);
		Task->ReadyForActivation();
		return;
	}
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (UAnimInstance* const Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
	{
		Anim->Montage_Play(Montage, Rate);
	}
}

void UknightGameplayAbility_PlungeAttack::SpawnThrusters(float Scale)
{
	StopThrusters();
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh || !ThrusterEffect)
	{
		return;
	}
	for (const FName Socket : ThrusterSockets)
	{
		if (!Mesh->DoesSocketExist(Socket))
		{
			continue;
		}
		UNiagaraComponent* const Jet = UNiagaraFunctionLibrary::SpawnSystemAttached(ThrusterEffect, Mesh, Socket,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
		if (Jet)
		{
			Jet->SetUsingAbsoluteRotation(true);
			Jet->SetWorldScale3D(FVector(Scale));
			ThrusterComponents.Add(Jet);
		}
	}
	UpdateThrusterRotation();
}

void UknightGameplayAbility_PlungeAttack::StopThrusters()
{
	for (UNiagaraComponent* const Jet : ThrusterComponents)
	{
		if (Jet)
		{
			Jet->Deactivate();
		}
	}
	ThrusterComponents.Reset();
}

void UknightGameplayAbility_PlungeAttack::UpdateThrusterRotation() const
{
	const AActor* const Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}
	// 기본은 몸 기준 아래로 뿜는다. 역분사 때는 뒤집어(롤 180) 위로 뿜는다. 몸 기울기를 따라간다.
	FQuat Rotation = Avatar->GetActorQuat() * FRotator(CurrentLeanPitch, 0.f, 0.f).Quaternion();
	if (bThrustersReversed)
	{
		Rotation = Rotation * FRotator(0.f, 0.f, 180.f).Quaternion();
	}
	for (UNiagaraComponent* const Jet : ThrusterComponents)
	{
		if (Jet)
		{
			Jet->SetWorldRotation(Rotation);
		}
	}
}

void UknightGameplayAbility_PlungeAttack::StartDive()
{
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
	UWorld* const World = GetWorld();
	if (!IsActive() || !Movement || !World)
	{
		return;
	}
	if (!Movement->IsFalling())
	{
		StartLand();
		return;
	}

	// 수평 이동: 노릴 대상이 있으면 그 근처에 떨어지도록, 땅까지 걸릴 시간으로 나눠 속도를 정한다.
	DiveHorizontalVelocity = Character->GetActorForwardVector().GetSafeNormal2D() * DiveForwardSpeed;
	FVector AimLocation;
	FRotator AimRotation;
	float Travel = 0.f;
	if (MaxAimDistance > 0.f && UknightCombatLibrary::ComputeAttackWarp(Character, MaxAimDistance, AimStopDistance, true, SoftTargetRange,
		SoftTargetAngle, AimLocation, AimRotation, Travel))
	{
		const FVector Start = Character->GetActorLocation();
		FHitResult Ground;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(knightPlungeGround), false, Character);
		const float Height = World->LineTraceSingleByChannel(Ground, Start, Start - FVector(0.f, 0.f, 10000.f), ECC_Visibility, Params)
			? FMath::Max(Start.Z - Ground.ImpactPoint.Z, 50.f) : 500.f;
		// 가속 구간까지 대략 넣어 땅에 닿는 시간을 잡는다.
		const float FallTime = FMath::Max(Height / DiveSpeed + DiveAccelTime * 0.5f, 0.08f);
		const FVector Flat = (AimLocation - Start).GetSafeNormal2D();
		DiveHorizontalVelocity = (Flat * (Travel / FallTime)).GetClampedToMaxSize(MaxDiveHorizontalSpeed);
		Character->SetActorRotation(FRotator(0.f, AimRotation.Yaw, 0.f));
	}

	// 역분사: 추진기를 위로 뒤집어 뿜으며 아래로 가속한다.
	Phase = EPhase::Dive;
	PhaseStartTime = World->GetTimeSeconds();
	PlayMontage(LoopMontage, false);
	bThrustersReversed = true;
	SpawnThrusters(DiveThrusterScale);
	if (DiveSound)
	{
		UGameplayStatics::PlaySoundAtLocation(Character, DiveSound, Character->GetActorLocation());
	}
}

void UknightGameplayAbility_PlungeAttack::TickPlunge()
{
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
	UWorld* const World = GetWorld();
	if (!IsActive() || !Movement || !World)
	{
		return;
	}
	const float DeltaTime = PlungeTickInterval;
	const float Elapsed = static_cast<float>(World->GetTimeSeconds() - PhaseStartTime);
	switch (Phase)
	{
	case EPhase::Boost:
	{
		// 떠오르는 속도를 반동 시간 동안 줄여서, 끝에서 잠깐 멈칫한 뒤 꽂히게 한다.
		const float Alpha = FMath::Clamp(Elapsed / BoostUpTime, 0.f, 1.f);
		Movement->Velocity.Z = BoostUpSpeed * (1.f - Alpha);
		Movement->Velocity.X *= 0.9f;
		Movement->Velocity.Y *= 0.9f;
		UpdateLean(DeltaTime, BoostLeanPitch);
		if (!Movement->IsFalling())
		{
			StartLand();
		}
		break;
	}
	case EPhase::Dive:
	{
		if (!Movement->IsFalling())
		{
			StartLand();
			break;
		}
		if (Elapsed > MaxDiveTime)
		{
			UE_LOG(LogknightAbility, Log, TEXT("PlungeAttack: no ground within %.1fs, ending."), MaxDiveTime);
			EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
			break;
		}
		// 가속: 처음 DiveAccelTime 동안 0에서 최고 속도로 (제곱 곡선이라 처음엔 묵직하게 떨어지다 꽂힌다).
		const float Accel = FMath::Clamp(Elapsed / DiveAccelTime, 0.f, 1.f);
		const float Speed = DiveSpeed * Accel * Accel;
		Movement->Velocity = DiveHorizontalVelocity * Accel + FVector(0.f, 0.f, -FMath::Max(Speed, 200.f));
		UpdateLean(DeltaTime, DiveLeanPitch);
		// 낙하 동작은 짧은 루프라, 끝나면 다시 튼다.
		if (UAnimInstance* const Anim = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
		{
			if (!Anim->Montage_IsPlaying(LoopMontage))
			{
				Anim->Montage_Play(LoopMontage, PlayRate * GetAttackSpeed());
			}
		}
		break;
	}
	default:
		break;
	}
	UpdateThrusterRotation();
}

void UknightGameplayAbility_PlungeAttack::UpdateLean(float DeltaTime, float TargetPitch)
{
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}
	if (!bLeanActive)
	{
		MeshBaseLocation = Mesh->GetRelativeLocation();
		MeshBaseRotation = Mesh->GetRelativeRotation().Quaternion();
		CurrentLeanPitch = 0.f;
		bLeanActive = true;
	}
	CurrentLeanPitch = FMath::FInterpTo(CurrentLeanPitch, TargetPitch, DeltaTime, LeanSpeed);
	// 캡슐 가운데(액터 원점)를 축으로 돌린다 (체공 기울기와 같은 방식).
	const FQuat Lean = FRotator(CurrentLeanPitch, 0.f, 0.f).Quaternion();
	Mesh->SetRelativeLocationAndRotation(Lean.RotateVector(MeshBaseLocation), Lean * MeshBaseRotation);
}

void UknightGameplayAbility_PlungeAttack::ResetLean()
{
	if (!bLeanActive)
	{
		return;
	}
	bLeanActive = false;
	CurrentLeanPitch = 0.f;
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (USkeletalMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr)
	{
		Mesh->SetRelativeLocationAndRotation(MeshBaseLocation, MeshBaseRotation);
	}
}

void UknightGameplayAbility_PlungeAttack::StartLand()
{
	if (Phase == EPhase::Land)
	{
		return;
	}
	Phase = EPhase::Land;
	UWorld* const World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(PhaseTimer);
		World->GetTimerManager().ClearTimer(TickTimer);
	}
	StopThrusters();
	ResetLean();
	RestoreMovement();
	// 착지 흔들림·착지 경직은 무브먼트 컴포넌트가 떨어진 속도로 이미 처리했다. 여기서는 내려찍는 동작과 타격.
	PlayMontage(EndMontage, true);
	if (World)
	{
		const float Rate = FMath::Max(PlayRate * GetAttackSpeed(), 0.01f);
		if (ImpactDelay > 0.f)
		{
			World->GetTimerManager().SetTimer(ImpactTimer, this, &ThisClass::OnImpact, ImpactDelay / Rate, false);
		}
		else
		{
			OnImpact();
		}
		if (EndRecoveryTime > 0.f)
		{
			World->GetTimerManager().SetTimer(PhaseTimer, this, &ThisClass::FinishRecovery, EndRecoveryTime / Rate, false);
		}
	}
}

void UknightGameplayAbility_PlungeAttack::OnImpact()
{
	AActor* const Avatar = GetAvatarActorFromActorInfo();
	UWorld* const World = GetWorld();
	if (!IsActive() || !Avatar || !World)
	{
		return;
	}
	// 맞은 대상이 없어도 땅을 찍는 순간 잠깐 멈칫한다. 맞으면 타격 히트스톱이 더 길게 덮어쓴다(연장만 됨).
	if (ImpactSelfHitStop > 0.f)
	{
		if (UknightHitStopComponent* const HitStop = Avatar->FindComponentByClass<UknightHitStopComponent>())
		{
			HitStop->ApplyHitStop(ImpactSelfHitStop);
		}
	}
	if (ImpactEffect)
	{
		const ACharacter* const Character = Cast<ACharacter>(Avatar);
		const float HalfHeight = Character && Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
		const FVector Feet = Avatar->GetActorLocation() - FVector(0.f, 0.f, HalfHeight) + Avatar->GetActorForwardVector() * 100.f;
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, ImpactEffect, Feet, Avatar->GetActorRotation(), FVector(ImpactEffectScale),
			true, true, ENCPoolMethod::AutoRelease);
	}
}

void UknightGameplayAbility_PlungeAttack::FinishRecovery()
{
	if (!IsActive())
	{
		return;
	}
	MontageStop(EndRecoveryBlendOut);
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UknightGameplayAbility_PlungeAttack::RestoreMovement()
{
	if (!bMovementSaved)
	{
		return;
	}
	bMovementSaved = false;
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr)
	{
		Movement->GravityScale = SavedGravityScale;
		Movement->AirControl = SavedAirControl;
	}
}

void UknightGameplayAbility_PlungeAttack::OnEndMontageFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UknightGameplayAbility_PlungeAttack::OnMontageInterrupted()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

void UknightGameplayAbility_PlungeAttack::OnMeleeHitEvent(FGameplayEventData Payload)
{
	const FHitResult* const Hit = Payload.TargetData.Num() > 0 && Payload.TargetData.Get(0) ? Payload.TargetData.Get(0)->GetHitResult() : nullptr;
	if (Hit)
	{
		UknightCombatLibrary::ApplyMeleeHit(GetAvatarActorFromActorInfo(), *Hit, HitParams);
	}
}

void UknightGameplayAbility_PlungeAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhaseTimer);
		World->GetTimerManager().ClearTimer(TickTimer);
		World->GetTimerManager().ClearTimer(ImpactTimer);
	}
	// 공중에서 끊긴 경우(피격 등)에도 중력·공중 조작·기울기·추진기를 원래대로 돌린다.
	StopThrusters();
	ResetLean();
	RestoreMovement();
	if (Phase != EPhase::Land && IsActive())
	{
		const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
		if (UAnimInstance* const Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
		{
			Anim->Montage_Stop(0.2f, StartMontage);
			Anim->Montage_Stop(0.2f, LoopMontage);
		}
	}
	Phase = EPhase::None;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
