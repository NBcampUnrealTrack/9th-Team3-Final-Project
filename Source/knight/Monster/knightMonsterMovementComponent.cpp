#include "Monster/knightMonsterMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"
#include "System/knightGameplayTags.h"

UknightMonsterMovementComponent::UknightMonsterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MaxWalkSpeed = 700.f;
	MaxAcceleration = 800.f;
	BrakingDecelerationWalking = 800.f;
	RotationRate = FRotator(0.f, 90.f, 0.f);
	bOrientRotationToMovement = true;
	bUseControllerDesiredRotation = false;
	GravityScale = 1.5f;

	// 경로 추종 시 곧바로 최고 속도를 내지 않고 무거운 몸체처럼 가속한다.
	bRequestedMoveUseAcceleration = true;
	bUseRVOAvoidance = false;

	LockTags.AddTag(knightGameplayTags::State_Monster_Reacting);
	LockTags.AddTag(knightGameplayTags::State_Dead);
}

bool UknightMonsterMovementComponent::IsMovementLocked() const
{
	if (!CachedASC.IsValid())
	{
		CachedASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	}
	const UAbilitySystemComponent* const ASC = CachedASC.Get();
	return ASC && ASC->HasAnyMatchingGameplayTags(LockTags);
}

float UknightMonsterMovementComponent::GetMaxSpeed() const
{
	return IsMovementLocked() ? 0.f : Super::GetMaxSpeed();
}

void UknightMonsterMovementComponent::PhysicsRotation(float DeltaTime)
{
	// 이렇게 하지 않으면 몬스터가 휘청이는 동안 Orient-to-movement가 마지막 경로 쪽으로 계속 회전시킨다.
	if (!IsMovementLocked())
	{
		Super::PhysicsRotation(DeltaTime);
	}
}

void UknightMonsterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (FMath::IsNearlyZero(TurnSpeed))
	{
		return;
	}
	// 회전이 끝났거나(허용 각도 도달) 회전을 요청하던 쪽이 멈췄으면(최대 시간 등) 남은 속도를 관성으로 줄인다.
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (!bCoastingTurn && Now - LastTurnTime < 0.1)
	{
		return;
	}
	AActor* const Owner = GetOwner();
	if (!Owner || IsMovementLocked() || TurnAcceleration <= 0.f)
	{
		TurnSpeed = 0.f;
		return;
	}
	const float NewSpeed = FMath::FInterpConstantTo(TurnSpeed, 0.f, DeltaTime, TurnAcceleration);
	const float Step = 0.5f * (TurnSpeed + NewSpeed) * DeltaTime;
	TurnSpeed = NewSpeed;
	Owner->SetActorRotation(FRotator(0.f, Owner->GetActorRotation().Yaw + Step, 0.f));
}

bool UknightMonsterMovementComponent::TurnTowards(const FVector& WorldLocation, float DeltaTime, float TurnRate, float ToleranceDegrees)
{
	AActor* const Owner = GetOwner();
	if (!Owner)
	{
		return true;
	}
	if (IsMovementLocked())
	{
		return false;
	}
	const FVector ToTarget = (WorldLocation - Owner->GetActorLocation()).GetSafeNormal2D();
	if (ToTarget.IsNearlyZero())
	{
		return true;
	}
	const float CurrentYaw = Owner->GetActorRotation().Yaw;
	const float DesiredYaw = ToTarget.Rotation().Yaw;
	const float Delta = FRotator::NormalizeAxis(DesiredYaw - CurrentYaw);

	LastTurnTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (FMath::Abs(Delta) <= ToleranceDegrees)
	{
		// 남은 속도는 TickComponent가 줄인다. 감속 곡선(v = sqrt(2 a d)) 덕분에 관성으로 돈 만큼이 남은 각도와 거의 같다.
		bCoastingTurn = TurnAcceleration > 0.f;
		if (!bCoastingTurn)
		{
			TurnSpeed = 0.f;
		}
		return true;
	}
	bCoastingTurn = false;

	float Step;
	if (TurnAcceleration > 0.f)
	{
		// 남은 각도 안에 멈출 수 있는 속도(v = sqrt(2 a d))를 넘지 않게 해서, 목표 근처에서 저절로 감속한다.
		const float StopSpeed = FMath::Sqrt(2.f * TurnAcceleration * FMath::Abs(Delta));
		const float DesiredSpeed = FMath::Sign(Delta) * FMath::Min(TurnRate, StopSpeed);
		TurnSpeed = FMath::FInterpConstantTo(TurnSpeed, DesiredSpeed, DeltaTime, TurnAcceleration);
		Step = TurnSpeed * DeltaTime;
		if (FMath::Sign(Step) != FMath::Sign(Delta))
		{
			// 목표가 반대편으로 넘어갔다. 감속이 끝날 때까지 더 돌지 않는다.
			Step = 0.f;
		}
		else if (FMath::Abs(Step) > FMath::Abs(Delta))
		{
			Step = Delta;
		}
	}
	else
	{
		Step = FMath::Clamp(Delta, -TurnRate * DeltaTime, TurnRate * DeltaTime);
	}
	Owner->SetActorRotation(FRotator(0.f, CurrentYaw + Step, 0.f));
	const bool bDone = FMath::Abs(Delta - Step) <= ToleranceDegrees;
	bCoastingTurn = bDone && TurnAcceleration > 0.f;
	if (bDone && !bCoastingTurn)
	{
		TurnSpeed = 0.f;
	}
	return bDone;
}
