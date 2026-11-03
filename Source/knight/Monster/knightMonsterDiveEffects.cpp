#include "Monster/knightMonsterDiveEffects.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Environment/knightWaterRippleSubsystem.h"
#include "GameFramework/Character.h"
#include "Monster/knightAttackTrailComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

namespace
{
	/** 머리 아래 바닥을 찾는 거리. */
	constexpr float SurfaceTraceLength = 5000.f;

	FVector GetSocketOrActor(const ACharacter& Character, FName Socket)
	{
		const USkeletalMeshComponent* const Mesh = Character.GetMesh();
		return Mesh && Mesh->DoesSocketExist(Socket) ? Mesh->GetSocketLocation(Socket) : Character.GetActorLocation();
	}

	void Deactivate(TWeakObjectPtr<UNiagaraComponent>& Component)
	{
		if (UNiagaraComponent* const Niagara = Component.Get())
		{
			Niagara->Deactivate();
		}
		Component.Reset();
	}
}

void FknightMonsterDiveEffects::StartFold(ACharacter& Character)
{
	Deactivate(Fold);
	USkeletalMeshComponent* const Mesh = Character.GetMesh();
	if (!FoldEffect || !Mesh)
	{
		return;
	}
	UNiagaraComponent* const Component = UNiagaraFunctionLibrary::SpawnSystemAttached(FoldEffect, Mesh, FoldSocket, FVector::ZeroVector, FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget, true, true, ENCPoolMethod::AutoRelease);
	if (Component)
	{
		// 몸 스케일을 받지 않고 cm 그대로 쓴다.
		Component->SetUsingAbsoluteScale(true);
		Component->SetWorldScale3D(FVector::OneVector);
	}
	Fold = Component;
}

void FknightMonsterDiveEffects::StartDive(ACharacter& Character, const FVector& Direction)
{
	Deactivate(Fold);
	StopDiveEffects();
	DiveDirection = Direction.GetSafeNormal(UE_SMALL_NUMBER, Character.GetActorForwardVector());
	RippleTimer = 0.f;
	UWorld* const World = Character.GetWorld();
	USceneComponent* const Root = Character.GetRootComponent();
	if (!World || !Root)
	{
		return;
	}
	const FRotator Rotation = DiveDirection.Rotation();
	if (LaunchEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, LaunchEffect, GetSocketOrActor(Character, LaunchSocket), Rotation, FVector::OneVector, true, true,
			ENCPoolMethod::AutoRelease);
	}
	if (BowEffect)
	{
		Bow = UNiagaraFunctionLibrary::SpawnSystemAttached(BowEffect, Root, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset, true, true, ENCPoolMethod::AutoRelease);
	}
	if (StreakEffect)
	{
		Streak = UNiagaraFunctionLibrary::SpawnSystemAttached(StreakEffect, Root, NAME_None, StreakOffset, FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset, true, true, ENCPoolMethod::AutoRelease);
	}
	if (SkimEffect)
	{
		// 수면에 닿을 때만 켠다. 위치는 매 프레임 머리 아래 수면으로 옮긴다.
		Skim = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, SkimEffect, Character.GetActorLocation(), Rotation, FVector::OneVector, false, false,
			ENCPoolMethod::None);
	}
	for (const FknightDiveTrail& Trail : Trails)
	{
		UknightAttackTrailComponent::FSettings Settings;
		Settings.BaseSocket = Trail.BaseSocket;
		Settings.TipSocket = Trail.TipSocket;
		Settings.BaseRatio = Trail.BaseRatio;
		Settings.TipExtension = Trail.TipExtension;
		Settings.Lifetime = Trail.Lifetime;
		Settings.FadeTime = Trail.FadeIn;
		ActiveTrails.Add(UknightAttackTrailComponent::SpawnOn(Character.GetMesh(), Trail.Material, Settings));
	}
	Tick(Character, 0.f);
}

void FknightMonsterDiveEffects::Tick(ACharacter& Character, float DeltaTime)
{
	// 내려오는 동안 기울어진 실제 진행 방향을 따른다.
	const FVector Velocity = Character.GetVelocity();
	const FVector Direction = Velocity.SizeSquared() > 1.f ? Velocity.GetSafeNormal() : DiveDirection;
	const FVector Head = GetSocketOrActor(Character, BowSocket);
	if (UNiagaraComponent* const Component = Bow.Get())
	{
		Component->SetWorldLocationAndRotation(Head + Direction * BowForward, Direction.Rotation());
	}

	UNiagaraComponent* const SkimComponent = Skim.Get();
	if (!SkimComponent)
	{
		return;
	}
	FVector Surface;
	bool bWater = false;
	if (FindSurfaceBelow(Character, Head, Surface, bWater) && bWater && Head.Z - Surface.Z <= SkimHeight)
	{
		SkimComponent->SetWorldLocationAndRotation(Surface, FRotator(0.f, Direction.Rotation().Yaw, 0.f));
		if (!SkimComponent->IsActive())
		{
			SkimComponent->Activate(true);
		}
		RippleTimer -= DeltaTime;
		UknightWaterRippleSubsystem* const Ripples = Character.GetWorld()->GetSubsystem<UknightWaterRippleSubsystem>();
		if (Ripples && SkimRippleRadius > 0.f && RippleTimer <= 0.f)
		{
			RippleTimer = SkimRippleInterval;
			Ripples->AddImpulse(Surface, SkimRippleRadius, SkimRippleDepth);
		}
	}
	else if (SkimComponent->IsActive())
	{
		SkimComponent->Deactivate();
	}
}

void FknightMonsterDiveEffects::FinishDive(ACharacter& Character)
{
	StopDiveEffects();
	UWorld* const World = Character.GetWorld();
	if (!ImpactEffect || !World)
	{
		return;
	}
	const FVector Head = GetSocketOrActor(Character, BowSocket);
	FVector Surface;
	bool bWater = false;
	if (FindSurfaceBelow(Character, Head, Surface, bWater) && Head.Z - Surface.Z <= ImpactMaxHeight)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, ImpactEffect, Surface, FRotator(-90.f, DiveDirection.Rotation().Yaw, 0.f), FVector::OneVector,
			true, true, ENCPoolMethod::AutoRelease);
	}
}

void FknightMonsterDiveEffects::Stop()
{
	Deactivate(Fold);
	StopDiveEffects();
}

void FknightMonsterDiveEffects::StopDiveEffects()
{
	Deactivate(Bow);
	Deactivate(Streak);
	if (UNiagaraComponent* const Component = Skim.Get())
	{
		// 남은 물보라는 수명까지 떨어지고 사라진다.
		Component->SetAutoDestroy(true);
		Component->Deactivate();
	}
	Skim.Reset();
	for (const TWeakObjectPtr<UknightAttackTrailComponent>& Trail : ActiveTrails)
	{
		if (Trail.IsValid())
		{
			Trail->Stop();
		}
	}
	ActiveTrails.Reset();
}

bool FknightMonsterDiveEffects::FindSurfaceBelow(const ACharacter& Character, const FVector& From, FVector& OutSurface, bool& bOutWater) const
{
	const UWorld* const World = Character.GetWorld();
	if (!World)
	{
		return false;
	}
	bool bFound = false;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(knightDiveSurface), false, &Character);
	if (World->LineTraceSingleByChannel(Hit, From, From - FVector(0.f, 0.f, SurfaceTraceLength), ECC_Visibility, Params))
	{
		OutSurface = Hit.ImpactPoint;
		bFound = true;
	}
	bOutWater = false;
	float WaterZ = 0.f;
	if (UknightWaterRippleSubsystem::FindWaterSurface(World, From, WaterZ) && WaterZ <= From.Z && (!bFound || WaterZ >= OutSurface.Z))
	{
		OutSurface = FVector(From.X, From.Y, WaterZ);
		bOutWater = true;
		bFound = true;
	}
	return bFound;
}
