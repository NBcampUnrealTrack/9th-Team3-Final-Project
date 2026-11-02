#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterStrafe.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/RootMotionSource.h"
#include "knight.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterMovementComponent.h"
#include "Monster/knightMonsterTargetingComponent.h"

namespace knightMonsterStrafe
{
	constexpr float TickInterval = 1.f / 60.f;
	constexpr int32 MaxTicks = 100000;
}

void UknightGameplayAbility_MonsterStrafe::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bLeft = ChooseLeft();
	ActiveMontage = bLeft ? LeftMontage : RightMontage;
	if (!ActiveMontage || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	bStepStarted = false;
	Elapsed = 0.f;

	UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ActiveMontage);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->ReadyForActivation();

	// 횟수가 아니라 Elapsed(몬스터 시간)로 끝낸다. 히트스톱 동안은 Elapsed가 거의 늘지 않는다.
	UAbilityTask_Repeat* const TickTask = UAbilityTask_Repeat::RepeatAction(this, knightMonsterStrafe::TickInterval, knightMonsterStrafe::MaxTicks);
	TickTask->OnPerformAction.AddDynamic(this, &ThisClass::HandleTick);
	TickTask->ReadyForActivation();
}

bool UknightGameplayAbility_MonsterStrafe::ChooseLeft() const
{
	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	const bool bRandomLeft = FMath::RandBool();
	UWorld* const World = GetWorld();
	if (!Monster || !World || WallCheckDistance <= 0.f)
	{
		return bRandomLeft;
	}
	// 몬스터 몸이 커서 캡슐 높이 중간에서 옆으로 쏴 본다. 맞으면(벽) 반대쪽으로.
	const FVector Start = Monster->GetActorLocation();
	const FVector Right = Monster->GetActorRightVector().GetSafeNormal2D();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MonsterStrafeWall), false, Monster);
	const auto Blocked = [&](const FVector& Dir)
	{
		FHitResult Hit;
		return World->LineTraceSingleByChannel(Hit, Start, Start + Dir * WallCheckDistance, ECC_WorldStatic, Params);
	};
	const bool bFirstLeft = bRandomLeft;
	const FVector FirstDir = bFirstLeft ? -Right : Right;
	if (Blocked(FirstDir) && !Blocked(-FirstDir))
	{
		return !bFirstLeft;
	}
	return bFirstLeft;
}

void UknightGameplayAbility_MonsterStrafe::HandleTick(int32 ActionNumber)
{
	AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	UknightMonsterMovementComponent* const Movement = Monster ? Monster->GetMonsterMovementComponent() : nullptr;
	if (!Monster || !Movement)
	{
		return;
	}
	const float DeltaTime = GetMonsterTimeStep(knightMonsterStrafe::TickInterval);
	Elapsed += DeltaTime;
	const UknightMonsterTargetingComponent* const Targeting = Monster->GetTargetingComponent();
	const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
	if (Target && !Movement->IsMovementLocked())
	{
		// TurnTowards는 허용 각도에 들어오면 멈췄다가 벗어나면 다시 돌기 시작해서 옆걸음 내내 몸이 떨린다. 남은 각도에 비례해 계속 돈다.
		const FVector ToTarget = (Target->GetActorLocation() - Monster->GetActorLocation()).GetSafeNormal2D();
		if (!ToTarget.IsNearlyZero())
		{
			const float Yaw = Monster->GetActorRotation().Yaw;
			const float Delta = FRotator::NormalizeAxis(ToTarget.Rotation().Yaw - Yaw);
			const float MaxStep = FaceTurnRate * DeltaTime;
			const float Step = FMath::Clamp(Delta * FaceTurnGain * DeltaTime, -MaxStep, MaxStep);
			Monster->SetActorRotation(FRotator(0.f, Yaw + Step, 0.f));
		}
	}

	// 몽타주에 루트 모션이 있으면 몽타주가 옮긴다.
	if (!bStepStarted && Elapsed >= StrafeStartTime && ActiveMontage && !ActiveMontage->HasRootMotion())
	{
		bStepStarted = true;
		const float Duration = FMath::Max(0.1f, StrafeTime - StrafeStartTime);
		const FVector Side = Monster->GetActorRightVector().GetSafeNormal2D() * (bLeft ? -1.f : 1.f);
		FVector Direction = Side;
		if (Target)
		{
			// 접선으로 곧장 가면 타깃에서 멀어진다. 걷는 호의 각도 절반만큼 안쪽으로 꺾은 현 방향으로 가서 거리를 유지한다.
			const FVector ToTarget = Target->GetActorLocation() - Monster->GetActorLocation();
			const float Radius = FMath::Max(100.f, static_cast<float>(ToTarget.Size2D()));
			const float HalfArc = 0.5f * StrafeSpeed * Duration / Radius;
			// Up x 타깃 방향 = 몬스터 오른쪽.
			const FVector Tangent = FVector::CrossProduct(FVector::UpVector, ToTarget.GetSafeNormal2D()) * (bLeft ? -1.f : 1.f);
			Direction = (Tangent * FMath::Cos(HalfArc) + ToTarget.GetSafeNormal2D() * FMath::Sin(HalfArc)).GetSafeNormal2D();
		}
		UAbilityTask_ApplyRootMotionConstantForce* const StepTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
			this, TEXT("Strafe"), Direction, StrafeSpeed, Duration, false, nullptr,
			ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, 0.f, true);
		StepTask->ReadyForActivation();
		UE_LOG(LogknightAI, Verbose, TEXT("%s: %s strafes %s for %.1f s."), *GetNameSafe(Monster), *GetName(), bLeft ? TEXT("left") : TEXT("right"), Duration);
	}

	if (Elapsed >= StrafeTime)
	{
		USkeletalMeshComponent* const Mesh = Monster->GetMesh();
		UAnimInstance* const AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
		if (AnimInstance && ActiveMontage && AnimInstance->Montage_IsPlaying(ActiveMontage))
		{
			// 블렌드 아웃이 시작되면 몽타주 태스크가 HandleMontageEnded로 어빌리티를 끝낸다.
			AnimInstance->Montage_Stop(StopBlendTime, ActiveMontage);
		}
		else
		{
			HandleMontageEnded();
		}
	}
}

void UknightGameplayAbility_MonsterStrafe::HandleMontageEnded()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UknightGameplayAbility_MonsterStrafe::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	ActiveMontage = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
