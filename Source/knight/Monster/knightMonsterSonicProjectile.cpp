#include "Monster/knightMonsterSonicProjectile.h"

#include "Combat/knightCombatLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "Monster/knightMonsterDebug.h"
#include "UObject/ConstructorHelpers.h"
#include "knight.h"

AknightMonsterSonicProjectile::AknightMonsterSonicProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Mesh->SetStaticMesh(Sphere.Object);
	}
	EffectComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Effect"));
	EffectComponent->SetupAttachment(Mesh);
	EffectComponent->SetAutoActivate(false);
	SetActorEnableCollision(false);
}

void AknightMonsterSonicProjectile::Launch(AActor* InAttacker, const FknightMeleeHitParams& InHitParams, const FVector& Direction, float InSpeed,
	float InRadius, float InMaxDistance, UNiagaraSystem* Effect)
{
	Attacker = InAttacker;
	HitParams = InHitParams;
	Velocity = Direction.GetSafeNormal() * InSpeed;
	Radius = InRadius;
	RemainingDistance = InMaxDistance;
	SetActorRotation(Direction.Rotation());
	// 기본 구체는 지름 100cm. 판정 반지름에 맞춘다.
	Mesh->SetWorldScale3D(FVector(InRadius / 50.f));
	if (Effect)
	{
		Mesh->SetVisibility(false);
		EffectComponent->SetAsset(Effect);
		EffectComponent->Activate(true);
	}
}

void AknightMonsterSonicProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* const World = GetWorld();
	AActor* const Source = Attacker.Get();
	if (!World || !Source)
	{
		Destroy();
		return;
	}
	const FVector Start = GetActorLocation();
	const float Step = FMath::Min(static_cast<float>(Velocity.Size()) * DeltaSeconds, RemainingDistance);
	const FVector End = Start + Velocity.GetSafeNormal() * Step;
	RemainingDistance -= Step;

	// 이번 틱에 지나간 구간의 중간과 끝을 구체로 본다. 빠른 투사체가 얇은 대상을 건너뛰지 않게 한다.
	// 회피(무적)로 피하면 그 대상은 다시 맞지 않고 투사체는 그대로 지나간다.
	bool bHit = false;
	for (const FVector& Center : { FMath::Lerp(Start, End, 0.5f), End })
	{
		UknightCombatLibrary::ApplyAreaMeleeHits(Source, Center, FCollisionShape::MakeSphere(Radius), HitParams, HitActors,
			[](const AActor&) { return true; },
			[&bHit](AActor&, EknightHitOutcome Outcome) { bHit |= Outcome != EknightHitOutcome::Evaded && Outcome != EknightHitOutcome::Ignored; });
		if (bHit)
		{
			break;
		}
	}
#if ENABLE_DRAW_DEBUG
	if (knightMonsterDebug::ShouldDrawAttacks())
	{
		DrawDebugSphere(World, End, Radius, 12, bHit ? FColor::Magenta : FColor::Red, false, DeltaSeconds * 2.f);
	}
#endif
	FHitResult WorldHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(knightSonicProjectile), false, Source);
	Params.AddIgnoredActor(this);
	const bool bBlocked = World->LineTraceSingleByChannel(WorldHit, Start, End, ECC_WorldStatic, Params);
	if (bHit || bBlocked || RemainingDistance <= 0.f)
	{
		Destroy();
		return;
	}
	SetActorLocation(End);
}
