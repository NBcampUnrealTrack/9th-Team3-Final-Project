#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterGlideAttack.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Animation/AnimInstance.h"
#include "Camera/knightPlayerCameraManager.h"
#include "Combat/knightCombatLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/RootMotionSource.h"
#include "Kismet/GameplayStatics.h"
#include "Monster/knightMonsterAirTurnComponent.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterDebug.h"
#include "Monster/knightMonsterHoverComponent.h"
#include "Monster/knightMonsterMovementComponent.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

namespace knightMonsterGlide
{
	constexpr float TickInterval = 1.f / 60.f;
	constexpr int32 MaxTicks = 100000;
}

UknightGameplayAbility_MonsterGlideAttack::UknightGameplayAbility_MonsterGlideAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Selection.MinRange = 1000.f;
	Selection.MaxRange = 2500.f;
	Selection.MaxAngle = 180.f;
}

void UknightGameplayAbility_MonsterGlideAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!StrikeMontage)
	{
		UE_LOG(LogknightAbility, Error, TEXT("%s has no StrikeMontage."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	StrikesLeft = FMath::Max(1, ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Enraged) ? EnragedStrikeCount : StrikeCount);
	Phase = EPhase::Roar;
	PhaseTime = 0.f;
	bLanded = false;
	bCircling = false;
	SetDarkness(bDarkenView);
	if (RoarMontage)
	{
		UAbilityTask_PlayMontageAndWait* const RoarTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, RoarMontage);
		RoarTask->ReadyForActivation();
	}
	UAbilityTask_Repeat* const Task = UAbilityTask_Repeat::RepeatAction(this, knightMonsterGlide::TickInterval, knightMonsterGlide::MaxTicks);
	Task->OnPerformAction.AddDynamic(this, &ThisClass::HandleTick);
	TickTask = Task;
	Task->ReadyForActivation();
}

void UknightGameplayAbility_MonsterGlideAttack::HandleTick(int32 ActionNumber)
{
	PhaseTime += knightMonsterGlide::TickInterval;
	AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	if (!Monster)
	{
		return;
	}
	const UknightMonsterTargetingComponent* const Targeting = Monster->GetTargetingComponent();
	const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
	UknightMonsterMovementComponent* const Movement = Monster->GetMonsterMovementComponent();

	switch (Phase)
	{
	case EPhase::Roar:
	case EPhase::Turn:
		if (Movement && Target)
		{
			Movement->TurnTowards(Target->GetActorLocation(), knightMonsterGlide::TickInterval, TurnRate, 2.f);
		}
		if (Phase == EPhase::Roar && Target && bDarkenView && BehindDistance > 0.f && PhaseTime >= CircleStartTime)
		{
			UpdateCircle(*Monster, *Target);
		}
		if (PhaseTime >= (Phase == EPhase::Roar ? (bDarkenView ? FMath::Max(RoarTime, DarknessHoldTime) : RoarTime) : TurnTime))
		{
			StartStrike();
		}
		break;
	case EPhase::Strike:
		SetSilhouette(SilhouetteLeadTime > 0.f && PhaseTime >= StabTime - SilhouetteLeadTime && PhaseTime < StabTime);
		if (Target && bDarkenView && HomingTurnRate > 0.f && PhaseTime < StabTime - HomingLockTime)
		{
			// 찌르기 직전까지 타깃 쪽으로 꺾는다. 속도는 시작할 때 정한 그대로다.
			const FVector ToTarget = (Target->GetActorLocation() - Monster->GetActorLocation()).GetSafeNormal2D(UE_SMALL_NUMBER, GlideDirection);
			const FRotator Heading = FMath::RInterpConstantTo(GlideDirection.Rotation(), ToTarget.Rotation(), knightMonsterGlide::TickInterval, HomingTurnRate);
			GlideDirection = Heading.Vector();
			Monster->SetActorRotation(Heading);
			SetGlideVelocity(GlideDirection * GlideSpeed);
		}
		if (PhaseTime >= HitStartTime && PhaseTime <= HitEndTime)
		{
			ApplyHits();
		}
		if (PhaseTime >= StrikeEndTime)
		{
			SetNoFlinch(false);
			if (--StrikesLeft > 0)
			{
				Phase = EPhase::Turn;
				PhaseTime = 0.f;
			}
			else
			{
				StartLand();
			}
		}
		break;
	case EPhase::Land:
		if (PhaseTime >= LandHoldTime)
		{
			Phase = EPhase::GetUp;
			SetGettingUp(true);
			if (!GetUpMontage)
			{
				HandleGetUpFinished();
				return;
			}
			UAbilityTask_PlayMontageAndWait* const GetUpTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, GetUpMontage);
			GetUpTask->OnCompleted.AddDynamic(this, &ThisClass::HandleGetUpFinished);
			GetUpTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleGetUpFinished);
			GetUpTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleGetUpFinished);
			GetUpTask->OnCancelled.AddDynamic(this, &ThisClass::HandleGetUpFinished);
			GetUpTask->ReadyForActivation();
		}
		break;
	case EPhase::GetUp:
		break;
	}
}

void UknightGameplayAbility_MonsterGlideAttack::StartStrike()
{
	AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	const UknightMonsterTargetingComponent* const Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
	const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
	Phase = EPhase::Strike;
	HitActors.Reset();
	SetNoFlinch(true);

	// 시작하는 순간의 타깃 위치로 고정한다. 꼬리 끝이 StabTime에 그곳에 오는 속도로 날고, 지나간 뒤에도 같은 속도를 유지한다.
	// 어둠 속에서는 속도만 고정하고 방향은 HandleTick이 찌르기 직전까지 꺾는다.
	bCircling = false;
	FVector Direction = Monster->GetActorForwardVector().GetSafeNormal2D();
	float Travel = 0.f;
	if (Target)
	{
		const FVector ToTarget = Target->GetActorLocation() - Monster->GetActorLocation();
		Direction = ToTarget.GetSafeNormal2D(UE_SMALL_NUMBER, Direction);
		Travel = FMath::Max(0.f, static_cast<float>(ToTarget.Size2D()) - TailReach);
	}
	const float Speed = FMath::Max(Travel / StabTime, MinGlideSpeed);
	// 최소 속도로 날면 일찍 도착하므로 그만큼 몽타주 앞부분을 건너뛴다. 판정 시각은 몽타주 시간 그대로다.
	const float StartPosition = FMath::Clamp(StabTime - Travel / Speed, 0.f, StabTime);
	PhaseTime = StartPosition;
	Monster->SetActorRotation(Direction.Rotation());
	UAbilityTask_PlayMontageAndWait* const StrikeTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, StrikeMontage,
		1.f, NAME_None, true, 1.f, StartPosition);
	StrikeTask->ReadyForActivation();
	// 마지막이 아니면 타깃 너머 RepassDistance까지 지나간다(찌르는 순간 몸은 타깃 TailReach 앞).
	const float AfterStab = StrikesLeft > 1 ? (TailReach + RepassDistance) / Speed : PassTime;
	StrikeEndTime = StabTime + AfterStab;
	GlideDirection = Direction;
	GlideSpeed = Speed;
	StartGlideForce(Direction * Speed, StrikeEndTime - StartPosition);
	// 꼬리가 바닥 가까이의 타깃에 닿도록 찌르는 순간까지 낮게 내려온다.
	if (UknightMonsterHoverComponent* const Hover = Monster->FindComponentByClass<UknightMonsterHoverComponent>())
	{
		Hover->SetAirborne(false, FMath::Max(0.1f, StabTime - StartPosition));
	}
	UE_LOG(LogknightAI, Log, TEXT("%s: glide strike at %.0f cm/s from %.2f s, %d left."), *GetNameSafe(Monster), Speed, StartPosition, StrikesLeft - 1);
}

void UknightGameplayAbility_MonsterGlideAttack::StartLand()
{
	AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	if (UknightMonsterAirTurnComponent* const AirTurn = Monster ? Monster->FindComponentByClass<UknightMonsterAirTurnComponent>() : nullptr)
	{
		AirTurn->EndAirTurnNow(0.3f);
	}
	bLanded = true;
	SetDarkness(false);
	SetCollapsed(true);
	Phase = EPhase::Land;
	PhaseTime = 0.f;
	if (!LandMontage)
	{
		HandleGetUpFinished();
		return;
	}
	UAbilityTask_PlayMontageAndWait* const LandTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, LandMontage);
	LandTask->ReadyForActivation();
	UE_LOG(LogknightAI, Log, TEXT("%s: glide ends, exhausted landing."), *GetNameSafe(Monster));
}

void UknightGameplayAbility_MonsterGlideAttack::HandleGetUpFinished()
{
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UknightGameplayAbility_MonsterGlideAttack::ApplyHits()
{
	AActor* const Avatar = GetAvatarActorFromActorInfo();
	const USkeletalMeshComponent* const Mesh = GetCurrentActorInfo() ? GetCurrentActorInfo()->SkeletalMeshComponent.Get() : nullptr;
	if (!Avatar)
	{
		return;
	}
	const FVector Center = Mesh && Mesh->DoesSocketExist(HitSocket) ? Mesh->GetSocketLocation(HitSocket) : Avatar->GetActorLocation();
	const int32 NewHits = UknightCombatLibrary::ApplyAreaMeleeHits(Avatar, Center, FCollisionShape::MakeSphere(HitRadius), HitParams, HitActors);
#if ENABLE_DRAW_DEBUG
	if (knightMonsterDebug::ShouldDrawAttacks())
	{
		DrawDebugSphere(GetWorld(), Center, HitRadius, 16, NewHits == 0 ? FColor::Red : FColor::Magenta, false, knightMonsterGlide::TickInterval, 0, 3.f);
	}
#endif
}

void UknightGameplayAbility_MonsterGlideAttack::SetNoFlinch(bool bOn)
{
	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || bOn == bNoFlinchApplied)
	{
		return;
	}
	if (bOn)
	{
		ASC->AddLooseGameplayTag(knightGameplayTags::State_Monster_NoFlinch);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(knightGameplayTags::State_Monster_NoFlinch);
	}
	bNoFlinchApplied = bOn;
}

void UknightGameplayAbility_MonsterGlideAttack::SetGettingUp(bool bOn)
{
	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || bOn == bGettingUpApplied)
	{
		return;
	}
	if (bOn)
	{
		ASC->AddLooseGameplayTag(knightGameplayTags::State_Monster_GettingUp);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(knightGameplayTags::State_Monster_GettingUp);
	}
	bGettingUpApplied = bOn;
}

void UknightGameplayAbility_MonsterGlideAttack::SetCollapsed(bool bOn)
{
	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || bOn == bCollapsedApplied)
	{
		return;
	}
	if (bOn)
	{
		ASC->AddLooseGameplayTag(knightGameplayTags::State_Monster_Collapsed);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(knightGameplayTags::State_Monster_Collapsed);
	}
	bCollapsedApplied = bOn;
}

void UknightGameplayAbility_MonsterGlideAttack::UpdateCircle(AknightMonsterCharacterBase& Monster, const AActor& Target)
{
	const float Remaining = FMath::Max(RoarTime, DarknessHoldTime) - PhaseTime;
	if (Remaining <= 0.f)
	{
		return;
	}
	// 타깃 둘레를 각도·반지름을 함께 좁혀 가며 돌아, 남은 시간 안에 등 뒤 지점에 닿는다. 타깃과 카메라가 움직이면 매번 다시 잡는다.
	const FVector Center = Target.GetActorLocation();
	const FVector From = Monster.GetActorLocation() - Center;
	const FVector To = GetBehindLocation(Monster, Target) - Center;
	const float Step = FMath::Min(1.f, knightMonsterGlide::TickInterval / Remaining);
	const float FromAngle = FMath::Atan2(From.Y, From.X);
	const float Angle = FromAngle + FMath::FindDeltaAngleRadians(FromAngle, FMath::Atan2(To.Y, To.X)) * Step;
	const float Radius = FMath::Lerp(static_cast<float>(From.Size2D()), static_cast<float>(To.Size2D()), Step);
	const FVector Next = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;
	FVector Velocity = (Next - Monster.GetActorLocation()) / knightMonsterGlide::TickInterval;
	Velocity.Z = 0.f;
	Velocity = Velocity.GetClampedToMaxSize(CircleMaxSpeed);
	if (!bCircling)
	{
		bCircling = true;
		CircleFlapTime = 0.f;
		StartGlideForce(Velocity, Remaining + 0.5f);
		UE_LOG(LogknightAI, Log, TEXT("%s: glide circles behind the target in the dark (%.2f s)."), *Monster.GetName(), Remaining);
	}
	else
	{
		SetGlideVelocity(Velocity);
	}
	CircleFlapTime -= knightMonsterGlide::TickInterval;
	if (CircleFlapSound && CircleFlapTime <= 0.f)
	{
		CircleFlapTime = CircleFlapInterval;
		UGameplayStatics::SpawnSoundAttached(CircleFlapSound, Monster.GetMesh());
	}
#if ENABLE_DRAW_DEBUG
	if (knightMonsterDebug::ShouldDrawAttacks())
	{
		DrawDebugSphere(GetWorld(), Center + To, 100.f, 12, FColor::Yellow, false, knightMonsterGlide::TickInterval, 0, 3.f);
	}
#endif
}

FVector UknightGameplayAbility_MonsterGlideAttack::GetBehindLocation(const AknightMonsterCharacterBase& Monster, const AActor& Target) const
{
	// 등 뒤 = 카메라가 보는 방향의 반대. 조종하는 컨트롤러가 없으면 타깃이 보는 방향의 반대.
	const APawn* const Pawn = Cast<APawn>(&Target);
	const FVector View = Pawn && Pawn->GetController() ? Pawn->GetControlRotation().Vector() : Target.GetActorForwardVector();
	const FVector Back = -View.GetSafeNormal2D(UE_SMALL_NUMBER, Target.GetActorForwardVector());
	FVector Start = Target.GetActorLocation();
	Start.Z = Monster.GetActorLocation().Z;
	FVector Goal = Start + Back * BehindDistance;
	// 벽 너머로 가지 않게 벽 앞(몸 반지름만큼)에서 멈춘다.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GlideBehind), false, &Monster);
	Params.AddIgnoredActor(&Target);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Goal, ECC_WorldStatic, Params))
	{
		Goal = Hit.Location - Back * Monster.GetSimpleCollisionRadius();
	}
	return Goal;
}

void UknightGameplayAbility_MonsterGlideAttack::StartGlideForce(const FVector& Velocity, float Duration)
{
	StopGlideForce();
	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	UCharacterMovementComponent* const Movement = Monster ? Monster->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return;
	}
	// 방향을 매 틱 바꿔야 해서 루트 모션 태스크 대신 소스를 직접 들고 Force를 고친다.
	const TSharedPtr<FRootMotionSource_ConstantForce> Force = MakeShared<FRootMotionSource_ConstantForce>();
	Force->InstanceName = TEXT("Glide");
	Force->AccumulateMode = ERootMotionAccumulateMode::Override;
	Force->Priority = 5;
	Force->Force = Velocity;
	Force->Duration = Duration;
	Force->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	Force->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
	GlideForceID = Movement->ApplyRootMotionSource(Force);
}

void UknightGameplayAbility_MonsterGlideAttack::SetGlideVelocity(const FVector& Velocity)
{
	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	UCharacterMovementComponent* const Movement = Monster ? Monster->GetCharacterMovement() : nullptr;
	const TSharedPtr<FRootMotionSource> Source = Movement && GlideForceID != 0 ? Movement->GetRootMotionSourceByID(GlideForceID) : nullptr;
	if (Source.IsValid())
	{
		static_cast<FRootMotionSource_ConstantForce*>(Source.Get())->Force = Velocity;
	}
}

void UknightGameplayAbility_MonsterGlideAttack::StopGlideForce()
{
	if (GlideForceID == 0)
	{
		return;
	}
	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	if (UCharacterMovementComponent* const Movement = Monster ? Monster->GetCharacterMovement() : nullptr)
	{
		if (Movement->GetRootMotionSourceByID(GlideForceID).IsValid())
		{
			Movement->RemoveRootMotionSourceByID(GlideForceID);
			Movement->Velocity = FVector::ZeroVector;
		}
	}
	GlideForceID = 0;
}

void UknightGameplayAbility_MonsterGlideAttack::SetDarkness(bool bOn)
{
	if (bOn == bDarknessOn)
	{
		return;
	}
	if (!bOn)
	{
		SetSilhouette(false);
	}
	UPrimitiveComponent* const Mesh = GetCurrentActorInfo() ? GetCurrentActorInfo()->SkeletalMeshComponent.Get() : nullptr;
	AknightPlayerCameraManager::ForEachLocalCamera(GetAvatarActorFromActorInfo(), [Mesh, bOn](AknightPlayerCameraManager& Camera)
	{
		if (bOn)
		{
			Camera.StartDarkness(Mesh);
		}
		else
		{
			Camera.StopDarkness(Mesh);
		}
	});
	bDarknessOn = bOn;
}

void UknightGameplayAbility_MonsterGlideAttack::SetSilhouette(bool bOn)
{
	bOn &= bDarknessOn;
	if (bOn == bSilhouetteOn)
	{
		return;
	}
	const UPrimitiveComponent* const Mesh = GetCurrentActorInfo() ? GetCurrentActorInfo()->SkeletalMeshComponent.Get() : nullptr;
	AknightPlayerCameraManager::ForEachLocalCamera(GetAvatarActorFromActorInfo(), [Mesh, bOn](AknightPlayerCameraManager& Camera)
	{
		Camera.SetDarknessSilhouette(Mesh, bOn);
	});
	bSilhouetteOn = bOn;
}

void UknightGameplayAbility_MonsterGlideAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (TickTask.IsValid())
	{
		TickTask->EndTask();
	}
	TickTask.Reset();
	StopGlideForce();
	SetNoFlinch(false);
	SetDarkness(false);
	SetCollapsed(false);
	SetGettingUp(false);

	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	UknightMonsterAirTurnComponent* const AirTurn = Monster ? Monster->FindComponentByClass<UknightMonsterAirTurnComponent>() : nullptr;
	if (AirTurn && bLanded)
	{
		// 탈진 착지(딜 타임)가 끝난 뒤부터 지상 턴을 센다.
		AirTurn->RestartGroundTurn();
	}
	else if (AirTurn && AirTurn->IsAirborne() && !Monster->IsDead())
	{
		// 착지 전에 끊겼다(격추가 아닌 다른 반응). 공중 높이로 돌아간다.
		const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
		UknightMonsterHoverComponent* const Hover = Monster->FindComponentByClass<UknightMonsterHoverComponent>();
		if (Hover && !(ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_HitReact_Knockdown)))
		{
			Hover->SetAirborne(true, 1.f);
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
