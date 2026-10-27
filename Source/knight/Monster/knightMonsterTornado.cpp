#include "Monster/knightMonsterTornado.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/knightCombatLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "GameFramework/PlayerController.h"
#include "Environment/knightWaterRippleSettings.h"
#include "Environment/knightWaterRippleSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Monster/knightMonsterDebug.h"
#include "System/knightGameplayTags.h"
#include "UObject/ConstructorHelpers.h"

namespace knightMonsterTornado
{
	/** 처음 생길 때의 크기 비율. 0이면 판정이 없어 생기는 순간이 안 보인다. */
	constexpr float StartScale = 0.2f;
	constexpr float SpinRate = 540.f;
	/** 깔때기 메시의 높이와 판정 반지름에 맞춘 반지름. */
	constexpr float FunnelMeshSize = 100.f;
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
	bFunnel = Params.FunnelMesh && Params.FunnelMaterial;
	bBurst = false;
	if (bFunnel)
	{
		Mesh->SetStaticMesh(Params.FunnelMesh);
		FunnelMaterialInstance = Mesh->CreateDynamicMaterialInstance(0, Params.FunnelMaterial);
		if (Params.StreamMaterial)
		{
			StreamMesh = NewObject<UStaticMeshComponent>(this, TEXT("Stream"));
			StreamMesh->SetStaticMesh(Params.FunnelMesh);
			StreamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			StreamMesh->SetCastShadow(false);
			StreamMesh->SetupAttachment(Root);
			StreamMesh->RegisterComponent();
			StreamMaterialInstance = StreamMesh->CreateDynamicMaterialInstance(0, Params.StreamMaterial);
		}
	}
	else if (Params.Effect)
	{
		Mesh->SetVisibility(false);
	}
	if (Params.Effect)
	{
		EffectComponent->SetAsset(Params.Effect);
	}
	bOverWater = false;
	ChurnTime = Params.WaterChurnInterval;
	UpdateWater(knightMonsterTornado::StartScale, false, 0.f);
	FormComponent = SpawnAtBase(Params.FormEffect, true);
	if (Params.SpawnSound)
	{
		UGameplayStatics::SpawnSoundAttached(Params.SpawnSound, Root, NAME_None, FVector(0.f, 0.f, BaseOffset), EAttachLocation::KeepRelativeOffset);
	}
	// 깔때기가 있으면 기둥이 차오를 때 입자를 켠다. 없으면 처음부터 작은 크기로 켠다.
	if (!bFunnel && Params.Effect)
	{
		EffectComponent->Activate(true);
	}
	UpdateVisual(knightMonsterTornado::StartScale, 0.f, 0.f, 0.f, 0.f);
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
		StartCollapse();
	}
	if (FadeStartAge >= 0.f)
	{
		const float FadeAlpha = Params.FadeTime > 0.f ? (Age - FadeStartAge) / Params.FadeTime : 1.f;
		if (FadeAlpha >= 1.f)
		{
			Destroy();
			return;
		}
		// 기둥이 아래부터 풀려 올라가고, 뒤 절반에 걸쳐 옅어진다.
		UpdateVisual(1.f - FadeAlpha, 1.f, FMath::SmoothStep(0.f, 1.f, FadeAlpha), FMath::Clamp(2.f * (1.f - FadeAlpha), 0.f, 1.f), DeltaSeconds);
		return;
	}

	const float GrowAlpha = Params.GrowTime > 0.f ? FMath::Clamp(Age / Params.GrowTime, 0.f, 1.f) : 1.f;
	const float Scale = FMath::Lerp(knightMonsterTornado::StartScale, 1.f, FMath::SmoothStep(0.f, 1.f, GrowAlpha));
	// 수면 소용돌이 뒤 RiseStart부터 기둥이 아래에서 위로 차오른다(처음엔 빠르게, 끝에서 느리게).
	const float RiseAlpha = FMath::Clamp((GrowAlpha - Params.RiseStart) / FMath::Max(1.f - Params.RiseStart, KINDA_SMALL_NUMBER), 0.f, 1.f);
	const float Rise = 1.f - FMath::Square(1.f - RiseAlpha);
	if (bFunnel && RiseAlpha > 0.f && Params.Effect && !EffectComponent->IsActive())
	{
		EffectComponent->Activate(true);
	}
	if (GrowAlpha >= 1.f)
	{
		if (!bBurst)
		{
			Burst();
		}
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
	UpdateWater(bFunnel ? FMath::Lerp(0.5f, 1.f, GrowAlpha) : Scale, GrowAlpha >= 1.f, DeltaSeconds);
	UpdateVisual(Scale, Rise, 0.f, 1.f, DeltaSeconds);
	if (bFunnel)
	{
		// 판정은 차오른 기둥 높이를 따르고, 반지름은 소용돌이 때 절반에서 시작한다.
		ApplyHits(FMath::Lerp(0.5f, 1.f, GrowAlpha), FMath::Max(Rise, 0.1f));
	}
	else
	{
		ApplyHits(Scale, Scale);
	}
}

UNiagaraComponent* AknightMonsterTornado::SpawnAtBase(UNiagaraSystem* System, bool bAttached)
{
	if (!System)
	{
		return nullptr;
	}
	if (!bAttached)
	{
		return UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), System, GetActorLocation() + FVector(0.f, 0.f, BaseOffset), FRotator::ZeroRotator,
			FVector::OneVector, true, true, ENCPoolMethod::AutoRelease);
	}
	return UNiagaraFunctionLibrary::SpawnSystemAttached(System, Root, NAME_None, FVector(0.f, 0.f, BaseOffset), FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset, true, true, ENCPoolMethod::None);
}

void AknightMonsterTornado::Burst()
{
	bBurst = true;
	if (FormComponent)
	{
		FormComponent->Deactivate();
		FormComponent = nullptr;
	}
	SpawnAtBase(Params.BurstEffect, false);
	if (Params.LoopSound)
	{
		LoopAudio = UGameplayStatics::SpawnSoundAttached(Params.LoopSound, Root, NAME_None, FVector(0.f, 0.f, BaseOffset), EAttachLocation::KeepRelativeOffset);
		if (LoopAudio)
		{
			LoopAudio->FadeIn(Params.LoopFadeIn);
		}
	}
	UWorld* const World = GetWorld();
	const FVector Base = GetActorLocation() + FVector(0.f, 0.f, BaseOffset);
	UknightWaterRippleSubsystem* const Ripples = World ? World->GetSubsystem<UknightWaterRippleSubsystem>() : nullptr;
	if (Ripples && bOverWater && Params.BurstRippleRadius > 0.f)
	{
		Ripples->AddImpulse(Base, Params.BurstRippleRadius, Params.BurstRippleDepth);
	}
	if (!Params.BurstShake || Params.BurstShakeScale <= 0.f || !World)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* const PC = It->Get();
		if (!PC || !PC->IsLocalController() || !PC->PlayerCameraManager)
		{
			continue;
		}
		const float Distance = FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), Base);
		const float Inner = Params.BurstShakeInnerRadius;
		const float Falloff = Distance <= Inner ? 1.f : 1.f - FMath::Clamp((Distance - Inner) / FMath::Max(Params.BurstShakeOuterRadius - Inner, 1.f), 0.f, 1.f);
		if (Falloff > 0.f)
		{
			PC->PlayerCameraManager->StartCameraShake(Params.BurstShake, Params.BurstShakeScale * Falloff);
		}
	}
}

void AknightMonsterTornado::StartCollapse()
{
	if (FormComponent)
	{
		FormComponent->Deactivate();
		FormComponent = nullptr;
	}
	// 새 입자만 멈추고 남은 물보라·물안개는 수명대로 흩어진다.
	EffectComponent->Deactivate();
	if (LoopAudio)
	{
		LoopAudio->FadeOut(Params.FadeTime, 0.f);
		LoopAudio = nullptr;
	}
	if (bFunnel)
	{
		SpawnAtBase(Params.CollapseEffect, false);
	}
}

void AknightMonsterTornado::ApplyHits(float RadiusScale, float HeightScale)
{
	AActor* const Source = Attacker.Get();
	if (!Source)
	{
		return;
	}
	const float Radius = Params.Radius * RadiusScale;
	const float HalfHeight = FMath::Max(Params.Height * HeightScale * 0.5f, Radius);
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

void AknightMonsterTornado::UpdateVisual(float Scale, float Rise, float Lift, float Opacity, float DeltaSeconds)
{
	const float Radius = Params.Radius * Scale;
	const float Height = Params.Height * Scale;
	Spin = FMath::Fmod(Spin + knightMonsterTornado::SpinRate * DeltaSeconds, 360.f);
	EffectSpin = FMath::Fmod(EffectSpin + Params.EffectSpinRate * DeltaSeconds, 360.f);
	if (bFunnel)
	{
		// 깔때기는 돌리지 않는다(도는 무늬는 머티리얼). 아래 끝을 밑동보다 FunnelSink만큼 낮춘다.
		const float Size = knightMonsterTornado::FunnelMeshSize;
		Mesh->SetRelativeScale3D(FVector(Radius / Size, Radius / Size, (Height + Params.FunnelSink) / Size));
		Mesh->SetRelativeLocation(FVector(0.f, 0.f, BaseOffset - Params.FunnelSink));
		for (UMaterialInstanceDynamic* const Instance : { FunnelMaterialInstance.Get(), StreamMaterialInstance.Get() })
		{
			if (Instance)
			{
				Instance->SetScalarParameterValue(TEXT("Rise"), Rise);
				Instance->SetScalarParameterValue(TEXT("Lift"), Lift);
				Instance->SetScalarParameterValue(TEXT("Fade"), Opacity);
			}
		}
		if (StreamMesh)
		{
			const float StreamRadius = Radius * Params.StreamScale / Size;
			StreamMesh->SetRelativeScale3D(FVector(StreamRadius, StreamRadius, (Height + Params.FunnelSink) / Size));
			StreamMesh->SetRelativeLocation(FVector(0.f, 0.f, BaseOffset - Params.FunnelSink));
		}
	}
	else if (Mesh->IsVisible() && Mesh->GetStaticMesh())
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
	// 입자는 로컬 공간이라 컴포넌트를 돌리면 함께 돈다. 깔때기가 있으면 크기는 그대로 두고 기둥이 차오르는 것으로 자란다.
	EffectComponent->SetRelativeLocation(FVector(0.f, 0.f, BaseOffset));
	EffectComponent->SetRelativeRotation(FRotator(0.f, EffectSpin, 0.f));
	EffectComponent->SetRelativeScale3D(FVector(bFunnel ? 1.f : Scale));
	if (FormComponent)
	{
		FormComponent->SetRelativeLocation(FVector(0.f, 0.f, BaseOffset));
		FormComponent->SetRelativeRotation(FRotator(0.f, EffectSpin, 0.f));
	}
}

void AknightMonsterTornado::UpdateWater(float Scale, bool bMoving, float DeltaSeconds)
{
	UWorld* const World = GetWorld();
	const FVector Location = GetActorLocation();
	float SurfaceZ = 0.f;
	// 수면이 바닥 근처나 그보다 위(물에 잠긴 바닥)일 때만 밑동이 수면에 붙는다.
	if (!World || !UknightWaterRippleSubsystem::FindWaterSurface(World, Location, SurfaceZ) || SurfaceZ < Location.Z - 50.f)
	{
		BaseOffset = 0.f;
		bOverWater = false;
		return;
	}
	BaseOffset = SurfaceZ - Location.Z;
	const FVector Point(Location.X, Location.Y, SurfaceZ);
	if (!bOverWater)
	{
		// 수면에 처음 닿으면 바로 물결과 물보라를 낸다.
		WaterTravel = Params.WaterRippleSpacing;
		WaterSplashTravel = Params.WaterSplashSpacing;
	}
	else
	{
		const float Step = FVector::Dist2D(Point, LastWaterPoint);
		WaterTravel += Step;
		WaterSplashTravel += Step;
	}
	bOverWater = true;
	LastWaterPoint = Point;

	UknightWaterRippleSubsystem* const Ripples = World->GetSubsystem<UknightWaterRippleSubsystem>();
	if (Ripples && Params.WaterMaxHeight > 0.f)
	{
		Ripples->BoostMaxHeight(Params.WaterMaxHeight, 2.f);
	}
	const float RippleRadius = Params.WaterRippleRadius * Scale;
	if (Ripples && RippleRadius > 0.f)
	{
		if (bMoving && WaterTravel >= Params.WaterRippleSpacing)
		{
			WaterTravel = 0.f;
			Ripples->AddImpulse(Point, RippleRadius, Params.WaterRippleDepth);
		}
		ChurnTime += DeltaSeconds;
		if (!bMoving && Params.WaterChurnInterval > 0.f && ChurnTime >= Params.WaterChurnInterval)
		{
			// 제자리에서는 같은 자리를 이어 눌러 밑동에서 물결 고리가 연달아 퍼진다.
			ChurnTime = 0.f;
			Ripples->AddImpulse(Point, RippleRadius, Params.WaterChurnDepth);
		}
	}
	if (bMoving && Params.WaterSplashSpacing > 0.f && WaterSplashTravel >= Params.WaterSplashSpacing)
	{
		WaterSplashTravel = 0.f;
		UNiagaraSystem* const Splash = Params.WaterSplash ? Params.WaterSplash.Get() : GetDefault<UknightWaterRippleSettings>()->SplashHeavySystem.LoadSynchronous();
		if (Splash)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Splash, Point, FRotator::ZeroRotator, FVector(Params.WaterSplashScale * Scale), true, true,
				ENCPoolMethod::AutoRelease);
		}
	}
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
