#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JanyeolFireActor.generated.h"
class USkeletalMeshComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UAnimSequence;
class UProceduralMeshComponent;

UCLASS(Blueprintable)
class JANYEOLFIRE_API AJanyeolFireActor : public AActor {
 GENERATED_BODY()
public:
 AJanyeolFireActor();
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Janyeol") TObjectPtr<USkeletalMeshComponent> Creature;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Janyeol") TObjectPtr<UProceduralMeshComponent> BackMantle;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Fire", meta=(ClampMin="0",ClampMax="1")) float Heat=0.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Fire", meta=(ClampMin="0")) float TransitionSpeed=2.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Fire") bool FireEnabled=true;
 // Anchor coordinates are centimeters in the imported mesh reference pose (+Y forward).
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Placement") FVector MouthAnchor=FVector(14,68,22.5);
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Placement",meta=(ClampMin="-10",ClampMax="20")) float BackSurfaceLift=8.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Fire") TObjectPtr<UNiagaraSystem> MouthSystem;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Fire") TObjectPtr<UNiagaraSystem> BackLowSystem;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Fire") TObjectPtr<UNiagaraSystem> BackRageSystem;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Fire") TObjectPtr<UMaterialInterface> FlameMaterial;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Fire") TObjectPtr<UMaterialInterface> MouthSmokeMaterial;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Fire") TObjectPtr<UMaterialInterface> BodyHeatMaterial;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Fire") TObjectPtr<UMaterialInterface> MantleMaterial;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Preview") TObjectPtr<UAnimSequence> PreviewAnimation;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Preview") bool AnimateInEditor=true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Janyeol|Preview",meta=(ClampMin="0",ClampMax="3")) float PlaybackSpeed=1.f;
 UFUNCTION(BlueprintCallable, Category="Janyeol|Fire") void SetFireHeat(float Value);
 UFUNCTION(CallInEditor, Category="Janyeol|Fire") void Calm();
 UFUNCTION(CallInEditor, Category="Janyeol|Fire") void Enraged();
 UFUNCTION(BlueprintCallable, CallInEditor, Category="Janyeol|Preview") void RefreshFire();
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;
 virtual bool ShouldTickIfViewportsOnly() const override {return true;}
#if WITH_EDITOR
 virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif
private:
 UPROPERTY() TArray<TObjectPtr<UNiagaraComponent>> Flames;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> MouthMID;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> MouthRightMID;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> LowMID;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> RageMID;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BodyMID;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> MantleMID;
 float CurrentHeat=0.f;
 void ApplyHeat(float Delta);
 void UpdateMantle(bool Rebuild);
};
