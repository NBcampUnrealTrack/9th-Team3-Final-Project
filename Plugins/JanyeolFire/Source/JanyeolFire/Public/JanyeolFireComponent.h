#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JanyeolFireComponent.generated.h"

class USkeletalMeshComponent;
class UProceduralMeshComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** Cosmetic effects driven by the owner's existing mesh. Does not own or play animations. */
UCLASS(ClassGroup=(Janyeol), meta=(BlueprintSpawnableComponent))
class JANYEOLFIRE_API UJanyeolFireComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UJanyeolFireComponent();

    /** None selects ACharacter::GetMesh(), or the first skeletal mesh on another actor. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Placement")
    FName TargetMeshComponentName = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Placement")
    FVector MouthAnchor = FVector(14, 68, 22.5);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Placement", meta=(ClampMin="-10", ClampMax="20"))
    float BackSurfaceLift = 8.f;

    /** Manual control only. Gameplay rage/death tags are deliberately not bound. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Fire", meta=(ClampMin="0", ClampMax="1"))
    float Heat = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Fire", meta=(ClampMin="0"))
    float TransitionSpeed = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Fire")
    bool FireEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Fire")
    bool ApplyBodyHeatMaterial = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Preview")
    bool PreviewInEditor = true;

    /** Optional. None skips both mouth emitters and their dynamic materials; back fire remains enabled. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Assets") TObjectPtr<UNiagaraSystem> MouthSystem;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Assets") TObjectPtr<UNiagaraSystem> BackLowSystem;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Assets") TObjectPtr<UNiagaraSystem> BackRageSystem;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Assets") TObjectPtr<UMaterialInterface> FlameMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Assets") TObjectPtr<UMaterialInterface> MouthSmokeMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Assets") TObjectPtr<UMaterialInterface> BodyHeatMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Assets") TObjectPtr<UMaterialInterface> MantleMaterial;

    UFUNCTION(BlueprintCallable, Category="Janyeol|Fire") void SetFireHeat(float Value);
    UFUNCTION(BlueprintCallable, Category="Janyeol|Fire") void SetFireEnabled(bool Enabled);
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Janyeol|Fire") void RefreshFire();
    UFUNCTION(BlueprintPure, Category="Janyeol|Fire") bool IsFireReady() const;
    UFUNCTION(BlueprintPure, Category="Janyeol|Fire") USkeletalMeshComponent* GetTargetMesh() const;
    UFUNCTION(BlueprintPure, Category="Janyeol|Fire") float GetCurrentHeat() const { return CurrentHeat; }

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

protected:
    virtual void OnRegister() override;
    virtual void OnUnregister() override;
    virtual void BeginPlay() override;
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif

private:
    USkeletalMeshComponent* ResolveMesh() const;
    bool CanCreateEffects() const;
    void DestroyEffects();
    void ApplyHeat(float DeltaTime);
    void UpdateMantle(bool Rebuild);

    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> TargetMesh;
    UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> BackMantle;
    UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraComponent>> Flames;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> MouthMID;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> MouthRightMID;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> LowMID;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> RageMID;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BodyMID;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> MantleMID;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> OriginalBodyMaterial;
    float CurrentHeat = 0.f;
    bool LastFireEnabled = false;
};
