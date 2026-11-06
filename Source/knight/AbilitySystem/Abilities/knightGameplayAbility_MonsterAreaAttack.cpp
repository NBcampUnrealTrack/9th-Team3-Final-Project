#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAreaAttack.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Camera/knightPlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "NiagaraComponent.h"
#include "Combat/knightCombatLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/RootMotionSource.h"
#include "Kismet/GameplayStatics.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "NiagaraFunctionLibrary.h"
#include "Monster/knightMonsterDebug.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "knight.h"
#include "knightPerfCounters.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "knightMonsterAreaAttack"

namespace knightMonsterAreaAttack
{
	/** 판정이 유지되는 동안 오버랩을 다시 하는 간격(몬스터 시간). */
	constexpr float ActiveCheckInterval = 1.f / 20.f;
	/** 판정 시계를 진행하는 간격(실제 시간). 판정 시각은 최대 이만큼 늦을 수 있다. */
	constexpr float ClockInterval = 1.f / 60.f;
	/** 시계 반복 횟수 상한. 어빌리티가 끝나면 시계도 같이 끝난다. */
	constexpr int32 MaxClockTicks = 100000;
}

UknightGameplayAbility_MonsterAreaAttack::UknightGameplayAbility_MonsterAreaAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 구체는 몬스터 앞쪽 AreaOffset.X + AreaRadius까지 닿으므로 그 안에서 고르도록 한다.
	Selection.MaxRange = 600.f;
	Selection.MaxAngle = 45.f;
}

void UknightGameplayAbility_MonsterAreaAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	HitActors.Reset();
	Knockback.Reset();
	bHitAreaEffectsPlayed = false;
	bWindupEffectStarted = false;
	AimYaw = 0.f;
	LastGroundFXYaw = 0.f;
	bAimSweeping = false;

	const FVector Center = GetAreaCenter();
	if (TelegraphCue.IsValid())
	{
		if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
		{
			FGameplayCueParameters CueParams;
			CueParams.Location = Center;
			CueParams.RawMagnitude = AreaRadius;
			CueParams.Instigator = GetAvatarActorFromActorInfo();
			ASC->ExecuteGameplayCue(TelegraphCue, CueParams);
		}
	}
#if ENABLE_DRAW_DEBUG
	if (knightMonsterDebug::ShouldDrawAttacks())
	{
		DrawDebugSphere(GetWorld(), Center, AreaRadius, 16, FColor::Orange, false, WindupTime);
	}
#endif

	if (Montage)
	{
		UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage);
		MontageTask->ReadyForActivation();
	}

	Elapsed = 0.f;
	RecoveryStart = -1.f;
	NextImpact = 0;
	bLungeStarted = false;
	bWindupDone = false;
	StartAttackWarning(Impacts.IsEmpty() ? WindupTime : Impacts[0].Time);
	// 월드 시간으로 기다리면 히트스톱 동안 몽타주만 멈추고 판정은 그대로 나간다. 몬스터 시간으로 직접 센다.
	UAbilityTask_Repeat* const ClockTask = UAbilityTask_Repeat::RepeatAction(this, knightMonsterAreaAttack::ClockInterval, knightMonsterAreaAttack::MaxClockTicks);
	ClockTask->OnPerformAction.AddDynamic(this, &ThisClass::HandleClockTick);
	ClockTask->ReadyForActivation();
}

void UknightGameplayAbility_MonsterAreaAttack::HandleClockTick(int32 ActionNumber)
{
	const AActor* const Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}
	const float Step = knightMonsterAreaAttack::ClockInterval * FMath::Max(Avatar->CustomTimeDilation, 0.f);
	Elapsed += Step;

	// 판정보다 먼저 돌려서, 이번 판정이 지금 고개 방향으로 나가게 한다.
	if (bAimSweep && Impacts.IsEmpty() && RecoveryStart < 0.f && Elapsed >= WindupTime)
	{
		UpdateAimSweep(Step);
	}

	if (!bLungeStarted && LungeMaxDistance > 0.f && LungeEndTime > LungeStartTime && Elapsed >= LungeStartTime)
	{
		bLungeStarted = true;
		StartLunge();
	}

	if (!bWindupEffectStarted && !bWindupDone && RecoveryStart < 0.f && Elapsed >= WindupEffectStartTime)
	{
		bWindupEffectStarted = true;
		StartWindupEffect();
	}

	if (RecoveryStart < 0.f)
	{
		if (!Impacts.IsEmpty())
		{
			// 판정 간격이 시계 간격보다 짧아도 빠뜨리지 않게, 지난 판정을 모두 처리한다.
			while (Impacts.IsValidIndex(NextImpact) && Elapsed >= Impacts[NextImpact].Time)
			{
				ApplyImpact(Impacts[NextImpact]);
				++NextImpact;
			}
			if (!Impacts.IsValidIndex(NextImpact))
			{
				RecoveryStart = Elapsed;
			}
		}
		else if (Elapsed >= WindupTime)
		{
			if (!bWindupDone)
			{
				bWindupDone = true;
				OnImpact();
				NextActiveCheck = WindupTime + knightMonsterAreaAttack::ActiveCheckInterval;
			}
			const float ActiveEnd = WindupTime + ActiveTime;
			while (NextActiveCheck <= FMath::Min(Elapsed, ActiveEnd))
			{
				OnImpact();
				NextActiveCheck += knightMonsterAreaAttack::ActiveCheckInterval;
			}
			if (Elapsed >= ActiveEnd)
			{
				RecoveryStart = Elapsed;
				StopAimSweep();
			}
		}
	}

	if (RecoveryStart >= 0.f && Elapsed >= RecoveryStart + RecoveryTime)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UknightGameplayAbility_MonsterAreaAttack::ApplyImpact(const FknightMonsterAreaImpact& Impact)
{
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const USkeletalMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh || !Mesh->DoesSocketExist(Impact.Socket))
	{
		UE_LOG(LogknightAI, Warning, TEXT("%s: impact socket %s not found."), *GetName(), *Impact.Socket.ToString());
		return;
	}
	const FVector Center = Mesh->GetSocketLocation(Impact.Socket);
	const UCapsuleComponent* const Capsule = Character->GetCapsuleComponent();
	const float Height = Center.Z - (Character->GetActorLocation().Z - Capsule->GetScaledCapsuleHalfHeight());
	if (Impact.MaxHeight > 0.f && Height > Impact.MaxHeight)
	{
		UE_LOG(LogknightAI, Verbose, TEXT("%s: %s impact at %s skipped, %.0f cm above ground."), *GetNameSafe(Character), *GetName(), *Impact.Socket.ToString(), Height);
		return;
	}
	HitArea(Center, Impact.Radius);
}

void UknightGameplayAbility_MonsterAreaAttack::HandleKnockbackTick(int32 ActionNumber)
{
	Knockback.Tick(knightMonsterAreaAttack::ActiveCheckInterval);
}

void UknightGameplayAbility_MonsterAreaAttack::StartLunge()
{
	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	const UknightMonsterTargetingComponent* const Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
	const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
	const UCharacterMovementComponent* const Movement = Monster ? Monster->GetCharacterMovement() : nullptr;
	if (!Target || !Movement)
	{
		return;
	}
	const FVector ToTarget = Target->GetActorLocation() - Monster->GetActorLocation();
	const float Distance = FMath::Clamp(static_cast<float>(ToTarget.Size2D()) - AreaOffset.X, 0.f, LungeMaxDistance);
	if (Distance < 1.f)
	{
		return;
	}
	// 정면이 아니라 타깃 쪽으로 던진다. 예고 동안 타깃이 옆으로 조금 움직여도 몸이 따라간다.
	const FVector Direction = ToTarget.GetSafeNormal2D();
	const float Duration = LungeEndTime - LungeStartTime;
	UAbilityTask_ApplyRootMotionConstantForce* const LungeTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("AreaLunge"), Direction, Distance / Duration, Duration, false, nullptr,
		ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, 0.f, false);
	LungeTask->ReadyForActivation();
	UE_LOG(LogknightAI, Verbose, TEXT("%s: %s lunges %.0f cm over %.2f s."), *GetNameSafe(Monster), *GetName(), Distance, Duration);
}

void UknightGameplayAbility_MonsterAreaAttack::OnImpact()
{
	KNIGHT_PERF_SCOPE("Monster.AreaAttackImpact");
	const FVector Center = GetAreaCenter();
	HitArea(Center, AreaRadius);
	if (!bHitAreaEffectsPlayed)
	{
		bHitAreaEffectsPlayed = true;
		StopWindupEffect();
		PlayHitAreaEffects(Center);
	}
}

void UknightGameplayAbility_MonsterAreaAttack::StartWindupEffect()
{
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}
	if (WindupEffect)
	{
		WindupEffectComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(WindupEffect, Mesh, WindupEffectSocket, FVector::ZeroVector, FRotator::ZeroRotator,
			FVector(WindupEffectScale), EAttachLocation::SnapToTarget, true, ENCPoolMethod::None);
	}
	if (WindupSound)
	{
		WindupSoundComponent = UGameplayStatics::SpawnSoundAttached(WindupSound, Mesh, WindupEffectSocket);
	}
}

void UknightGameplayAbility_MonsterAreaAttack::StopWindupEffect()
{
	// 새 입자만 멈추고 남은 입자는 사라지게 둔다.
	if (UNiagaraComponent* const Effect = WindupEffectComponent.Get())
	{
		Effect->Deactivate();
	}
	WindupEffectComponent.Reset();
	if (UAudioComponent* const Sound = WindupSoundComponent.Get())
	{
		Sound->FadeOut(0.1f, 0.f);
	}
	WindupSoundComponent.Reset();
}

void UknightGameplayAbility_MonsterAreaAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopWindupEffect();
	StopAimSweep();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UknightGameplayAbility_MonsterAreaAttack::PlayHitAreaEffects(const FVector& Center) const
{
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UWorld* const World = GetWorld();
	if (!Character || !World || (HitAreaEffects.IsEmpty() && !HitAreaSound))
	{
		return;
	}
	// 몬스터 발밑 높이(이동 캡슐 바닥)에 놓는다. 낮게 떠 있는 날짐승도 바닥에서 퍼져 나가게.
	const UCapsuleComponent* const Capsule = Character->GetCapsuleComponent();
	const FVector Ground(Center.X, Center.Y, Capsule ? Capsule->GetComponentLocation().Z - Capsule->GetScaledCapsuleHalfHeight() : Center.Z);
	const FRotator Facing(0.f, Character->GetActorRotation().Yaw + AimYaw, 0.f);
	for (UNiagaraSystem* const Effect : HitAreaEffects)
	{
		if (Effect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Effect, Ground, Facing, FVector(HitAreaEffectScale), true, true, ENCPoolMethod::AutoRelease);
		}
	}
	if (HitAreaSound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, HitAreaSound, Ground);
	}
}

void UknightGameplayAbility_MonsterAreaAttack::HitArea(const FVector& Center, float Radius)
{
	AActor* const Avatar = GetAvatarActorFromActorInfo();
	UWorld* const World = GetWorld();
	if (!Avatar || !World)
	{
		return;
	}

	const int32 NewHits = UknightCombatLibrary::ApplyAreaMeleeHits(Avatar, Center, FCollisionShape::MakeSphere(Radius), HitParams, HitActors,
		[this, Avatar](const AActor& Target) { return IsInsideCone(*Avatar, Target.GetActorLocation()); },
		[this, Avatar](AActor& Target, EknightHitOutcome Outcome)
		{
			// 가드로 막혔으면 가드 밀림(GuardReaction)과 이 공격의 가드 넉백 중 더 먼 쪽 하나만 쓴다.
			const bool bGuardPushed = HandleGuardPush(Target, Outcome, KnockbackSpeed * GuardKnockbackScale * KnockbackTime);
			if (KnockbackSpeed > 0.f && !bGuardPushed && (Outcome == EknightHitOutcome::Hit || Outcome == EknightHitOutcome::Guarded))
			{
				const float Speed = KnockbackSpeed * (Outcome == EknightHitOutcome::Guarded ? GuardKnockbackScale : 1.f);
				if (bKnockbackAway)
				{
					Knockback.StartAway(*Avatar, Target, Speed, KnockbackTime);
				}
				else
				{
					Knockback.Start(*Avatar, Target, Speed, KnockbackTime);
				}
				const int32 Ticks = FMath::CeilToInt(KnockbackTime / knightMonsterAreaAttack::ActiveCheckInterval);
				UAbilityTask_Repeat* const KnockbackTask = UAbilityTask_Repeat::RepeatAction(this, knightMonsterAreaAttack::ActiveCheckInterval, Ticks);
				KnockbackTask->OnPerformAction.AddDynamic(this, &ThisClass::HandleKnockbackTick);
				KnockbackTask->ReadyForActivation();
			}
			if (Outcome == EknightHitOutcome::Hit && HitDeafenTime > 0.f)
			{
				const APawn* const Pawn = Cast<APawn>(&Target);
				const APlayerController* const PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
				if (AknightPlayerCameraManager* const Camera = PC && PC->IsLocalController() ? Cast<AknightPlayerCameraManager>(PC->PlayerCameraManager) : nullptr)
				{
					Camera->StartDeafen(HitDeafenTime);
				}
			}
			if (Outcome != EknightHitOutcome::Hit || !HitTargetEffect)
			{
				return;
			}
			UAbilitySystemComponent* const SourceASC = GetAbilitySystemComponentFromActorInfo();
			UAbilitySystemComponent* const TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Target);
			if (SourceASC && TargetASC)
			{
				const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(HitTargetEffect, GetAbilityLevel());
				SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
			}
		});
	CheckNearMiss(Center, Radius, HitActors);

#if ENABLE_DRAW_DEBUG
	if (knightMonsterDebug::ShouldDrawAttacks())
	{
		DrawDebugSphere(World, Center, Radius, 16, NewHits == 0 ? FColor::Red : FColor::Magenta, false,
			ActiveTime > 0.f ? knightMonsterAreaAttack::ActiveCheckInterval : 0.5f, 0, 3.f);
		if (ConeHalfAngle > 0.f)
		{
			const FTransform Aim = GetAimTransform(*Avatar);
			const FVector Origin = Aim.TransformPosition(ConeOrigin);
			const float Length = FVector::Dist(Origin, Center) + Radius;
			DrawDebugCone(World, Origin, Aim.GetUnitAxis(EAxis::X), Length, FMath::DegreesToRadians(ConeHalfAngle),
				FMath::DegreesToRadians(5.f), 12, FColor::Yellow, false, ActiveTime > 0.f ? knightMonsterAreaAttack::ActiveCheckInterval : 0.5f);
		}
	}
#endif
	UE_LOG(LogknightAI, Verbose, TEXT("%s: %s hit %d target(s)."), *GetNameSafe(Avatar), *GetName(), NewHits);
}

bool UknightGameplayAbility_MonsterAreaAttack::IsInsideCone(const AActor& Avatar, const FVector& Point) const
{
	if (ConeHalfAngle <= 0.f)
	{
		return true;
	}
	// 높이는 보지 않는다. 브레스가 아래로 기울어도 같은 좌우 폭으로 판정한다.
	const FTransform Aim = GetAimTransform(Avatar);
	const FVector Origin = Aim.TransformPosition(ConeOrigin);
	const FVector ToPoint = (Point - Origin).GetSafeNormal2D();
	const FVector Forward = Aim.GetUnitAxis(EAxis::X).GetSafeNormal2D();
	return !ToPoint.IsNearlyZero() && FVector::DotProduct(ToPoint, Forward) >= FMath::Cos(FMath::DegreesToRadians(ConeHalfAngle));
}

FVector UknightGameplayAbility_MonsterAreaAttack::GetAreaCenter() const
{
	const AActor* const Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return FVector::ZeroVector;
	}
	const ACharacter* const Character = Cast<ACharacter>(Avatar);
	const USkeletalMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
	if (!AreaSocket.IsNone() && Mesh && Mesh->DoesSocketExist(AreaSocket))
	{
		return Mesh->GetSocketLocation(AreaSocket) + GetAimTransform(*Avatar).GetRotation().RotateVector(AreaOffset);
	}
	return GetAimTransform(*Avatar).TransformPosition(AreaOffset);
}

FTransform UknightGameplayAbility_MonsterAreaAttack::GetAimTransform(const AActor& Avatar) const
{
	FTransform Transform = Avatar.GetActorTransform();
	if (!FMath::IsNearlyZero(AimYaw))
	{
		Transform.SetRotation(Transform.GetRotation() * FQuat(FVector::UpVector, FMath::DegreesToRadians(AimYaw)));
	}
	return Transform;
}

void UknightGameplayAbility_MonsterAreaAttack::UpdateAimSweep(float DeltaTime)
{
	AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	const UknightMonsterTargetingComponent* const Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
	const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
	if (!Monster)
	{
		return;
	}
	bAimSweeping = true;
	if (Target)
	{
		// 입(원뿔 꼭짓점)에서 본 타깃 방향. 몸 정면 기준 좌우 각도로 바꿔 한계 안에서 천천히 따라간다.
		const FVector Mouth = Monster->GetActorTransform().TransformPosition(ConeOrigin);
		const FVector ToTarget = Monster->GetActorQuat().UnrotateVector(Target->GetActorLocation() - Mouth);
		if (!ToTarget.IsNearlyZero())
		{
			const float Desired = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X)), -AimSweepMaxYaw, AimSweepMaxYaw);
			AimYaw = FMath::FixedTurn(AimYaw, Desired, AimSweepRate * DeltaTime);
		}
	}
	Monster->SetHeadAimYaw(AimYaw);
	if (AimSweepGroundFX && FMath::Abs(AimYaw - LastGroundFXYaw) >= AimSweepGroundFXStep)
	{
		LastGroundFXYaw = AimYaw;
		SpawnAimGroundFX();
	}
}

void UknightGameplayAbility_MonsterAreaAttack::StopAimSweep()
{
	if (!bAimSweeping)
	{
		return;
	}
	bAimSweeping = false;
	if (AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo()))
	{
		Monster->ClearHeadAimYaw();
	}
}

void UknightGameplayAbility_MonsterAreaAttack::SpawnAimGroundFX() const
{
	const AActor* const Avatar = GetAvatarActorFromActorInfo();
	UWorld* const World = GetWorld();
	if (!Avatar || !World)
	{
		return;
	}
	// 몸이 아니라 고개 방향 앞 바닥. 바닥을 못 찾으면(낭떠러지) 남기지 않는다.
	const FRotator Facing(0.f, Avatar->GetActorRotation().Yaw + AimYaw, 0.f);
	const FVector Source = Avatar->GetActorLocation() + Facing.Vector() * AimSweepGroundFXDistance;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(knightAimGroundFX), false, Avatar);
	if (World->LineTraceSingleByObjectType(Hit, Source + FVector(0.f, 0.f, 3000.f), Source - FVector(0.f, 0.f, 3000.f),
		FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, AimSweepGroundFX, Hit.ImpactPoint, Facing, FVector(1.f), true, true, ENCPoolMethod::AutoRelease);
	}
}

#if WITH_EDITOR
EDataValidationResult UknightGameplayAbility_MonsterAreaAttack::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	for (int32 Index = 0; Index < Impacts.Num(); ++Index)
	{
		if (Impacts[Index].Socket.IsNone())
		{
			Context.AddError(FText::Format(LOCTEXT("EmptyImpactSocket", "Impacts[{0}] has no socket."), Index));
			Result = EDataValidationResult::Invalid;
		}
		if (Index > 0 && Impacts[Index].Time < Impacts[Index - 1].Time)
		{
			Context.AddError(FText::Format(LOCTEXT("ImpactOrder", "Impacts[{0}] comes before Impacts[{1}]. Keep impacts in time order."), Index, Index - 1));
			Result = EDataValidationResult::Invalid;
		}
	}
	if (LungeMaxDistance > 0.f && LungeEndTime <= LungeStartTime)
	{
		Context.AddError(LOCTEXT("LungeTime", "LungeEndTime must be after LungeStartTime."));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
