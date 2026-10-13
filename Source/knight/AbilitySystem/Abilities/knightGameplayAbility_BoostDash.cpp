#include "AbilitySystem/Abilities/knightGameplayAbility_BoostDash.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Character/knightMovementComponent.h"
#include "Engine/World.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "knight.h"
#include "Perception/AISense_Hearing.h"
#include "System/knightGameplayTags.h"

UknightGameplayAbility_BoostDash::UknightGameplayAbility_BoostDash(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EknightAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = EknightAbilityActivationGroup::Exclusive_Replaceable;
	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_Boost_Dash));
	ActivationOwnedTags.AddTag(knightGameplayTags::State_Boosting);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_HitReact);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Overheated);
	DashCueTag = knightGameplayTags::GameplayCue_Boost_Dash;

	// 경직 중 대시 캔슬: 실행 중에 다시 발동되면 지금 대시를 끝내고 새로 시작한다. 언제 허용할지는 CanActivateAbility가 정한다.
	bRetriggerInstancedAbility = true;

	// 퀵스텝은 행동 에너지가 비용만큼 있어야 나간다(2026-10-09 사용자 결정). 비용 값은 BP의 StaminaCost(전에는 Cost GE).
	bRequireFullStaminaCost = true;

	// 등 부스터 두 개: 등 뒤쪽(X -), 양옆(Y ±). (2026-10-12)
	BackThrusterOffsets = { FVector(-15.f, -12.f, 4.f), FVector(-15.f, 12.f, 4.f) };
}

float UknightGameplayAbility_BoostDash::GetMechAttributeValue(const FGameplayAttribute& Attribute, float Fallback) const
{
	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	return ASC && ASC->HasAttributeSetForAttribute(Attribute) ? ASC->GetNumericAttribute(Attribute) : Fallback;
}

bool UknightGameplayAbility_BoostDash::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	// 퀵스텝 쿨타임 중이면 발동하지 않는다.
	const UWorld* const World = GetWorld();
	if (World && World->GetTimeSeconds() < CooldownEndTime)
	{
		return false;
	}
	// 이미 대시 중이면 경직 중일 때만 다시 발동(새 대시로 다시 시작)을 허용한다. 이동 도중에는 끊지 않는다.
	if (IsActive() && !bInRecovery)
	{
		return false;
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UknightGameplayAbility_BoostDash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	StartDash(*Character);
}

void UknightGameplayAbility_BoostDash::StartDash(ACharacter& Character)
{
	bCooldownPending = true;

	FVector Direction = Character.GetCharacterMovement()->GetLastInputVector().GetSafeNormal2D();
	const bool bHasInput = !Direction.IsNearlyZero();
	if (!bHasInput)
	{
		Direction = -Character.GetActorForwardVector().GetSafeNormal2D();
	}
	DashDirection = Direction;

	const UAbilitySystemComponent* const OwnerASC = GetAbilitySystemComponentFromActorInfo();
	const bool bLockedOn = OwnerASC && OwnerASC->HasMatchingGameplayTag(knightGameplayTags::State_LockedOn);
	if (bFaceDashDirectionWhenFree && bHasInput && !bLockedOn)
	{
		Character.SetActorRotation(FRotator(0.f, Direction.Rotation().Yaw, 0.f));
	}
	UAnimMontage* const Montage = SelectDashMontage(Character, Direction);

	if (InvulnerableDuration > 0.f)
	{
		if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->AddLooseGameplayTag(knightGameplayTags::State_Invulnerable);
			bInvulnerableApplied = true;
		}
		UAbilityTask_WaitDelay* const Task = UAbilityTask_WaitDelay::WaitDelay(this, InvulnerableDuration);
		Task->OnFinish.AddDynamic(this, &ThisClass::OnInvulnerabilityEnded);
		Task->ReadyForActivation();
	}

	if (Montage)
	{
		UAbilityTask_PlayMontageAndWait* const Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, Montage, GetMontagePlayRate(Montage), NAME_None, true, 0.f);
		Task->ReadyForActivation();
	}

	// 거리 = 기본 거리 x 퀵스텝 속도 배율 x 중량 계수. 시간은 고정이라 거리가 늘면 속도도 빨라진다.
	const UknightMovementComponent* const KnightMovement = Cast<UknightMovementComponent>(Character.GetCharacterMovement());
	const float WeightFactor = KnightMovement ? KnightMovement->GetWeightSpeedFactor() : 1.f;
	const float Distance = DashDistance * GetMechAttributeValue(UknightAttributeSet::GetDodgeSpeedAttribute(), 1.f) * WeightFactor;
	const float Strength = Distance / DashDuration;
	UAbilityTask_ApplyRootMotionConstantForce* const DashTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("BoostDash"), Direction, Strength, DashDuration, false, StrengthOverTime,
		ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, Character.GetCharacterMovement()->GetMaxSpeed(), false);
	DashTask->OnFinish.AddDynamic(this, &ThisClass::OnDashFinished);
	DashTask->ReadyForActivation();

	StartBackThrusters(Character);

	UAbilitySystemComponent* const CueASC = DashCueTag.IsValid() ? GetAbilitySystemComponentFromActorInfo() : nullptr;
	if (UAbilitySystemComponent* const ASC = CueASC)
	{
		FGameplayCueParameters CueParams;
		CueParams.Location = Character.GetActorLocation();
		CueParams.Normal = Direction;
		CueParams.Instigator = &Character;
		ASC->ExecuteGameplayCue(DashCueTag, CueParams);
	}

	// 추진기는 시끄럽다: 아직 나이트를 못 본 몬스터도 소리를 들을 수 있다 (AI 청각).
	UAISense_Hearing::ReportNoiseEvent(&Character, Character.GetActorLocation(), NoiseLoudness, &Character, NoiseRange, TEXT("Boost"));
}

float UknightGameplayAbility_BoostDash::GetMontagePlayRate(const UAnimMontage* Montage) const
{
	// 지금 규칙: 고정 배속(MontagePlayRate). 헤더 주석 참고 — 바뀔 수 있는 항목.
	return MontagePlayRate;
}

UAnimMontage* UknightGameplayAbility_BoostDash::SelectDashMontage(const ACharacter& Character, const FVector& Direction) const
{
	const FVector Forward = Character.GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = Character.GetActorRightVector().GetSafeNormal2D();
	const float ForwardDot = FVector::DotProduct(Direction, Forward);
	const float RightDot = FVector::DotProduct(Direction, Right);

	UAnimMontage* Picked = DashMontage;
	if (FMath::Abs(ForwardDot) >= FMath::Abs(RightDot))
	{
		if (ForwardDot < 0.f && DashMontageBack)
		{
			Picked = DashMontageBack;
		}
	}
	else if (RightDot > 0.f)
	{
		Picked = DashMontageRight ? DashMontageRight.Get() : DashMontage.Get();
	}
	else
	{
		Picked = DashMontageLeft ? DashMontageLeft.Get() : DashMontage.Get();
	}
	return Picked;
}

void UknightGameplayAbility_BoostDash::OnDashFinished()
{
	// 등 부스터는 이동이 끝나면 잠깐 뒤에 끈다. (2026-10-12)
	if (UWorld* const World = GetWorld(); World && BackThrusterLinger > 0.f)
	{
		World->GetTimerManager().SetTimer(BackThrusterStopTimer, this, &ThisClass::StopBackThrusters, BackThrusterLinger, false);
	}
	else
	{
		StopBackThrusters();
	}
	// 쿨타임은 이동이 끝난 시점부터 센다. 그래야 경직 중에 쿨타임이 끝나 대시로 캔슬할 수 있다.
	StartCooldown();
	if (RecoveryDuration <= 0.f)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
		return;
	}

	// 경직: 공격·가드가 대시를 대신할 수 있게 캔슬 창을 열고, 이동 입력으로는 가속하지 못하게 한다(미끄러지며 정지).
	bInRecovery = true;
	if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AddLooseGameplayTag(knightGameplayTags::Window_Cancel);
		bCancelWindowApplied = true;
	}
	if (const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UknightMovementComponent* const KnightMovement = Cast<UknightMovementComponent>(Character->GetCharacterMovement()))
		{
			// 관성 이어가기: 대시 방향 쪽(CarryMomentumMaxAngle 안쪽)으로 방향키를 누르고 있으면 잠그지 않는다.
			// 대시가 끝날 때 속도는 걷기·달리기 최고 속도로 잘려 있어서, 잠그지 않으면 그 속도 그대로 이어서 움직인다.
			const FVector HeldInput = KnightMovement->GetLastInputVector().GetSafeNormal2D();
			const bool bCarryMomentum = CarryMomentumMaxAngle > 0.f && !HeldInput.IsNearlyZero()
				&& FVector::DotProduct(HeldInput, DashDirection) >= FMath::Cos(FMath::DegreesToRadians(CarryMomentumMaxAngle));
			if (!bCarryMomentum)
			{
				KnightMovement->SetMoveInputLocked(true);
				bMoveLockApplied = true;
			}
		}
	}

	// 경직 동안 대시 재발동 요청이 오면 CanActivateAbility가 허용하고, 엔진이 이 대시를 끝낸 뒤 새로 시작한다.
	UAbilityTask_WaitDelay* const Task = UAbilityTask_WaitDelay::WaitDelay(this, RecoveryDuration);
	Task->OnFinish.AddDynamic(this, &ThisClass::OnRecoveryFinished);
	Task->ReadyForActivation();
}

void UknightGameplayAbility_BoostDash::OnRecoveryFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UknightGameplayAbility_BoostDash::ReleaseRecoveryState()
{
	bInRecovery = false;
	if (bCancelWindowApplied)
	{
		bCancelWindowApplied = false;
		if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveLooseGameplayTag(knightGameplayTags::Window_Cancel);
		}
	}
	if (bMoveLockApplied)
	{
		bMoveLockApplied = false;
		const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
		if (UknightMovementComponent* const KnightMovement = Character ? Cast<UknightMovementComponent>(Character->GetCharacterMovement()) : nullptr)
		{
			KnightMovement->SetMoveInputLocked(false);
		}
	}
}

void UknightGameplayAbility_BoostDash::StartCooldown()
{
	if (!bCooldownPending)
	{
		return;
	}
	bCooldownPending = false;
	const float Cooldown = GetMechAttributeValue(UknightAttributeSet::GetDodgeCooldownAttribute(), 0.f);
	if (const UWorld* const World = GetWorld(); World && Cooldown > 0.f)
	{
		CooldownEndTime = World->GetTimeSeconds() + Cooldown;
	}
}

void UknightGameplayAbility_BoostDash::StartBackThrusters(const ACharacter& Character)
{
	StopBackThrusters();
	USkeletalMeshComponent* const Mesh = Character.GetMesh();
	UWorld* const World = GetWorld();
	if (!BackThrusterEffect || !Mesh || !World)
	{
		UE_LOG(LogknightAbility, Warning, TEXT("BoostDash back booster: skipped (Effect=%s Mesh=%s World=%s)"),
			*GetNameSafe(BackThrusterEffect), *GetNameSafe(Mesh), *GetNameSafe(World));
		return;
	}
	for (int32 Index = 0; Index < BackThrusterOffsets.Num(); ++Index)
	{
		// 어디에도 붙이지 않고 월드에 만든 뒤, 위치·회전을 매 프레임 직접 정한다.
		UNiagaraComponent* const Jet = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, BackThrusterEffect, Character.GetActorLocation(),
			Character.GetActorRotation(), FVector(BackThrusterScale), true, true, ENCPoolMethod::None);
		if (Jet)
		{
			BackThrusters.Add(Jet);
		}
	}
	UpdateBackThrusters();
	if (bDebugBackThrusters)
	{
		UE_LOG(LogknightAbility, Log, TEXT("BoostDash back booster: spawned %d / %d jets, effect %s, bone %s exists=%d"),
			BackThrusters.Num(), BackThrusterOffsets.Num(), *GetNameSafe(BackThrusterEffect), *BackThrusterBone.ToString(),
			Mesh->DoesSocketExist(BackThrusterBone) ? 1 : 0);
		for (const UNiagaraComponent* const Jet : BackThrusters)
		{
			UE_LOG(LogknightAbility, Log, TEXT("  jet %s at %s rot %s active=%d visible=%d"), *GetNameSafe(Jet),
				*Jet->GetComponentLocation().ToCompactString(), *Jet->GetComponentRotation().ToCompactString(), Jet->IsActive() ? 1 : 0, Jet->IsVisible() ? 1 : 0);
		}
	}
	World->GetTimerManager().SetTimer(BackThrusterTimer, this, &ThisClass::UpdateBackThrusters, 1.f / 60.f, true);
}

void UknightGameplayAbility_BoostDash::UpdateBackThrusters()
{
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const USkeletalMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh || BackThrusters.IsEmpty())
	{
		return;
	}
	const FVector Anchor = Mesh->DoesSocketExist(BackThrusterBone) ? Mesh->GetSocketLocation(BackThrusterBone) : Character->GetActorLocation();
	const FQuat ActorQuat = Character->GetActorQuat();
	// 대시 반대 방향으로, 조금 아래로 뿜는다. 이펙트는 로컬 -Z로 뿜므로 로컬 Z를 뿜는 방향의 반대로 둔다.
	const FVector Back = DashDirection.IsNearlyZero() ? -Character->GetActorForwardVector() : -DashDirection;
	const float Down = FMath::DegreesToRadians(BackThrusterDownAngle);
	const FVector Emit = (Back.GetSafeNormal2D() * FMath::Cos(Down) - FVector::UpVector * FMath::Sin(Down)).GetSafeNormal();
	const FRotator Rotation = FRotationMatrix::MakeFromZ(-Emit).Rotator();
	for (int32 Index = 0; Index < BackThrusters.Num(); ++Index)
	{
		if (UNiagaraComponent* const Jet = BackThrusters[Index])
		{
			const FVector Offset = BackThrusterOffsets.IsValidIndex(Index) ? BackThrusterOffsets[Index] : FVector::ZeroVector;
			Jet->SetWorldLocationAndRotation(Anchor + ActorQuat.RotateVector(Offset), Rotation);
			if (bDebugBackThrusters)
			{
				const FVector Location = Jet->GetComponentLocation();
				DrawDebugSphere(Jet->GetWorld(), Location, 6.f, 8, FColor::Yellow, false, 0.f, SDPG_Foreground);
				DrawDebugLine(Jet->GetWorld(), Location, Location + Emit * 80.f, FColor::Red, false, 0.f, SDPG_Foreground, 2.f);
			}
		}
	}
}

void UknightGameplayAbility_BoostDash::StopBackThrusters()
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
			// 이미 나온 입자는 끝까지 보이고, 다 사라지면 컴포넌트가 스스로 지워진다.
			Jet->Deactivate();
		}
	}
	BackThrusters.Reset();
}

void UknightGameplayAbility_BoostDash::OnInvulnerabilityEnded()
{
	RemoveInvulnerability();
}

void UknightGameplayAbility_BoostDash::RemoveInvulnerability()
{
	if (bInvulnerableApplied)
	{
		bInvulnerableApplied = false;
		if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveLooseGameplayTag(knightGameplayTags::State_Invulnerable);
		}
	}
}

void UknightGameplayAbility_BoostDash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	RemoveInvulnerability();
	ReleaseRecoveryState();
	StopBackThrusters();

	// 이동이 끝나기 전에 끊긴 대시(피격 등)도 쿨타임은 건다. 비용 부족 등으로 시작도 못 했으면 걸지 않는다.
	StartCooldown();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
