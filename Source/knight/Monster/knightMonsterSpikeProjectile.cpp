#include "Monster/knightMonsterSpikeProjectile.h"

#include "Combat/knightCombatLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Monster/knightMonsterDebug.h"
#include "knight.h"

namespace knightMonsterSpike
{
	UStaticMeshComponent* CreateHalf(AActor& Owner, USceneComponent* Parent, const TCHAR* Name)
	{
		UStaticMeshComponent* const Half = Owner.CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Half->SetupAttachment(Parent);
		Half->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return Half;
	}
}

AknightMonsterSpikeProjectile::AknightMonsterSpikeProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	FrontHalf = knightMonsterSpike::CreateHalf(*this, Root, TEXT("FrontHalf"));
	BackHalf = knightMonsterSpike::CreateHalf(*this, Root, TEXT("BackHalf"));
	SetActorEnableCollision(false);
}

void AknightMonsterSpikeProjectile::SetLook(const FknightSpikeLook& Look)
{
	SpikeEffect = Look.Effect;
	if (!Look.Mesh)
	{
		return;
	}
	// 원뿔의 밑면(로컬 최소 Z)을 액터 원점에 두고, 앞쪽 절반은 끝이 +X, 뒤쪽 절반은 -X를 향하게 눕힌다.
	const float BaseOffset = -Look.Mesh->GetBounds().GetBox().Min.Z * Look.Scale.Z;
	const TPair<UStaticMeshComponent*, float> Halves[] = { { FrontHalf, 1.f }, { BackHalf, -1.f } };
	for (const TPair<UStaticMeshComponent*, float>& Half : Halves)
	{
		Half.Key->SetStaticMesh(Look.Mesh);
		if (Look.Material)
		{
			Half.Key->SetMaterial(0, Look.Material);
		}
		Half.Key->SetRelativeScale3D(Look.Scale);
		Half.Key->SetRelativeRotation(FRotator(-90.f * Half.Value, 0.f, 0.f));
		Half.Key->SetRelativeLocation(FVector(BaseOffset * Half.Value, 0.f, 0.f));
	}
}

void AknightMonsterSpikeProjectile::LaunchUp(float Speed, float Duration)
{
	bFalling = false;
	Velocity = FVector::UpVector * Speed;
	Remaining = FMath::Max(Duration, 0.01f);
	SetActorRotation(Velocity.Rotation());
}

void AknightMonsterSpikeProjectile::LaunchDown(AActor* InAttacker, const FknightMeleeHitParams& InHitParams, const FknightSpikeImpact& InImpact,
	float FallTime, const TSharedPtr<FknightSpikeVolleyHits>& InVolleyHits)
{
	Attacker = InAttacker;
	HitParams = InHitParams;
	Impact = InImpact;
	VolleyHits = InVolleyHits;
	bFalling = true;
	Remaining = FMath::Max(FallTime, 0.01f);
	Velocity = (Impact.Landing - GetActorLocation()) / Remaining;
	SetActorRotation(Velocity.Rotation());
	if (Impact.FallSound)
	{
		UGameplayStatics::SpawnSoundAttached(Impact.FallSound, Root);
	}
	// 올라가는 가시는 금방 사라지므로 떨어지는 가시에만 불길을 붙인다. 가시는 비스듬히 돌 수 있어도 불길은 늘 위로 오른다.
	if (SpikeEffect)
	{
		if (UNiagaraComponent* const Fire = UNiagaraFunctionLibrary::SpawnSystemAttached(SpikeEffect, Root, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset, true))
		{
			Fire->SetUsingAbsoluteRotation(true);
			Fire->SetWorldRotation(FRotator::ZeroRotator);
		}
	}
}

void AknightMonsterSpikeProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bLanded)
	{
		return;
	}
	const float Step = FMath::Min(DeltaSeconds, Remaining);
	SetActorLocation(GetActorLocation() + Velocity * Step);
	Remaining -= Step;
	if (Remaining > 0.f)
	{
		return;
	}
	if (bFalling)
	{
		Land();
	}
	else
	{
		Destroy();
	}
}

void AknightMonsterSpikeProjectile::Land()
{
	bLanded = true;
	SetActorTickEnabled(false);
	SetActorLocation(Impact.Landing);
	SetLifeSpan(FMath::Max(Impact.StuckTime, 0.01f));

	for (UNiagaraSystem* const Effect : Impact.Effects)
	{
		if (Effect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Effect, Impact.Landing, FRotator::ZeroRotator, FVector::OneVector, true, true,
				ENCPoolMethod::AutoRelease);
		}
	}
	if (Impact.ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Impact.ImpactSound, Impact.Landing);
	}
	UWorld* const World = GetWorld();
	AActor* const AttackerActor = Attacker.Get();
	if (!World || !AttackerActor)
	{
		return;
	}

	// 가시는 대상을 통과해 땅에 박히므로, 착탄점 위 낙하 경로(기둥) 안에 있던 대상이 맞는다. 맞은 기록은 볼리 전체가 같이 쓴다.
	const float HalfHeight = FMath::Max(Impact.PathHeight * 0.5f, Impact.HitRadius);
	const FVector PathCenter = Impact.Landing + FVector::UpVector * HalfHeight;
	FknightSpikeVolleyHits LocalVolley;
	FknightSpikeVolleyHits& Volley = VolleyHits ? *VolleyHits : LocalVolley;
	const double Now = World->GetTimeSeconds();
	// 아직 다시 맞을 수 없는 대상은 이미 맞은 것으로 넘겨 건너뛰게 한다.
	TSet<TWeakObjectPtr<AActor>> Skip;
	for (const TPair<TWeakObjectPtr<AActor>, double>& Entry : Volley.LastHitTime)
	{
		if (Volley.RehitInterval < 0.f || Now - Entry.Value < Volley.RehitInterval)
		{
			Skip.Add(Entry.Key);
		}
	}
	TSet<TWeakObjectPtr<AActor>> Hit = Skip;
	const int32 NewHits = UknightCombatLibrary::ApplyAreaMeleeHits(AttackerActor, PathCenter, FCollisionShape::MakeCapsule(Impact.HitRadius, HalfHeight),
		HitParams, Hit, [](const AActor&) { return true; },
		[this, AttackerActor](AActor& Target, EknightHitOutcome Outcome) { UknightMonsterGuardPushComponent::HandleHit(AttackerActor, Target, Outcome, GuardReaction); },
		FVector::UpVector);
	for (const TWeakObjectPtr<AActor>& Actor : Hit)
	{
		if (!Skip.Contains(Actor))
		{
			Volley.LastHitTime.Add(Actor, Now);
		}
	}

#if ENABLE_DRAW_DEBUG
	if (knightMonsterDebug::ShouldDrawAttacks())
	{
		DrawDebugCapsule(World, PathCenter, HalfHeight, Impact.HitRadius, FQuat::Identity, NewHits == 0 ? FColor::Red : FColor::Magenta, false, 1.f, 0, 3.f);
	}
#endif
}
