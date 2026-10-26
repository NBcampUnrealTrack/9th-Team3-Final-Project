#include "Environment/KnightSandstorm.h"
#include "Components/BoxComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

namespace
{
	// Transient per-world ownership. A second placed controller cannot overwrite the same fog.
	TMap<TWeakObjectPtr<AExponentialHeightFog>, TWeakObjectPtr<AKnightSandstorm>> FogOwners;
}

AKnightSandstorm::AKnightSandstorm()
{
	PrimaryActorTick.bCanEverTick = false;
	Coverage = CreateDefaultSubobject<UBoxComponent>(TEXT("DustCoverage"));
	SetRootComponent(Coverage);
	Coverage->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Coverage->SetGenerateOverlapEvents(false);
	Coverage->SetCanEverAffectNavigation(false);
	Coverage->SetHiddenInGame(true);
	Coverage->SetBoxExtent(CoverageExtent);
	Dust = CreateDefaultSubobject<UNiagaraComponent>(TEXT("WindDust"));
	Dust->SetupAttachment(Coverage);
	Dust->SetAutoActivate(false);
	Dust->SetAutoDestroy(false);
	Dust->SetCastShadow(false);
	Dust->SetCanEverAffectNavigation(false);
}

void AKnightSandstorm::CaptureFog(UExponentialHeightFogComponent* F)
{
	BaselineFog = TargetFog;
	Baseline.bValid = true;
	Baseline.Density = F->FogDensity; Baseline.Falloff = F->FogHeightFalloff;
	Baseline.Tint = F->FogInscatteringLuminance; Baseline.Start = F->StartDistance;
	Baseline.End = F->EndDistance; Baseline.Cutoff = F->FogCutoffDistance; Baseline.MaxOpacity = F->FogMaxOpacity;
	Baseline.bVolumetric = F->bEnableVolumetricFog; Baseline.VolStart = F->VolumetricFogStartDistance;
	Baseline.VolFade = F->VolumetricFogNearFadeInDistance; Baseline.VolDistance = F->VolumetricFogDistance;
	Baseline.VolExtinction = F->VolumetricFogExtinctionScale; Baseline.VolAlbedo = F->VolumetricFogAlbedo;
}

void AKnightSandstorm::ReleaseFog()
{
	if (!BaselineFog || !Baseline.bValid) return;
	const auto* FogOwner = FogOwners.Find(BaselineFog);
	if (FogOwner && FogOwner->IsValid() && FogOwner->Get() != this) return;
	if (UExponentialHeightFogComponent* F = BaselineFog->GetComponent())
	{
		F->SetFogDensity(Baseline.Density); F->SetFogHeightFalloff(Baseline.Falloff);
		F->SetFogInscatteringColor(Baseline.Tint); F->SetStartDistance(Baseline.Start);
		F->SetEndDistance(Baseline.End); F->SetFogCutoffDistance(Baseline.Cutoff); F->SetFogMaxOpacity(Baseline.MaxOpacity);
		F->SetVolumetricFog(Baseline.bVolumetric); F->SetVolumetricFogStartDistance(Baseline.VolStart);
		F->SetVolumetricFogNearFadeInDistance(Baseline.VolFade); F->SetVolumetricFogDistance(Baseline.VolDistance);
		F->SetVolumetricFogExtinctionScale(Baseline.VolExtinction); F->SetVolumetricFogAlbedo(Baseline.VolAlbedo);
	}
	FogOwners.Remove(BaselineFog);
}

void AKnightSandstorm::RestoreOriginalFog()
{
	bEnabled = false;
	ReleaseFog();
	if (Dust) { Dust->DeactivateImmediate(); Dust->SetVisibility(false); }
}

void AKnightSandstorm::ApplySettings()
{
	if (bApplying || IsTemplate() || !GetWorld() || !Coverage || !Dust) return;
	TGuardValue<bool> Guard(bApplying, true);
	if (BaselineFog != TargetFog)
	{
		ReleaseFog(); Baseline = FKnightSandstormFogBaseline(); BaselineFog = nullptr;
	}
	const bool Active = bEnabled && Intensity > 0;
	if (Active && TargetFog)
	{
		auto* FogOwner = FogOwners.Find(TargetFog);
		if (FogOwner && FogOwner->IsValid() && FogOwner->Get() != this)
		{
			UE_LOG(LogTemp, Warning, TEXT("Sandstorm %s: target fog already owned by %s; controller not applied"), *GetName(), *FogOwner->Get()->GetName());
			Dust->DeactivateImmediate(); return;
		}
		if (UExponentialHeightFogComponent* F = TargetFog->GetComponent())
		{
			if (!Baseline.bValid) CaptureFog(F);
			FogOwners.Add(TargetFog, this);
			F->SetFogDensity(FMath::Clamp(FogDensity * Intensity * 8000.f / FMath::Max(1000.f,HazeTransitionDistance),0.f,1.f));
			F->SetFogHeightFalloff(FMath::Clamp(FogHeightFalloff,.001f,2.f));
			F->SetFogInscatteringColor(FogTint);
			F->SetStartDistance(FMath::Max(0.f,NearClearDistance));
			F->SetEndDistance(0.f); F->SetFogCutoffDistance(0.f); F->SetFogMaxOpacity(1.f);
			F->SetVolumetricFog(bUseVolumetricFog);
			F->SetVolumetricFogStartDistance(FMath::Max(0.f,VolumetricStartDistance));
			F->SetVolumetricFogNearFadeInDistance(FMath::Max(1.f,VolumetricNearFadeDistance));
			F->SetVolumetricFogDistance(FMath::Max(1000.f,VolumetricViewDistance));
			F->SetVolumetricFogExtinctionScale(1.f);
			F->SetVolumetricFogAlbedo(FogTint.ToFColor(true));
		}
	}
	else ReleaseFog();
	// One persistent component, no per-frame spawn/rebuild. Loop-rate bindings sample once per second.
	const FVector Extent = CoverageExtent.GetAbs().BoundToBox(FVector(100),FVector(50000));
	Coverage->SetBoxExtent(Extent);
	if (Dust->GetAsset() != DustSystem) { Dust->DeactivateImmediate(); Dust->SetAsset(DustSystem); }
	if (DustMaterial && (!DustMID || DustMID->Parent != DustMaterial)) DustMID = UMaterialInstanceDynamic::Create(DustMaterial,this);
	if (DustMID)
	{
		DustMID->SetVectorParameterValue(TEXT("BaseColor_Tint"),FogTint);
		DustMID->SetScalarParameterValue(TEXT("GustStrength"),FMath::Clamp(GustStrength,0.f,1.f));
		Dust->SetVariableMaterial(TEXT("User.DustMaterial"),DustMID);
	}
	const float Amount = FMath::Clamp(DustAmount * Intensity,0.f,2.f) * (bLightDust ? .35f : 1.f);
	Dust->SetVariableFloat(TEXT("User.FineRate"),FMath::Min(240.f,160.f*Amount));
	Dust->SetVariableFloat(TEXT("User.WispRate"),FMath::Min(20.f,12.f*Amount));
	Dust->SetVariableVec3(TEXT("User.BoxSize"),Extent*2);
	Dust->SetVariableVec3(TEXT("User.WindVelocity"),GetActorTransform().InverseTransformVectorNoScale(WindDirection.GetSafeNormal()*FMath::Clamp(WindSpeed,0.f,1200.f)));
	Dust->SetVariableVec2(TEXT("User.SizeScale"),FVector2D(FMath::Clamp(DustSize,.25f,3.f)));
	// Includes six seconds of maximum wind travel + turbulence + largest sprite; independent of camera frustum.
	const FVector Bounds = Extent + FVector(10000);
	Dust->SetSystemFixedBounds(FBox(-Bounds,Bounds));
	Dust->SetVisibility(Active);
	if (Active && Amount > 0 && DustSystem && DustMaterial)
	{
		if (!Dust->IsActive()) Dust->Activate(true);
	}
	else Dust->DeactivateImmediate();
}

void AKnightSandstorm::SetSandstormEnabled(bool Value) { bEnabled=Value; ApplySettings(); }
void AKnightSandstorm::OnConstruction(const FTransform& T) { Super::OnConstruction(T); ApplySettings(); }
void AKnightSandstorm::PostRegisterAllComponents() { Super::PostRegisterAllComponents(); ApplySettings(); }
void AKnightSandstorm::BeginPlay() { Super::BeginPlay(); ApplySettings(); }
void AKnightSandstorm::EndPlay(const EEndPlayReason::Type R) { ReleaseFog(); Dust->DeactivateImmediate(); Super::EndPlay(R); }
void AKnightSandstorm::Destroyed() { ReleaseFog(); Super::Destroyed(); }
