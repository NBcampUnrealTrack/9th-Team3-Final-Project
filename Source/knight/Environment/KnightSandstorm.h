#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KnightSandstorm.generated.h"

class AExponentialHeightFog;
class UExponentialHeightFogComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UBoxComponent;

/** Only properties owned by the storm; the level's lighting/exposure is never changed. */
USTRUCT()
struct FKnightSandstormFogBaseline
{
	GENERATED_BODY()
	UPROPERTY() bool bValid = false;
	UPROPERTY() float Density = 0;
	UPROPERTY() float Falloff = 0;
	UPROPERTY() FLinearColor Tint = FLinearColor::Black;
	UPROPERTY() float Start = 0;
	UPROPERTY() float End = 0;
	UPROPERTY() float Cutoff = 0;
	UPROPERTY() float MaxOpacity = 1;
	UPROPERTY() bool bVolumetric = false;
	UPROPERTY() float VolStart = 0;
	UPROPERTY() float VolFade = 0;
	UPROPERTY() float VolDistance = 0;
	UPROPERTY() float VolExtinction = 1;
	UPROPERTY() FColor VolAlbedo = FColor::White;
};

/** One controller for an explicitly assigned level fog. Dust is bounded; height fog remains world-wide. */
UCLASS(Blueprintable)
class KNIGHT_API AKnightSandstorm : public AActor
{
	GENERATED_BODY()
public:
	AKnightSandstorm();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sandstorm") bool bEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sandstorm", meta=(ClampMin=0, ClampMax=2)) float Intensity = 1.f;
	/** Assign the existing fog. Do not assign the same fog to multiple storm actors. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Sandstorm|Haze") TObjectPtr<AExponentialHeightFog> TargetFog;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sandstorm|Haze", meta=(ClampMin=0, Units="cm")) float NearClearDistance = 1500.f;
	/** Broad optical transition, not a hard visibility cutoff. Scales the density relative to 8000 cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sandstorm|Haze", meta=(ClampMin=1000, Units="cm")) float HazeTransitionDistance = 8000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sandstorm|Haze", meta=(ClampMin=0, ClampMax=1)) float FogDensity = .6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sandstorm|Haze") FLinearColor FogTint = FLinearColor(.4375f, .42426f, .359892f);
	/** A shallow height falloff keeps ground-level combat consistent despite the original fog actor's altitude. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sandstorm|Haze", meta=(ClampMin=.001, ClampMax=2)) float FogHeightFalloff = .001f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sandstorm|Volumetric") bool bUseVolumetricFog = true;
	/** Volumetric controls are independent of ordinary height fog StartDistance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sandstorm|Volumetric", meta=(ClampMin=0, Units="cm")) float VolumetricStartDistance = 1500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sandstorm|Volumetric", meta=(ClampMin=1, Units="cm")) float VolumetricNearFadeDistance = 6000.f;
	/** Grid integration distance, NOT distance at which objects disappear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sandstorm|Volumetric", meta=(ClampMin=1000, Units="cm")) float VolumetricViewDistance = 20000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sandstorm|Dust") FVector WindDirection = FVector(1.f, .25f, 0.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sandstorm|Dust", meta=(ClampMin=0, ClampMax=1200, Units="cm/s")) float WindSpeed = 350.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sandstorm|Dust", meta=(ClampMin=0, ClampMax=2)) float DustAmount = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sandstorm|Dust", meta=(ClampMin=.25, ClampMax=3)) float DustSize = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sandstorm|Dust", meta=(ClampMin=0, ClampMax=1)) float GustStrength = .25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sandstorm|Dust", meta=(ClampMin=100, ClampMax=50000, Units="cm")) FVector CoverageExtent = FVector(10000,10000,2000);
	/** 35% spawn rate; distance haze and all project quality CVars are unchanged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sandstorm|Scalability") bool bLightDust = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sandstorm|Assets") TObjectPtr<UNiagaraSystem> DustSystem;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sandstorm|Assets") TObjectPtr<UMaterialInterface> DustMaterial;

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Sandstorm") void ApplySettings();
	UFUNCTION(BlueprintCallable, Category="Sandstorm") void SetSandstormEnabled(bool bNewEnabled);
	/** Restores the captured level fog and stops dust. Also called on removal / EndPlay. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Sandstorm") void RestoreOriginalFog();
	UFUNCTION(BlueprintPure, Category="Sandstorm") UNiagaraComponent* GetDustComponent() const { return Dust; }
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostRegisterAllComponents() override;
	virtual void Destroyed() override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	void ReleaseFog();
	void CaptureFog(UExponentialHeightFogComponent* Fog);
	UPROPERTY(VisibleAnywhere, Category="Sandstorm|Components") TObjectPtr<UBoxComponent> Coverage;
	UPROPERTY(VisibleAnywhere, Category="Sandstorm|Components") TObjectPtr<UNiagaraComponent> Dust;
	UPROPERTY() TObjectPtr<AExponentialHeightFog> BaselineFog;
	UPROPERTY() FKnightSandstormFogBaseline Baseline;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> DustMID;
	bool bApplying = false;
};
