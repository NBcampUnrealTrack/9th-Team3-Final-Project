#include "AbilitySystem/Abilities/knightGameplayAbility_JumpHover.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraShakeBase.h"
#include "Components/AudioComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Character/knightMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "System/knightGameplayTags.h"
#include "TimerManager.h"
#include "knight.h"
#include "NativeGameplayTags.h"

// 체공 중 상태. State.Boosting은 전투 자세(CombatStance)를 켜므로 따로 쓴다.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Hovering, "State.Hovering");

namespace
{
	constexpr float TickInterval = 1.f / 60.f;
	/** Braking used while hovering with no input (cm/s^2). The normal falling value is much stronger. */
	constexpr float HoverBraking = 150.f;
}

UknightGameplayAbility_JumpHover::UknightGameplayAbility_JumpHover(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EknightAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = EknightAbilityActivationGroup::Independent;
	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_Boost_Flight));
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_HitReact);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_InputBlocked);
	ThrusterSockets = { TEXT("foot_l"), TEXT("foot_r") };
}

void UknightGameplayAbility_JumpHover::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	bHovering = false;
	bJumped = false;

	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UknightMovementComponent* const Movement = Character ? Cast<UknightMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	if (!Movement || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (Movement->IsMovingOnGround())
	{
		// 공격·검 뽑기·대시 몽타주 중에는 점프하지 않는다.
		const UAnimInstance* const Anim = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
		if ((Anim && Anim->IsAnyMontagePlaying()) || !Character->CanJump())
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
		Character->Jump();
		bJumped = true;
	}
	else if (!StartHover())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_WaitInputRelease* const Task = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	Task->OnRelease.AddDynamic(this, &ThisClass::OnReleased);
	Task->ReadyForActivation();
}

bool UknightGameplayAbility_JumpHover::StartHover()
{
	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UknightMovementComponent* const Movement = Character ? Cast<UknightMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	const UWorld* const World = GetWorld();
	if (!ASC || !Movement || !World || !Movement->IsFalling() || ASC->HasMatchingGameplayTag(knightGameplayTags::State_Overheated))
	{
		return false;
	}
	const FGameplayAttribute Gauge = UknightAttributeSet::GetBoostGaugeAttribute();
	if (ASC->HasAttributeSetForAttribute(Gauge) && ASC->GetNumericAttribute(Gauge) < MinGaugeToHover)
	{
		return false;
	}

	// 낙하 모드는 유지하고 중력만 끈다. 애님 블루프린트가 공중 자세를 계속 쓴다.
	SavedGravityScale = Movement->GravityScale;
	SavedAirControl = Movement->AirControl;
	SavedLateralFriction = Movement->FallingLateralFriction;
	SavedBrakingFalling = Movement->BrakingDecelerationFalling;
	Movement->GravityScale = 0.f;
	Movement->AirControl = HoverAirControl;
	Movement->FallingLateralFriction = HoverDrag;
	Movement->BrakingDecelerationFalling = HoverBraking;

	// 떨어지는 중이면 살짝 위로 튀고, 올라가는 중이면 그 속도에서 부드럽게 멈춘다(Tick).
	if (Movement->Velocity.Z < 0.f)
	{
		Movement->Velocity.Z = HoverStartLift;
	}

	ASC->AddLooseGameplayTag(TAG_State_Hovering);
	bHovering = true;
	HoverStartTime = World->GetTimeSeconds();
	EnsureTicking();
	StartEffects();
	UE_LOG(LogknightAbility, Verbose, TEXT("JumpHover: hover start"));
	return true;
}

void UknightGameplayAbility_JumpHover::EnsureTicking()
{
	UWorld* const World = GetWorld();
	if (World && !World->GetTimerManager().IsTimerActive(TickTimer))
	{
		LastTickTime = World->GetTimeSeconds();
		World->GetTimerManager().SetTimer(TickTimer, FTimerDelegate::CreateUObject(this, &ThisClass::TickHover), TickInterval, true);
	}
}

void UknightGameplayAbility_JumpHover::TickHover()
{
	UWorld* const World = GetWorld();
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UknightMovementComponent* const Movement = Character ? Cast<UknightMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	if (!World || !Movement)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(TickTimer);
		}
		return;
	}
	const double Now = World->GetTimeSeconds();
	const float DeltaTime = FMath::Max(static_cast<float>(Now - LastTickTime), KINDA_SMALL_NUMBER);
	LastTickTime = Now;

	FRotator TargetTilt = FRotator::ZeroRotator;
	if (bHovering)
	{
		if (!Movement->IsFalling())
		{
			// 착지했거나 다른 이동(대시 등)이 낙하 상태를 끝냈다.
			EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
			return;
		}

		// 수직: 오르내림 목표 속도로 부드럽게 맞춘다. 위치 sin → 속도 cos.
		const float Omega = 2.f * PI / HoverBobPeriod;
		const float Phase = static_cast<float>(Now - HoverStartTime) * Omega;
		const float TargetZ = HoverBobAmplitude * Omega * FMath::Cos(Phase);
		Movement->Velocity.Z = FMath::FInterpTo(Movement->Velocity.Z, TargetZ, DeltaTime, HoverCatchSpeed);

		// 게이지.
		float GaugeValue = -1.f;
		if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
		{
			const FGameplayAttribute Gauge = UknightAttributeSet::GetBoostGaugeAttribute();
			if (ASC->HasAttributeSetForAttribute(Gauge) && HoverDrainPerSecond > 0.f)
			{
				// 부스트 회복 GE는 지금 항상 돌고 있어서(막는 태그 없음), 켜 두면 그만큼 더 빼서 체공 중에는 회복이 없게 한다.
				float Drain = HoverDrainPerSecond;
				if (bCancelGaugeRecovery && ASC->HasAttributeSetForAttribute(UknightAttributeSet::GetBoostRecoveryRateAttribute()))
				{
					Drain += FMath::Max(0.f, ASC->GetNumericAttribute(UknightAttributeSet::GetBoostRecoveryRateAttribute()));
				}
				ASC->ApplyModToAttribute(Gauge, EGameplayModOp::Additive, -Drain * DeltaTime);
				GaugeValue = ASC->GetNumericAttribute(Gauge);
				if (GaugeValue <= 0.f)
				{
					UE_LOG(LogknightAbility, Log, TEXT("JumpHover: boost gauge empty, falling"));
					EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
					return;
				}
			}
		}

		// 기울기: 미는 방향(가속 입력) 기준. 앞이면 앞으로 숙이고(피치 -), 오른쪽이면 오른쪽으로 기운다(롤 +).
		const FVector Accel = Movement->GetCurrentAcceleration().GetSafeNormal2D();
		const FVector Local = Character->GetActorRotation().UnrotateVector(Accel);
		TargetTilt = FRotator(-Local.X * HoverMaxTilt, 0.f, Local.Y * HoverMaxTilt);

		UpdateLoopAudio(DeltaTime, Movement->Velocity.Size2D(), GaugeValue, Now);
	}

	UpdateTilt(DeltaTime, TargetTilt);
	UpdateThrusterRotation();

	if (!bHovering && !bTiltActive)
	{
		World->GetTimerManager().ClearTimer(TickTimer);
	}
}

void UknightGameplayAbility_JumpHover::UpdateTilt(float DeltaTime, const FRotator& Target)
{
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh || (HoverMaxTilt <= 0.f && !bTiltActive))
	{
		return;
	}
	if (!bTiltActive)
	{
		if (Target.IsNearlyZero(0.01f))
		{
			return;
		}
		// 똑바로 선 상태에서 기울기를 시작할 때만 기준 자세를 저장한다.
		MeshBaseLocation = Mesh->GetRelativeLocation();
		MeshBaseRotation = Mesh->GetRelativeRotation().Quaternion();
		CurrentTilt = FRotator::ZeroRotator;
		bTiltActive = true;
	}

	CurrentTilt = FMath::RInterpTo(CurrentTilt, Target, DeltaTime, HoverTiltSpeed);
	if (Target.IsNearlyZero(0.01f) && CurrentTilt.IsNearlyZero(0.05f))
	{
		Mesh->SetRelativeLocationAndRotation(MeshBaseLocation, MeshBaseRotation);
		CurrentTilt = FRotator::ZeroRotator;
		bTiltActive = false;
		return;
	}
	// 캡슐 가운데(액터 원점)를 축으로 돌린다. 메시 기준 위치도 같이 돌려서 발밑이 아니라 몸 가운데가 축이 된다.
	const FQuat Tilt = CurrentTilt.Quaternion();
	Mesh->SetRelativeLocationAndRotation(Tilt.RotateVector(MeshBaseLocation), Tilt * MeshBaseRotation);
}

void UknightGameplayAbility_JumpHover::StopHover()
{
	if (!bHovering)
	{
		return;
	}
	bHovering = false;
	if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveLooseGameplayTag(TAG_State_Hovering);
	}
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (UknightMovementComponent* const Movement = Character ? Cast<UknightMovementComponent>(Character->GetCharacterMovement()) : nullptr)
	{
		Movement->GravityScale = SavedGravityScale;
		Movement->AirControl = SavedAirControl;
		Movement->FallingLateralFriction = SavedLateralFriction;
		Movement->BrakingDecelerationFalling = SavedBrakingFalling;
	}
	StopEffects();
	// 기울기는 Tick이 계속 돌면서 똑바로 되돌린 뒤 멈춘다.
}

void UknightGameplayAbility_JumpHover::StartEffects()
{
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}
	if (ThrusterEffect)
	{
		for (const FName Socket : ThrusterSockets)
		{
			if (!Mesh->DoesSocketExist(Socket))
			{
				UE_LOG(LogknightAbility, Warning, TEXT("JumpHover: socket '%s' not found on %s"), *Socket.ToString(), *GetNameSafe(Mesh->GetSkeletalMeshAsset()));
				continue;
			}
			UNiagaraComponent* const Jet = UNiagaraFunctionLibrary::SpawnSystemAttached(ThrusterEffect, Mesh, Socket,
				FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
			if (Jet)
			{
				// 뼈 회전과 상관없이 몸 기준 아래로 뿜게 한다(UpdateThrusterRotation).
				Jet->SetUsingAbsoluteRotation(true);
				Jet->SetWorldScale3D(FVector(ThrusterScale));
				ThrusterComponents.Add(Jet);
			}
		}
		UpdateThrusterRotation();
	}
	if (HoverStartSound)
	{
		UGameplayStatics::PlaySoundAtLocation(Character, HoverStartSound, Character->GetActorLocation());
	}
	if (HoverLoopSound)
	{
		LoopAudio = UGameplayStatics::SpawnSoundAttached(HoverLoopSound, Mesh);
		if (LoopAudio)
		{
			// 처음에는 제자리 소리(낮게 웅웅)로 시작한다. 필터·피치는 Tick에서 속도를 따라간다.
			LoopBlend = 0.f;
			LoopAudio->SetLowPassFilterEnabled(true);
			LoopAudio->SetLowPassFilterFrequency(HoverLoopLowPassIdle);
			LoopAudio->SetPitchMultiplier(HoverLoopPitchIdle);
			LoopAudio->FadeIn(0.15f);
		}
	}
	if (HoverStartShake)
	{
		if (APlayerController* const PC = Cast<APlayerController>(Character->GetController()))
		{
			PC->ClientStartCameraShake(HoverStartShake, HoverStartShakeScale);
		}
	}
}

void UknightGameplayAbility_JumpHover::StopEffects()
{
	for (UNiagaraComponent* const Jet : ThrusterComponents)
	{
		if (Jet)
		{
			// 이미 나온 입자는 끝까지 보이고, 다 사라지면 컴포넌트가 스스로 지워진다.
			Jet->Deactivate();
		}
	}
	ThrusterComponents.Reset();
	if (LoopAudio)
	{
		LoopAudio->FadeOut(0.25f, 0.f);
		LoopAudio = nullptr;
	}
}

void UknightGameplayAbility_JumpHover::UpdateLoopAudio(float DeltaTime, float HorizontalSpeed, float GaugeValue, double Now)
{
	if (!LoopAudio)
	{
		return;
	}
	// 속도 비율을 부드럽게 따라간다(갑자기 바뀌면 소리가 튄다).
	const float Target = FMath::Clamp(HorizontalSpeed / FMath::Max(HoverLoopFullSpeed, 1.f), 0.f, 1.f);
	LoopBlend = FMath::FInterpTo(LoopBlend, Target, DeltaTime, 4.f);

	// 필터는 귀가 로그로 듣기 때문에 주파수도 로그로 섞는다.
	const float LogIdle = FMath::Loge(FMath::Max(HoverLoopLowPassIdle, 20.f));
	const float LogMoving = FMath::Loge(FMath::Max(HoverLoopLowPassMoving, 20.f));
	LoopAudio->SetLowPassFilterFrequency(FMath::Exp(FMath::Lerp(LogIdle, LogMoving, LoopBlend)));
	LoopAudio->SetPitchMultiplier(FMath::Lerp(HoverLoopPitchIdle, HoverLoopPitchMoving, LoopBlend));

	// 게이지가 바닥에 가까우면 추진이 불안정하게 떨린다. 적을수록 더 크게, 더 빠르게.
	float Volume = 1.f;
	if (HoverLoopLowGauge > 0.f && GaugeValue >= 0.f && GaugeValue < HoverLoopLowGauge)
	{
		const float Low = 1.f - GaugeValue / HoverLoopLowGauge;
		const float Rate = FMath::Lerp(6.f, 14.f, Low);
		Volume = 1.f - 0.45f * Low * (0.5f + 0.5f * FMath::Sin(static_cast<float>(Now) * Rate * 2.f * PI));
	}
	LoopAudio->SetVolumeMultiplier(Volume);
}

void UknightGameplayAbility_JumpHover::UpdateThrusterRotation() const
{
	const AActor* const Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || ThrusterComponents.IsEmpty())
	{
		return;
	}
	// 몸이 기운 만큼 분사도 기운다(기울기는 액터 기준).
	const FQuat Rotation = Avatar->GetActorQuat() * CurrentTilt.Quaternion();
	for (UNiagaraComponent* const Jet : ThrusterComponents)
	{
		if (Jet)
		{
			Jet->SetWorldRotation(Rotation);
		}
	}
}

void UknightGameplayAbility_JumpHover::OnReleased(float TimeHeld)
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UknightGameplayAbility_JumpHover::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopHover();
	if (bJumped)
	{
		bJumped = false;
		if (ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
		{
			Character->StopJumping();
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
