#include "Monster/knightMonsterPushOutComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Monster/knightMonsterBodyBlockerComponent.h"
#include "Interfaces/CombatInterface.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

namespace knightMonsterPushOut
{
	// 플레이어 캡슐을 조금 줄여서 검사한다. 몸에 맞닿아 서 있는 것(겹침 0)까지 밀어내지 않게.
	constexpr float Skin = 2.f;
}

UknightMonsterPushOutComponent::UknightMonsterPushOutComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 몬스터와 플레이어가 이번 프레임 이동을 마친 뒤에 겹침을 본다.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	MeshPushOutPauseTags.AddTag(knightGameplayTags::State_HitReact_Knockdown);
}

void UknightMonsterPushOutComponent::BeginPlay()
{
	Super::BeginPlay();

	const AActor* const Owner = GetOwner();
	if (Owner)
	{
		TArray<UknightMonsterBodyBlockerComponent*> Blockers;
		Owner->GetComponents<UknightMonsterBodyBlockerComponent>(Blockers);
		BodyPrimitives.Append(Blockers);
		const ACharacter* const Character = Cast<ACharacter>(Owner);
		if (bPushOutOfMesh && Character && Character->GetMesh())
		{
			MeshPrimitive = Character->GetMesh();
			BodyPrimitives.Add(MeshPrimitive);
		}
	}
	SetComponentTickEnabled(BodyPrimitives.Num() > 0);
}

void UknightMonsterPushOutComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* const Owner = GetOwner();
	UWorld* const World = GetWorld();
	if (!Owner || !World || PushSpeed <= 0.f)
	{
		return;
	}
	if (Owner->GetClass()->ImplementsInterface(UCombatInterface::StaticClass()) && ICombatInterface::Execute_IsDeadBP(Owner))
	{
		return;
	}

	const bool bIncludeMesh = !IsMeshPushOutPaused();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* const PlayerController = It->Get();
		ACharacter* const Character = PlayerController ? Cast<ACharacter>(PlayerController->GetPawn()) : nullptr;
		UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
		UCapsuleComponent* const Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
		if (!Movement || !Capsule || !IsOverlappingBody(Character, bIncludeMesh))
		{
			continue;
		}

		const FVector Delta = GetPushDirection(Character->GetActorLocation()) * PushSpeed * DeltaTime;
		// 이미 몸과 겹친 상태라 몬스터를 무시하고 옮긴다. 벽·지형에는 그대로 막히고, 막히면 벽을 따라 미끄러진다.
		const bool bWasIgnoring = Capsule->GetMoveIgnoreActors().Contains(Owner);
		Capsule->IgnoreActorWhenMoving(Owner, true);
		FHitResult Hit;
		Movement->SafeMoveUpdatedComponent(Delta, Capsule->GetComponentQuat(), true, Hit);
		if (Hit.IsValidBlockingHit())
		{
			// 캐릭터 이동 컴포넌트에서는 protected라 기반 클래스(public)로 부른다. 가상 함수라 캐릭터용 미끄러짐이 그대로 쓰인다.
			static_cast<UMovementComponent*>(Movement)->SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
		}
		if (!bWasIgnoring)
		{
			Capsule->IgnoreActorWhenMoving(Owner, false);
		}
		UE_LOG(LogknightAI, VeryVerbose, TEXT("%s pushed %s out of its body (%.0f cm)."), *GetNameSafe(Owner), *GetNameSafe(Character), Delta.Size());
	}
}

bool UknightMonsterPushOutComponent::IsMeshPushOutPaused() const
{
	if (!MeshPrimitive || MeshPushOutPauseTags.IsEmpty())
	{
		return false;
	}
	const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	return ASC && ASC->HasAnyMatchingGameplayTags(MeshPushOutPauseTags);
}

bool UknightMonsterPushOutComponent::IsOverlappingBody(const ACharacter* Character, bool bIncludeMesh) const
{
	const UCapsuleComponent* const Capsule = Character->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius() - knightMonsterPushOut::Skin;
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight() - knightMonsterPushOut::Skin;
	if (Radius <= 0.f)
	{
		return false;
	}
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	const FVector Location = Capsule->GetComponentLocation();
	const FQuat Rotation = Capsule->GetComponentQuat();

	for (const UPrimitiveComponent* const Primitive : BodyPrimitives)
	{
		// 죽으면 차단 상자 충돌이 꺼지는 등 도중에 바뀔 수 있어 매번 확인한다.
		if (!Primitive || !Primitive->IsQueryCollisionEnabled() || (!bIncludeMesh && Primitive == MeshPrimitive))
		{
			continue;
		}
		if (FVector::DistSquared(Primitive->Bounds.Origin, Location) > FMath::Square(Primitive->Bounds.SphereRadius + Radius + HalfHeight))
		{
			continue;
		}
		if (Primitive->OverlapComponent(Location, Rotation, Shape))
		{
			return true;
		}
	}
	return false;
}

FVector UknightMonsterPushOutComponent::GetPushDirection(const FVector& Location) const
{
	const AActor* const Owner = GetOwner();
	const FVector Forward = Owner->GetActorForwardVector().GetSafeNormal2D();
	const FVector Center = Owner->GetActorLocation();
	const FVector AxisPoint = FMath::ClosestPointOnSegment(Location, Center - Forward * BodyAxisHalfLength, Center + Forward * BodyAxisHalfLength);
	const FVector Away = (Location - AxisPoint).GetSafeNormal2D();
	// 중심선 바로 위에 있으면 방향이 없으므로 옆으로 민다.
	return Away.IsNearlyZero() ? Owner->GetActorRightVector().GetSafeNormal2D() : Away;
}
