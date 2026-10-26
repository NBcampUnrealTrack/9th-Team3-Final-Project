#include "AbilitySystem/Abilities/knightGameplayAbility_PlungeAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_StanceSwitch.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraShakeBase.h"
#include "Combat/knightCombatLibrary.h"
#include "Combat/knightGroggyFeedbackComponent.h"
#include "Combat/knightHitStopComponent.h"
#include "Components/AudioComponent.h"
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

	// 락온 상태가 아닐 때 공격하면 화면 중앙(카메라/컨트롤러 수평 방향)을 바라보게 한다.
	void FaceScreenCenterForPlungeAttack(ACharacter* Character, const UAbilitySystemComponent* ASC)
	{
		if (!Character || (ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_LockedOn)))
		{
			return;
		}
		if (const AController* const Controller = Character->GetController())
		{
			Character->SetActorRotation(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f));
		}
	}
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
	// (2026-10-15) 추진기를 쓰는 동안 State.Boosting → 전투 자세(CombatStance 트리거)로 들어간다.
	ActivationOwnedTags.AddTag(knightGameplayTags::State_Boosting);
}

void UknightGameplayAbility_PlungeAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	HitActorsThisAttack.Reset();

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

	FaceScreenCenterForPlungeAttack(Character, GetAbilitySystemComponentFromActorInfo());

	// (2026-10-15) 한손 스탠스면 한손 몽타주 (비어 있는 것은 양손용).
	const bool bOneHanded = !UknightGameplayAbility_StanceSwitch::IsTwoHanded(GetAbilitySystemComponentFromActorInfo());
	ActiveStartMontage = bOneHanded && OneHandedStartMontage ? OneHandedStartMontage.Get() : StartMontage.Get();
	ActiveLoopMontage = bOneHanded && OneHandedLoopMontage ? OneHandedLoopMontage.Get() : LoopMontage.Get();
	ActiveEndMontage = bOneHanded && OneHandedEndMontage ? OneHandedEndMontage.Get() : EndMontage.Get();
	ActiveLeapPoseMontage = bOneHanded && OneHandedLeapPoseMontage ? OneHandedLeapPoseMontage.Get() : LeapPoseMontage.Get();

	SavedGravityScale = Movement->GravityScale;
	SavedAirControl = Movement->AirControl;
	bMovementSaved = true;
	if (bLeapToTarget)
	{
		// (2026-10-15) 점프 베기: 중력은 두고 대상 앞 땅으로 뛰어간다.
		Movement->AirControl = 0.f;
		UAbilityTask_WaitGameplayEvent* const LeapHitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, knightGameplayTags::Event_Melee_Hit, nullptr, false, true);
		LeapHitTask->EventReceived.AddDynamic(this, &ThisClass::OnMeleeHitEvent);
		LeapHitTask->ReadyForActivation();
		StartLeap();
		return;
	}

	// 낙하 모드는 그대로 두고 중력·공중 조작을 끈다(애님은 공중 자세 유지). 속도는 Tick이 정한다.
	Movement->GravityScale = 0.f;
	Movement->AirControl = 0.f;
	Movement->Velocity = FVector(Movement->Velocity.X * 0.3f, Movement->Velocity.Y * 0.3f, BoostUpSpeed);

	UAbilityTask_WaitGameplayEvent* const HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, knightGameplayTags::Event_Melee_Hit, nullptr, false, true);
	HitTask->EventReceived.AddDynamic(this, &ThisClass::OnMeleeHitEvent);
	HitTask->ReadyForActivation();

	// 반동: 발 추진기가 아래로 짧게 터지며 살짝 떠오른다.
	Phase = EPhase::Boost;
	PhaseStartTime = World->GetTimeSeconds();
	PlayMontage(ActiveStartMontage, false);
	bThrustersReversed = false;
	SpawnThrusters(BoostThrusterScale);
	PlayThrusterSound(BoostSound);
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
	if (!Montage)
	{
		return;
	}
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
		if (AActor* const Avatar = GetAvatarActorFromActorInfo())
		{
			if (UknightGroggyFeedbackComponent* const Feedback = Avatar->FindComponentByClass<UknightGroggyFeedbackComponent>())
			{
				Feedback->WatchAttack(this, Montage, GetStanceScaledHitParams(HitParams), &HitActorsThisAttack);
			}
		}
		return;
	}
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (UAnimInstance* const Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
	{
		Anim->Montage_Play(Montage, Rate);
		if (AActor* const Avatar = GetAvatarActorFromActorInfo())
		{
			if (UknightGroggyFeedbackComponent* const Feedback = Avatar->FindComponentByClass<UknightGroggyFeedbackComponent>())
			{
				Feedback->WatchAttack(this, Montage, GetStanceScaledHitParams(HitParams), &HitActorsThisAttack);
			}
		}
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

void UknightGameplayAbility_PlungeAttack::PlayThrusterSound(USoundBase* Sound)
{
	StopThrusterSound(0.08f);
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USceneComponent* const AttachComp = Character ? Character->GetRootComponent() : nullptr;
	if (!Sound || !AttachComp)
	{
		return;
	}
	ActiveThrusterAudio = UGameplayStatics::SpawnSoundAttached(
		Sound, AttachComp, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true);
}

void UknightGameplayAbility_PlungeAttack::StopThrusterSound(float FadeOutTime)
{
	if (ActiveThrusterAudio)
	{
		if (ActiveThrusterAudio->IsPlaying())
		{
			if (FadeOutTime > 0.f)
			{
				ActiveThrusterAudio->FadeOut(FadeOutTime, 0.f);
			}
			else
			{
				ActiveThrusterAudio->Stop();
			}
		}
		ActiveThrusterAudio = nullptr;
	}
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

	FaceScreenCenterForPlungeAttack(Character, GetAbilitySystemComponentFromActorInfo());

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
	PlayMontage(ActiveLoopMontage, false);
	bThrustersReversed = true;
	SpawnThrusters(DiveThrusterScale);
	PlayThrusterSound(DiveSound);
}

void UknightGameplayAbility_PlungeAttack::StartLeap()
{
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
	UWorld* const World = GetWorld();
	if (!Character || !Movement || !World)
	{
		return;
	}
	FaceScreenCenterForPlungeAttack(Character, GetAbilitySystemComponentFromActorInfo());
	const FVector Start = Character->GetActorLocation();
	FVector Destination = Start;
	FRotator AimRotation = FRotator::ZeroRotator;
	float Travel = 0.f;
	const bool bHasLeapTarget = MaxAimDistance > 0.f && UknightCombatLibrary::ComputeAttackWarp(Character, MaxAimDistance, AimStopDistance, true,
		SoftTargetRange, SoftTargetAngle, Destination, AimRotation, Travel);
	if (bHasLeapTarget)
	{
		Character->SetActorRotation(FRotator(0.f, AimRotation.Yaw, 0.f));
	}
	else
	{
		Destination = Start + Character->GetActorForwardVector().GetSafeNormal2D() * LeapNoTargetDistance;
	}

	// 도착 높이: 도착 지점 아래 땅 + 캡슐 반 높이. 땅을 못 찾으면 지금 높이에서 내려간다고 본다.
	const float HalfHeight = Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
	FHitResult Ground;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(knightLeapGround), false, Character);
	const FVector Probe(Destination.X, Destination.Y, FMath::Max(Start.Z, Destination.Z) + 300.f);
	// 지형·정적 메시만 본다(큰 몬스터 몸 위를 땅으로 잡지 않게).
	const double TargetZ = World->LineTraceSingleByObjectType(Ground, Probe, Probe - FVector(0.f, 0.f, 20000.f),
		FCollisionObjectQueryParams(ECC_WorldStatic), Params) ? Ground.ImpactPoint.Z + HalfHeight : Start.Z - 200.0;

	FVector Horizontal = Destination - Start;
	Horizontal.Z = 0.f;
	const float Distance = Horizontal.Size();
	const float Time = FMath::Clamp(Distance / FMath::Max(LeapSpeed, 1.f), MinLeapTime, MaxLeapTime);
	const float Gravity = -Movement->GetGravityZ();
	// 포물선: Time 뒤에 TargetZ에 닿는 수직 출발 속도.
	const float VerticalSpeed = static_cast<float>((TargetZ - Start.Z) + 0.5 * Gravity * Time * Time) / Time;
	LeapHorizontalVelocity = Horizontal / Time;
	Movement->Velocity = LeapHorizontalVelocity + FVector(0.f, 0.f, VerticalSpeed);

	Phase = EPhase::Leap;
	PhaseStartTime = World->GetTimeSeconds();
	LeapDuration = Time;
	LeapDestination = FVector(Destination.X, Destination.Y, TargetZ);
	// 뛰어가는 동안은 점프 자세, 도착 지점에서 LeapSwingDistance 안으로 들어오면 휘두른다.
	bLeapSwingStarted = !ActiveLeapPoseMontage || GetLeapRemainingDistance() <= LeapSwingDistance;
	PlayMontage(bLeapSwingStarted ? ActiveStartMontage.Get() : ActiveLeapPoseMontage.Get(), false);
	// (2026-10-26) 대상 쪽으로 뛰어갈 때만 락온 돌진과 같은 잔상. 휘두르기가 시작되거나(bLeapSwingStarted) 도착 지점 1m 안이면
	// 멈추고, 처음부터 휘두르는 거리면 남기지 않는다(몬스터에 붙어 있을 때). 착지(Time)나 어빌리티가 끝나도 멈춘다.
	if (bLeapGhostTrail && bHasLeapTarget && !bLeapSwingStarted)
	{
		const TWeakObjectPtr<ThisClass> WeakThis(this);
		AknightGhostTrailActor::StartGhostTrailForAbility(this, LeapGhostTrailSettings, Time, LeapDestination,
			[WeakThis]() { return !WeakThis.IsValid() || WeakThis->bLeapSwingStarted; });
	}
	if (ThrusterEffect)
	{
		bThrustersReversed = false;
		SpawnThrusters(BoostThrusterScale);
	}
	PlayThrusterSound(BoostSound);
	World->GetTimerManager().SetTimer(TickTimer, this, &ThisClass::TickPlunge, PlungeTickInterval, true);
}

float UknightGameplayAbility_PlungeAttack::GetLeapRemainingDistance() const
{
	const AActor* const Avatar = GetAvatarActorFromActorInfo();
	return Avatar ? static_cast<float>(FVector::Dist(Avatar->GetActorLocation(), LeapDestination)) : 0.f;
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
			if (!Anim->Montage_IsPlaying(ActiveLoopMontage))
			{
				Anim->Montage_Play(ActiveLoopMontage, PlayRate * GetAttackSpeed());
			}
		}
		break;
	}
	case EPhase::Leap:
	{
		// 출발 후 잠깐은 아직 땅에 붙어 있을 수 있어 착지 검사를 미룬다.
		if (Elapsed > 0.1f && !Movement->IsFalling())
		{
			StartLand();
			break;
		}
		if (Elapsed > MaxDiveTime)
		{
			EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
			break;
		}
		// 수평 속도는 고정(도착 지점에 맞게), 수직은 중력이 맡는다.
		Movement->Velocity.X = LeapHorizontalVelocity.X;
		Movement->Velocity.Y = LeapHorizontalVelocity.Y;
		if (UAnimInstance* const Anim = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
		{
			if (!bLeapSwingStarted)
			{
				if (GetLeapRemainingDistance() <= LeapSwingDistance)
				{
					// 대상 가까이: 휘두르기.
					bLeapSwingStarted = true;
					PlayMontage(ActiveStartMontage, false);
				}
				else if (!Anim->Montage_IsPlaying(ActiveLeapPoseMontage))
				{
					Anim->Montage_Play(ActiveLeapPoseMontage, 1.f);
				}
			}
			// 휘두르기가 끝나면 착지할 때까지 낙하 자세를 반복한다.
			else if (!Anim->Montage_IsPlaying(ActiveStartMontage) && !Anim->Montage_IsPlaying(ActiveLoopMontage))
			{
				Anim->Montage_Play(ActiveLoopMontage, PlayRate * GetAttackSpeed());
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
	StopThrusterSound(0.1f);
	ResetLean();
	RestoreMovement();
	// 착지 흔들림·착지 경직은 무브먼트 컴포넌트가 떨어진 속도로 이미 처리했다. 여기서는 내려찍는 동작과 타격.
	PlayMontage(ActiveEndMontage, true);
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
	const ACharacter* const Character = Cast<ACharacter>(Avatar);
	const float HalfHeight = Character && Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
	const FVector Feet = Avatar->GetActorLocation() - FVector(0.f, 0.f, HalfHeight) + Avatar->GetActorForwardVector() * 100.f;
	if (ImpactEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, ImpactEffect, Feet, Avatar->GetActorRotation(), FVector(ImpactEffectScale),
			true, true, ENCPoolMethod::AutoRelease);
	}

	FVector GroundImpactLoc = Feet;
	FHitResult GroundHit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(knightPlungeImpactCrack), false, Avatar);
	if (World->LineTraceSingleByObjectType(GroundHit, Feet + FVector(0.f, 0.f, 150.f),
		Feet - FVector(0.f, 0.f, 350.f), FCollisionObjectQueryParams(ECC_WorldStatic), QueryParams))
	{
		GroundImpactLoc = GroundHit.ImpactPoint;
	}

	const bool bTwoHanded = UknightGameplayAbility_StanceSwitch::IsTwoHanded(GetAbilitySystemComponentFromActorInfo());
	UNiagaraSystem* CrackAsset = GroundCrackEffect;
	if (!CrackAsset)
	{
		CrackAsset = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/SlashHitVFX/NS/NS_Hit_GroundCrack.NS_Hit_GroundCrack"));
	}
	if (CrackAsset)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, CrackAsset, GroundImpactLoc, Avatar->GetActorRotation(),
			FVector(bTwoHanded ? 1.35f : 1.05f), true, true, ENCPoolMethod::AutoRelease);
	}

	if (!bLeapToTarget || bTwoHanded)
	{
		UNiagaraSystem* RiftAsset = GroundRiftEffect;
		if (!RiftAsset)
		{
			RiftAsset = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/SlashHitVFX/NS/NS_Hit_GroundRift.NS_Hit_GroundRift"));
		}
		if (RiftAsset)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, RiftAsset, GroundImpactLoc, Avatar->GetActorRotation(),
				FVector(bTwoHanded ? 1.15f : 0.9f), true, true, ENCPoolMethod::AutoRelease);
		}
	}

	UNiagaraSystem* DustAsset = GroundDustEffect;
	if (!DustAsset)
	{
		DustAsset = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Knight/Creatures/Shared/Effects/NS_Monster_GroundDust.NS_Monster_GroundDust"));
	}
	if (DustAsset)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, DustAsset, GroundImpactLoc, Avatar->GetActorRotation(),
			FVector(bTwoHanded ? 1.3f : 1.0f), true, true, ENCPoolMethod::AutoRelease);
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
	// (2026-10-20) 한 번 발동에 대상마다 한 번만(시작 베기와 착지 충격이 둘 다 맞히던 것).
	if (Hit && Hit->GetActor() && !HitActorsThisAttack.Contains(Hit->GetActor()))
	{
		HitActorsThisAttack.Add(Hit->GetActor());
		UknightCombatLibrary::ApplyMeleeHit(GetAvatarActorFromActorInfo(), *Hit, GetStanceScaledHitParams(HitParams), this);
	}
}

void UknightGameplayAbility_PlungeAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AActor* const Avatar = GetAvatarActorFromActorInfo())
	{
		if (UknightGroggyFeedbackComponent* const Feedback = Avatar->FindComponentByClass<UknightGroggyFeedbackComponent>())
		{
			Feedback->EndPredictionForAttack(this);
		}
	}
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhaseTimer);
		World->GetTimerManager().ClearTimer(TickTimer);
		World->GetTimerManager().ClearTimer(ImpactTimer);
	}
	// 공중에서 끊긴 경우(피격 등)에도 중력·공중 조작·기울기·추진기를 원래대로 돌린다.
	StopThrusters();
	StopThrusterSound(0.1f);
	ResetLean();
	RestoreMovement();
	if (Phase != EPhase::Land && IsActive())
	{
		const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
		if (UAnimInstance* const Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
		{
			Anim->Montage_Stop(0.2f, ActiveStartMontage);
			Anim->Montage_Stop(0.2f, ActiveLoopMontage);
			if (ActiveLeapPoseMontage)
			{
				Anim->Montage_Stop(0.2f, ActiveLeapPoseMontage);
			}
		}
	}
	Phase = EPhase::None;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
