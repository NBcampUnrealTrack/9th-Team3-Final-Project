#include "Monster/knightMonsterKnockback.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "knight.h"

namespace knightMonsterKnockback
{
	/** 날리는 속도 중 앞쪽 비율. 달려오는 몬스터보다 한참 느려야 몸 앞에 붙어 같이 밀려가지 않는다. */
	constexpr float ForwardRatio = 0.25f;
	/** 몸 옆으로 벗어날 때 두 캡슐 사이에 두는 여유. */
	constexpr float SideMargin = 50.f;

	float GetRadius(const AActor& Actor)
	{
		const ACharacter* const Character = Cast<ACharacter>(&Actor);
		return Character && Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : Actor.GetSimpleCollisionRadius();
	}
}

void FknightMonsterKnockback::Start(const AActor& Monster, AActor& Target, float Speed, float Time)
{
	if (Speed <= 0.f || Time <= 0.f)
	{
		return;
	}
	const FVector Forward = Monster.GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = Monster.GetActorRightVector().GetSafeNormal2D();
	const float Lateral = FVector::DotProduct(Target.GetActorLocation() - Monster.GetActorLocation(), Right);
	Side = Lateral >= 0.f ? Right : -Right;
	SourceMonster = &Monster;
	Actor = &Target;
	bFlipped = false;
	// 가드로 세기가 줄어도 몸 옆까지는 밀어 낸다.
	Clearance = knightMonsterKnockback::GetRadius(Monster) + knightMonsterKnockback::GetRadius(Target) + knightMonsterKnockback::SideMargin;
	const float SideSpeed = FMath::Max(Speed, (Clearance - FMath::Abs(Lateral)) / Time);
	Velocity = Side * SideSpeed + Forward * Speed * knightMonsterKnockback::ForwardRatio;
	Remaining = Time;
	UE_LOG(LogknightAI, Verbose, TEXT("%s: knockback %s at %.0f cm/s for %.2fs."), *Monster.GetName(), *Target.GetName(), Speed, Time);
}

void FknightMonsterKnockback::StartAway(const AActor& Monster, AActor& Target, float Speed, float Time)
{
	if (Speed <= 0.f || Time <= 0.f)
	{
		return;
	}
	SourceMonster = &Monster;
	Actor = &Target;
	Velocity = (Target.GetActorLocation() - Monster.GetActorLocation()).GetSafeNormal2D(UE_SMALL_NUMBER, Monster.GetActorForwardVector().GetSafeNormal2D()) * Speed;
	Side = FVector::ZeroVector;
	Clearance = 0.f;
	bFlipped = true;
	Remaining = Time;
	UE_LOG(LogknightAI, Verbose, TEXT("%s: knockback %s away at %.0f cm/s for %.2fs."), *Monster.GetName(), *Target.GetName(), Speed, Time);
}

void FknightMonsterKnockback::Tick(float DeltaTime)
{
	AActor* const Target = Actor.Get();
	if (!Target || Remaining <= 0.f)
	{
		Remaining = 0.f;
		return;
	}
	const float Step = FMath::Min(DeltaTime, Remaining);
	const FVector Delta = Velocity * Step;
	FHitResult Blocked;
	// 캐릭터는 무브먼트로 옮겨 경사진 지형은 타고 넘고, 벽에서만 막히게 한다.
	ACharacter* const Character = Cast<ACharacter>(Target);
	UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (Movement && Movement->UpdatedComponent)
	{
		Movement->SafeMoveUpdatedComponent(Delta, Movement->UpdatedComponent->GetComponentQuat(), true, Blocked);
		if (Blocked.IsValidBlockingHit() && Movement->IsWalkable(Blocked))
		{
			// 오르막 지형이면 남은 이동을 경사면에 붙여 다시 옮긴다. 벽(걸을 수 없는 면)만 막힌 것으로 본다.
			const FVector Rest = FVector::VectorPlaneProject(Delta * (1.f - Blocked.Time), Blocked.Normal);
			Movement->SafeMoveUpdatedComponent(Rest, Movement->UpdatedComponent->GetComponentQuat(), true, Blocked);
			Blocked.bBlockingHit = Blocked.bBlockingHit && !Movement->IsWalkable(Blocked);
		}
	}
	else
	{
		Target->AddActorWorldOffset(Delta, true, &Blocked);
	}
	Remaining -= Step;
	// 한쪽이 벽에 막혔으면 반대쪽으로 한 번 바꿔 날린다. 경로를 건너 반대쪽 몸 옆까지 가도록 남은 시간을 늘린다.
	if (Blocked.bBlockingHit && !bFlipped)
	{
		bFlipped = true;
		const float SideSpeed = FMath::Abs(FVector::DotProduct(Velocity, Side));
		const AActor* const MonsterActor = SourceMonster.Get();
		const float Lateral = MonsterActor ? FMath::Abs(FVector::DotProduct(Target->GetActorLocation() - MonsterActor->GetActorLocation(), Side)) : 0.f;
		Velocity -= 2.f * FVector::DotProduct(Velocity, Side) * Side;
		Side = -Side;
		if (SideSpeed > KINDA_SMALL_NUMBER)
		{
			Remaining = FMath::Max(Remaining, (Lateral + Clearance) / SideSpeed);
		}
		UE_LOG(LogknightAI, Verbose, TEXT("%s: knockback blocked by %s, flipped side for %.2fs."), *Target->GetName(), *GetNameSafe(Blocked.GetActor()), Remaining);
	}
}
