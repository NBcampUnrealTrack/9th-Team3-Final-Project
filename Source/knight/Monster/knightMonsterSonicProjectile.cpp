#include "Monster/knightMonsterSonicProjectile.h"

#include "Camera/knightPlayerCameraManager.h"
#include "Combat/knightCombatLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Environment/knightWaterRippleSettings.h"
#include "Environment/knightWaterRippleSubsystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Monster/knightMonsterDebug.h"
#include "UObject/ConstructorHelpers.h"
#include "knight.h"

AknightMonsterSonicProjectile::AknightMonsterSonicProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Mesh->SetStaticMesh(Sphere.Object);
	}
	EffectComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Effect"));
	// 이펙트는 판정 크기로 늘린 구체가 아니라 스케일 1인 루트에 붙인다. 나이아가라 경량 이미터는 붙은 컴포넌트의 스케일을 위치·속도에 받는다.
	EffectComponent->SetupAttachment(RootComponent);
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
	Mesh->SetRelativeScale3D(FVector(InRadius / 50.f));
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
	EknightHitOutcome Impact = EknightHitOutcome::Ignored;
	FVector ImpactAt = End;
	FVector ImpactDirection = Velocity.GetSafeNormal();
	const AActor* ImpactTarget = nullptr;
	for (const FVector& Center : { FMath::Lerp(Start, End, 0.5f), End })
	{
		UknightCombatLibrary::ApplyAreaMeleeHits(Source, Center, FCollisionShape::MakeSphere(Radius), HitParams, HitActors,
			[](const AActor&) { return true; },
			[this, Source, &bHit, &Impact, &ImpactAt, &ImpactDirection, &ImpactTarget](AActor& Target, EknightHitOutcome Outcome)
			{
				if (Outcome == EknightHitOutcome::Evaded || Outcome == EknightHitOutcome::Ignored)
				{
					return;
				}
				bHit = true;
				Impact = Outcome;
				ImpactTarget = &Target;
				// 패링은 몬스터와 대상을 잇는 방향을 기준으로 튕겨 낸다. 진행 방향을 쓰면 공중에서 비스듬히 쏜 음파나
				// 옆으로 벌려 쏜 음파에서 이펙트가 몬스터 쪽을 보지 않아 화면에 따라 어색하다.
				if (Outcome == EknightHitOutcome::Parried)
				{
					ImpactDirection = (Target.GetActorLocation() - Source->GetActorLocation()).GetSafeNormal(UE_SMALL_NUMBER, ImpactDirection);
				}
				// 대상 몸 앞(음파가 온 쪽)에서 부서지게 한다.
				ImpactAt = Target.GetActorLocation() - ImpactDirection * 60.f;
				if (Outcome == EknightHitOutcome::Hit)
				{
					DeafenTarget(Target);
				}
			});
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
	if (bHit)
	{
		CheckNearMiss(Start, End, ImpactTarget, true);
		SpawnImpact(Impact, ImpactAt, ImpactDirection);
		Destroy();
		return;
	}
	// 수면을 지나쳤으면 물에 닿은 것으로 본다(물이 WorldStatic이 아닌 레벨도 있다).
	float SurfaceZ = 0.f;
	const bool bWater = UknightWaterRippleSubsystem::FindWaterSurface(World, End, SurfaceZ) && End.Z <= SurfaceZ && Start.Z > SurfaceZ - 1.f;
	const bool bEnding = bWater || bBlocked || RemainingDistance <= 0.f;
	CheckNearMiss(Start, End, nullptr, bEnding);
	if (bEnding)
	{
		const FVector Location = bWater ? FMath::Lerp(Start, End, FMath::Clamp((Start.Z - SurfaceZ) / FMath::Max(Start.Z - End.Z, 1.f), 0.f, 1.f))
			: bBlocked ? WorldHit.ImpactPoint : End;
		SpawnWaterImpact(Location);
		SpawnImpact(EknightHitOutcome::Ignored, Location, Velocity.GetSafeNormal());
		Destroy();
		return;
	}
	SetActorLocation(End);
}

void AknightMonsterSonicProjectile::DeafenTarget(AActor& Target) const
{
	const APawn* const Pawn = Cast<APawn>(&Target);
	const APlayerController* const PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (AknightPlayerCameraManager* const Camera = DeafenTime > 0.f && PC && PC->IsLocalController() ? Cast<AknightPlayerCameraManager>(PC->PlayerCameraManager) : nullptr)
	{
		Camera->StartDeafen(DeafenTime);
	}
}

void AknightMonsterSonicProjectile::SpawnImpact(EknightHitOutcome Outcome, const FVector& Location, const FVector& Direction) const
{
	UNiagaraSystem* const System = Outcome == EknightHitOutcome::Hit ? ImpactEffects.Hit.Get()
		: Outcome == EknightHitOutcome::Guarded ? ImpactEffects.Guarded.Get()
		: Outcome == EknightHitOutcome::Parried ? ImpactEffects.Parried.Get()
		: ImpactEffects.World.Get();
	UE_LOG(LogknightAI, Log, TEXT("%s: sonic ends (%s)."), *GetNameSafe(Attacker.Get()), *UEnum::GetValueAsString(Outcome));
	if (System)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), System, Location, Direction.Rotation(), FVector::OneVector, true, true, ENCPoolMethod::AutoRelease);
	}
}

bool AknightMonsterSonicProjectile::SpawnWaterImpact(const FVector& Location) const
{
	UWorld* const World = GetWorld();
	float SurfaceZ = 0.f;
	// 음파 반지름 안까지 수면에 닿았으면 물에 부딪힌 것으로 본다.
	if (!World || !UknightWaterRippleSubsystem::FindWaterSurface(World, Location, SurfaceZ) || Location.Z - SurfaceZ > Radius)
	{
		return false;
	}
	const FVector Epicenter(Location.X, Location.Y, SurfaceZ);
	UNiagaraSystem* const Splash = ImpactEffects.WaterSplash ? ImpactEffects.WaterSplash.Get()
		: GetDefault<UknightWaterRippleSettings>()->SplashHeavySystem.LoadSynchronous();
	if (Splash)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Splash, Epicenter, FRotator::ZeroRotator, FVector(ImpactEffects.WaterSplashScale), true, true,
			ENCPoolMethod::AutoRelease);
	}
	UknightWaterRippleSubsystem* const Ripples = World->GetSubsystem<UknightWaterRippleSubsystem>();
	if (Ripples && ImpactEffects.WaterRippleRadius > 0.f && ImpactEffects.WaterRippleDepth > 0.f)
	{
		Ripples->AddImpulse(Epicenter, ImpactEffects.WaterRippleRadius, ImpactEffects.WaterRippleDepth);
	}
	return true;
}

void AknightMonsterSonicProjectile::CheckNearMiss(const FVector& Start, const FVector& End, const AActor* HitTarget, bool bEnding)
{
	if (ImpactEffects.NearMissDistance <= 0.f)
	{
		return;
	}
	AknightPlayerCameraManager::ForEachLocalCamera(this, [this, &Start, &End, HitTarget, bEnding](AknightPlayerCameraManager& Camera)
	{
		const APlayerController* const PC = Camera.GetOwningPlayerController();
		const APawn* const Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn || Pawn == HitTarget || NearMissed.Contains(Pawn))
		{
			return;
		}
		// 아직 다가오는 중(가장 가까운 점이 구간 끝)이면 지나갈 때까지 기다린다. 이번 틱에 사라지면 그 자리에서 판단한다.
		const FVector Closest = FMath::ClosestPointOnSegment(Pawn->GetActorLocation(), Start, End);
		const bool bPassed = bEnding || !Closest.Equals(End, 1.f);
		if (bPassed && FVector::Dist(Closest, Pawn->GetActorLocation()) - Radius <= ImpactEffects.NearMissDistance)
		{
			NearMissed.Add(Pawn);
			Camera.StartSonicNearMiss();
		}
	});
}
