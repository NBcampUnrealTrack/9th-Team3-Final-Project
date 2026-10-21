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
	// 발밑을 이만큼 쓸어 몸 윗면에 서 있거나 걸쳐 있는지 본다.
	constexpr float StandProbe = 5.f;
}

UknightMonsterPushOutComponent::UknightMonsterPushOutComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 몬스터와 플레이어가 이번 프레임 이동을 마친 뒤에 겹침을 본다.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	ShallowMeshPushOutTags.AddTag(knightGameplayTags::State_HitReact_Knockdown);
	PassThroughPawnTags.AddTag(knightGameplayTags::State_Monster_PassThroughPawns);
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
		TopPrimitives.Append(Blockers);
		const ACharacter* const Character = Cast<ACharacter>(Owner);
		MovementCapsule = Character ? Character->GetCapsuleComponent() : nullptr;
		// 이동 캡슐이 플레이어를 막으면 그 윗면에도 올라설 수 있다.
		if (MovementCapsule && MovementCapsule->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block)
		{
			TopPrimitives.Add(MovementCapsule);
		}
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

	const float MeshPenetration = IsShallowMeshPushOut() ? ShallowMeshPenetration : 0.f;
	const bool bWantPassThrough = WantsPassThroughPawns();
	if (bWantPassThrough && !bCapsulePassingThrough)
	{
		SetCapsulePassThrough(true);
	}
	bool bAnyInsideCapsule = false;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* const PlayerController = It->Get();
		ACharacter* const Character = PlayerController ? Cast<ACharacter>(PlayerController->GetPawn()) : nullptr;
		UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
		UCapsuleComponent* const Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
		if (!Movement || !Capsule)
		{
			continue;
		}
		// 캡슐이 플레이어를 통과시키는 동안에는 캡슐 안에 들어온 플레이어도 밀어낸다. 다시 막을 때 겹쳐 있으면 엔진이 한 번에 튕겨 내기 때문이다.
		const bool bInsideCapsule = bCapsulePassingThrough && IsOverlappingPrimitive(Character, MovementCapsule, 0.f);
		bAnyInsideCapsule |= bInsideCapsule;
		if (!bInsideCapsule && !IsOverlappingBody(Character, MeshPenetration) && !IsStandingOnBody(Character))
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

	// 태그가 빠져도 플레이어가 캡슐 밖으로 나갈 때까지 기다렸다가 다시 막는다.
	if (!bWantPassThrough && bCapsulePassingThrough && !bAnyInsideCapsule)
	{
		SetCapsulePassThrough(false);
	}
}

bool UknightMonsterPushOutComponent::WantsPassThroughPawns() const
{
	if (!MovementCapsule || PassThroughPawnTags.IsEmpty())
	{
		return false;
	}
	const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	return ASC && ASC->HasAnyMatchingGameplayTags(PassThroughPawnTags);
}

void UknightMonsterPushOutComponent::SetCapsulePassThrough(bool bPassThrough)
{
	if (bPassThrough)
	{
		SavedCapsulePawnResponse = MovementCapsule->GetCollisionResponseToChannel(ECC_Pawn);
		MovementCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
	else
	{
		MovementCapsule->SetCollisionResponseToChannel(ECC_Pawn, SavedCapsulePawnResponse);
	}
	bCapsulePassingThrough = bPassThrough;
	UE_LOG(LogknightAI, Verbose, TEXT("%s movement capsule %s pawns."), *GetNameSafe(GetOwner()), bPassThrough ? TEXT("passes through") : TEXT("blocks"));
}

bool UknightMonsterPushOutComponent::IsShallowMeshPushOut() const
{
	if (!MeshPrimitive || ShallowMeshPushOutTags.IsEmpty())
	{
		return false;
	}
	const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	return ASC && ASC->HasAnyMatchingGameplayTags(ShallowMeshPushOutTags);
}

bool UknightMonsterPushOutComponent::IsOverlappingBody(const ACharacter* Character, float MeshPenetration) const
{
	for (const UPrimitiveComponent* const Primitive : BodyPrimitives)
	{
		if (IsOverlappingPrimitive(Character, Primitive, Primitive == MeshPrimitive ? MeshPenetration : 0.f))
		{
			return true;
		}
	}
	return false;
}

bool UknightMonsterPushOutComponent::IsOverlappingPrimitive(const ACharacter* Character, const UPrimitiveComponent* Primitive, float Penetration) const
{
	// 죽으면 차단 상자 충돌이 꺼지는 등 도중에 바뀔 수 있어 매번 확인한다.
	if (!Primitive || !Primitive->IsQueryCollisionEnabled())
	{
		return false;
	}
	const UCapsuleComponent* const Capsule = Character->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius() - knightMonsterPushOut::Skin;
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight() - knightMonsterPushOut::Skin;
	// 허용 깊이만큼 캡슐을 더 줄여서 본다. 캡슐 중심선 둘레는 남겨 깊이 들어온 것은 계속 잡는다.
	const float Shrink = FMath::Clamp(Penetration, 0.f, Radius - knightMonsterPushOut::Skin);
	if (Radius - Shrink <= 0.f)
	{
		return false;
	}
	const FVector Location = Capsule->GetComponentLocation();
	if (FVector::DistSquared(Primitive->Bounds.Origin, Location) > FMath::Square(Primitive->Bounds.SphereRadius + Radius + HalfHeight))
	{
		return false;
	}
	return Primitive->OverlapComponent(Location, Capsule->GetComponentQuat(), FCollisionShape::MakeCapsule(Radius - Shrink, HalfHeight - Shrink));
}

bool UknightMonsterPushOutComponent::IsStandingOnBody(const ACharacter* Character) const
{
	const AActor* const Owner = GetOwner();
	const UPrimitiveComponent* const Base = Cast<UPrimitiveComponent>(Character->GetMovementBaseObject());
	if (Base && Base->GetOwner() == Owner)
	{
		return true;
	}
	// 떨어지는 중에 모서리에 걸치면 바닥(MovementBase)이 없어서, 발밑을 조금 쓸어 받치는 것이 몸인지 본다.
	const UCapsuleComponent* const Capsule = Character->GetCapsuleComponent();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
	const FVector Start = Capsule->GetComponentLocation();
	const FVector End = Start - FVector(0.f, 0.f, knightMonsterPushOut::StandProbe);
	const FQuat Rotation = Capsule->GetComponentQuat();
	for (UPrimitiveComponent* const Primitive : TopPrimitives)
	{
		if (!Primitive || !Primitive->IsQueryCollisionEnabled())
		{
			continue;
		}
		FHitResult Hit;
		// 옆면에 닿아 있는 것(처음부터 겹침)은 서 있는 것이 아니다. 아래쪽에서 받치는 면만 센다.
		if (Primitive->SweepComponent(Hit, Start, End, Rotation, Shape) && !Hit.bStartPenetrating
			&& Hit.ImpactNormal.Z > UE_KINDA_SMALL_NUMBER)
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
