#include "Monster/knightMonsterKnockback.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "knight.h"

void FknightMonsterKnockback::Start(const AActor& Monster, AActor& Target, float Speed, float Time)
{
	if (Speed <= 0.f || Time <= 0.f)
	{
		return;
	}
	const FVector Forward = Monster.GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = Monster.GetActorRightVector().GetSafeNormal2D();
	Side = FVector::DotProduct(Target.GetActorLocation() - Monster.GetActorLocation(), Right) >= 0.f ? Right : -Right;
	Actor = &Target;
	bFlipped = false;
	Velocity = (Side * 2.f + Forward).GetSafeNormal() * Speed;
	Remaining = Time;
	UE_LOG(LogknightAI, Verbose, TEXT("%s: knockback %s at %.0f cm/s for %.2fs."), *Monster.GetName(), *Target.GetName(), Speed, Time);
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
	// 한쪽이 벽에 막혔으면 반대쪽으로 한 번 바꿔 날린다. 몸 앞에 붙은 채 밀려가지 않게 한다.
	if (Blocked.bBlockingHit && !bFlipped)
	{
		bFlipped = true;
		Velocity -= 2.f * FVector::DotProduct(Velocity, Side) * Side;
		UE_LOG(LogknightAI, Verbose, TEXT("%s: knockback blocked by %s, flipped side."), *Target->GetName(), *GetNameSafe(Blocked.GetActor()));
	}
	Remaining -= Step;
}
