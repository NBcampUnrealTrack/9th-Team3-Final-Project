#include "Monster/knightMonsterTornado.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/knightCombatLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "Monster/knightMonsterDebug.h"
#include "System/knightGameplayTags.h"
#include "UObject/ConstructorHelpers.h"

namespace knightMonsterTornado
{
	/** 처음 생길 때의 크기 비율. 0이면 판정이 없어 생기는 순간이 안 보인다. */
	constexpr float StartScale = 0.2f;
	constexpr float SpinRate = 540.f;
}

AknightMonsterTornado::AknightMonsterTornado()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (Cone.Succeeded())
	{
		Mesh->SetStaticMesh(Cone.Object);
	}
	EffectComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Effect"));
	EffectComponent->SetupAttachment(Root);
	EffectComponent->SetAutoActivate(false);
	SetActorEnableCollision(false);
}

void AknightMonsterTornado::Launch(AActor* InAttacker, AActor* InTarget, const FknightMeleeHitParams& InHitParams,
	const FknightMonsterTornadoParams& InParams, const FVector& Direction)
{
	Attacker = InAttacker;
	Target = InTarget;
	HitParams = InHitParams;
	Params = InParams;
	MoveDirection = Direction.GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector);
	Age = 0.f;
	FadeStartAge = -1.f;
	FVector Location = GetActorLocation();
	SnapToFloor(Location);
	SetActorLocation(Location);
	if (Params.Effect)
	{
		Mesh->SetVisibility(false);
		EffectComponent->SetAsset(Params.Effect);
		EffectComponent->Activate(true);
	}
	UpdateVisual(knightMonsterTornado::StartScale, 0.f);
}

void AknightMonsterTornado::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	const AActor* const Source = Attacker.Get();
	const UAbilitySystemComponent* const SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Source);
	if (FadeStartAge < 0.f && (!Source || (SourceASC && SourceASC->HasMatchingGameplayTag(knightGameplayTags::State_Dead))
		|| Age >= Params.GrowTime + Params.ChaseTime))
	{
		FadeStartAge = Age;
	}
	if (FadeStartAge >= 0.f)
	{
		const float FadeAlpha = Params.FadeTime > 0.f ? (Age - FadeStartAge) / Params.FadeTime : 1.f;
		if (FadeAlpha >= 1.f)
		{
			Destroy();
			return;
		}
		UpdateVisual(1.f - FadeAlpha, DeltaSeconds);
		return;
	}

	const float GrowAlpha = Params.GrowTime > 0.f ? FMath::Clamp(Age / Params.GrowTime, 0.f, 1.f) : 1.f;
	const float Scale = FMath::Lerp(knightMonsterTornado::StartScale, 1.f, FMath::SmoothStep(0.f, 1.f, GrowAlpha));
	if (GrowAlpha >= 1.f)
	{
		// 타깃 쪽으로 TurnRate만큼만 꺾으며 바닥을 따라 나아간다.
		if (const AActor* const Chased = Target.Get())
		{
			const FVector ToTarget = (Chased->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			if (!ToTarget.IsNearlyZero())
			{
				const float Current = MoveDirection.Rotation().Yaw;
				const float Desired = ToTarget.Rotation().Yaw;
				const float Delta = FMath::Clamp(FMath::FindDeltaAngleDegrees(Current, Desired), -Params.TurnRate * DeltaSeconds, Params.TurnRate * DeltaSeconds);
				MoveDirection = FRotator(0.f, Current + Delta, 0.f).Vector();
			}
		}
		FVector Location = GetActorLocation() + MoveDirection * Params.Speed * DeltaSeconds;
		SnapToFloor(Location);
		SetActorLocation(Location);
	}
	UpdateVisual(Scale, DeltaSeconds);
	ApplyHits(Scale);
}

void AknightMonsterTornado::ApplyHits(float Scale)
{
	AActor* const Source = Attacker.Get();
	if (!Source)
	{
		return;
	}
	const float Radius = Params.Radius * Scale;
	const float HalfHeight = FMath::Max(Params.Height * Scale * 0.5f, Radius);
	const FVector Center = GetActorLocation() + FVector(0.f, 0.f, HalfHeight);
	TSet<TWeakObjectPtr<AActor>>& Hits = HitActors;
	const int32 NewHits = UknightCombatLibrary::ApplyAreaMeleeHits(Source, Center, FCollisionShape::MakeCapsule(Radius, HalfHeight), HitParams, HitActors,
		[](const AActor&) { return true; },
		[&Hits](AActor& HitActor, EknightHitOutcome Outcome)
		{
			// 구르기 무적으로 피했으면 다시 맞을 수 있다. 무적이 끝나고도 안에 있으면 맞는다.
			if (Outcome == EknightHitOutcome::Evaded)
			{
				Hits.Remove(&HitActor);
			}
		});
#if ENABLE_DRAW_DEBUG
	if (knightMonsterDebug::ShouldDrawAttacks())
	{
		DrawDebugCapsule(GetWorld(), Center, HalfHeight, Radius, FQuat::Identity, NewHits > 0 ? FColor::Magenta : FColor::Red, false, 0.f, 0, 3.f);
	}
#endif
}

void AknightMonsterTornado::UpdateVisual(float Scale, float DeltaSeconds)
{
	const float Radius = Params.Radius * Scale;
	const float Height = Params.Height * Scale;
	Spin = FMath::Fmod(Spin + knightMonsterTornado::SpinRate * DeltaSeconds, 360.f);
	if (Mesh->IsVisible() && Mesh->GetStaticMesh())
	{
		// 기본 원뿔을 거꾸로 세워(꼭짓점이 바닥) 판정 원기둥 크기에 맞춘다.
		const FBoxSphereBounds Bounds = Mesh->GetStaticMesh()->GetBounds();
		const FVector Extent = Bounds.BoxExtent;
		const FVector MeshScale(Radius / FMath::Max(1.f, static_cast<float>(Extent.X)), Radius / FMath::Max(1.f, static_cast<float>(Extent.Y)),
			Height * 0.5f / FMath::Max(1.f, static_cast<float>(Extent.Z)));
		const FQuat Rotation = FQuat(FVector::UpVector, FMath::DegreesToRadians(Spin)) * FQuat(FVector::ForwardVector, PI);
		Mesh->SetRelativeScale3D(MeshScale);
		Mesh->SetRelativeRotation(Rotation);
		Mesh->SetRelativeLocation(FVector(0.f, 0.f, Height * 0.5f) - Rotation.RotateVector(Bounds.Origin * MeshScale));
	}
	EffectComponent->SetRelativeScale3D(FVector(Scale));
}

void AknightMonsterTornado::SnapToFloor(FVector& Location) const
{
	const UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}
	FHitResult Floor;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(knightTornadoFloor), false, this);
	QueryParams.AddIgnoredActor(Attacker.Get());
	// 채널로 쏘면 캐릭터 캡슐도 막아 타깃 머리 위로 올라탄다. 바닥(WorldStatic 물체)만 본다.
	if (World->LineTraceSingleByObjectType(Floor, Location + FVector(0.f, 0.f, 500.f), Location - FVector(0.f, 0.f, 2000.f),
		FCollisionObjectQueryParams(ECC_WorldStatic), QueryParams))
	{
		Location.Z = Floor.ImpactPoint.Z;
	}
}
