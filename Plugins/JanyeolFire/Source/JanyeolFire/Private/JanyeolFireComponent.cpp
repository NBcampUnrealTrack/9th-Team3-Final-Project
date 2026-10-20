#include "JanyeolFireComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "ProceduralMeshComponent.h"

UJanyeolFireComponent::UJanyeolFireComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    // Read the completed skeletal pose, including physics, instead of the previous frame.
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
    bTickInEditor = true;
}

USkeletalMeshComponent* UJanyeolFireComponent::ResolveMesh() const
{
    AActor* Owner = GetOwner();
    if (!Owner) return nullptr;
    if (!TargetMeshComponentName.IsNone())
    {
        TInlineComponentArray<USkeletalMeshComponent*> Meshes(Owner);
        for (USkeletalMeshComponent* Mesh : Meshes)
            if (Mesh->GetFName() == TargetMeshComponentName) return Mesh;
        return nullptr;
    }
    if (const ACharacter* Character = Cast<ACharacter>(Owner)) return Character->GetMesh();
    return Owner->FindComponentByClass<USkeletalMeshComponent>();
}

bool UJanyeolFireComponent::CanCreateEffects() const
{
    const UWorld* World = GetWorld();
    return !IsTemplate() && GetOwner() && !GetOwner()->IsTemplate() && World &&
        World->GetNetMode() != NM_DedicatedServer &&
        (World->IsGameWorld() || PreviewInEditor);
}

USkeletalMeshComponent* UJanyeolFireComponent::GetTargetMesh() const { return TargetMesh; }
bool UJanyeolFireComponent::IsFireReady() const
{
    const int32 ExpectedCount = (MouthSystem && MouthSmokeMaterial) ? 16 : 14;
    return IsValid(TargetMesh) && IsValid(BackMantle) && Flames.Num() == ExpectedCount;
}

void UJanyeolFireComponent::OnRegister()
{
    Super::OnRegister();
    RefreshFire();
}

void UJanyeolFireComponent::BeginPlay()
{
    Super::BeginPlay();
    // Blueprint construction can assign the mesh after component registration.
    if (!IsFireReady()) RefreshFire();
}

void UJanyeolFireComponent::OnUnregister()
{
    DestroyEffects();
    Super::OnUnregister();
}

#if WITH_EDITOR
void UJanyeolFireComponent::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    // Super may reconstruct and replace this component; OnRegister initializes the replacement.
    DestroyEffects();
    Super::PostEditChangeProperty(Event);
}
#endif

void UJanyeolFireComponent::DestroyEffects()
{
    if (IsValid(TargetMesh))
    {
        RemoveTickPrerequisiteComponent(TargetMesh);
        // Do not overwrite a material subsequently assigned by another gameplay effect.
        if (BodyMID && TargetMesh->GetMaterial(0) == BodyMID)
            TargetMesh->SetMaterial(0, OriginalBodyMaterial);
    }
    for (UNiagaraComponent* Flame : Flames)
        if (IsValid(Flame)) Flame->DestroyComponent();
    Flames.Reset();
    if (IsValid(BackMantle)) BackMantle->DestroyComponent();
    BackMantle = nullptr;
    TargetMesh = nullptr;
    MouthMID = MouthRightMID = LowMID = RageMID = BodyMID = MantleMID = nullptr;
    OriginalBodyMaterial = nullptr;
}

void UJanyeolFireComponent::RefreshFire()
{
    DestroyEffects();
    if (!CanCreateEffects()) return;
    USkeletalMeshComponent* Mesh = ResolveMesh();
    if (!IsValid(Mesh) || !Mesh->GetSkeletalMeshAsset() || !BackLowSystem ||
        !BackRageSystem || !FlameMaterial || !MantleMaterial) return;
    const bool bUseMouth = MouthSystem && MouthSmokeMaterial;
    const FReferenceSkeleton& Ref = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
    if (bUseMouth && Ref.FindBoneIndex(TEXT("head")) == INDEX_NONE) return;
    for (const TCHAR* Bone : {TEXT("chest"), TEXT("spine_02"), TEXT("spine_01"), TEXT("pelvis"), TEXT("tail_01")})
        if (Ref.FindBoneIndex(FName(Bone)) == INDEX_NONE) return;
    TargetMesh = Mesh;
    AddTickPrerequisiteComponent(Mesh);

    if (bUseMouth)
    {
        MouthMID = UMaterialInstanceDynamic::Create(MouthSmokeMaterial, this);
        MouthRightMID = UMaterialInstanceDynamic::Create(MouthSmokeMaterial, this);
        MouthMID->SetScalarParameterValue(TEXT("SmokePhase"), 0.f);
        MouthMID->SetScalarParameterValue(TEXT("WaveAmount"), .8f);
        MouthRightMID->SetScalarParameterValue(TEXT("SmokePhase"), 1.7f);
        MouthRightMID->SetScalarParameterValue(TEXT("WaveAmount"), 1.f);
    }
    LowMID = UMaterialInstanceDynamic::Create(FlameMaterial, this);
    RageMID = UMaterialInstanceDynamic::Create(FlameMaterial, this);
    MantleMID = UMaterialInstanceDynamic::Create(MantleMaterial, this);
    if (ApplyBodyHeatMaterial && BodyHeatMaterial)
    {
        OriginalBodyMaterial = Mesh->GetMaterial(0);
        BodyMID = UMaterialInstanceDynamic::Create(BodyHeatMaterial, this);
        Mesh->SetMaterial(0, BodyMID);
    }
    BackMantle = NewObject<UProceduralMeshComponent>(GetOwner(), NAME_None, RF_Transient | RF_DuplicateTransient);
    BackMantle->SetupAttachment(Mesh);
    BackMantle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BackMantle->SetCastShadow(false);
    BackMantle->SetMaterial(0, MantleMID);
    BackMantle->RegisterComponent();

    const TCHAR* Bones[] = {TEXT("chest"), TEXT("chest"), TEXT("spine_02"), TEXT("spine_02"), TEXT("spine_01"), TEXT("pelvis"), TEXT("tail_01")};
    const FVector Points[] = {FVector(-8,31,66), FVector(8,31,66), FVector(-7,12,61), FVector(7,12,61), FVector(0,-8,54), FVector(0,-27,46), FVector(0,-39,39)};
    for (int32 I = bUseMouth ? 0 : 2; I < 16; ++I)
    {
        const int32 BackIndex = (I - 2) % 7;
        const FName Bone(I < 2 ? TEXT("head") : Bones[BackIndex]);
        const FVector Position = I < 2 ? FVector(I == 0 ? -MouthAnchor.X : MouthAnchor.X, MouthAnchor.Y, MouthAnchor.Z) : Points[BackIndex] + FVector(0,0,BackSurfaceLift);
        const int32 B = Ref.FindBoneIndex(Bone);
        FTransform Rest = Ref.GetRefBonePose()[B];
        for (int32 P = Ref.GetParentIndex(B); P != INDEX_NONE; P = Ref.GetParentIndex(P)) Rest = Rest * Ref.GetRefBonePose()[P];
        UNiagaraComponent* Flame = NewObject<UNiagaraComponent>(GetOwner(), NAME_None, RF_Transient | RF_DuplicateTransient);
        Flames.Add(Flame);
        Flame->SetupAttachment(Mesh, Bone);
        Flame->SetRelativeLocation(Rest.InverseTransformPosition(Position));
        Flame->SetRelativeRotation(Rest.GetRotation().Inverse());
        Flame->SetAutoActivate(false);
        Flame->bTickInEditor = true;
        Flame->SetAsset(I < 2 ? MouthSystem : (I < 9 ? BackLowSystem : BackRageSystem));
        Flame->SetVariableMaterial(TEXT("User.FireMaterial"), I == 0 ? MouthMID : (I == 1 ? MouthRightMID : (I < 9 ? LowMID : RageMID)));
        Flame->SetVariableFloat(TEXT("User.Seed"), float(I) * 13.37f);
        if (I < 2) Flame->SetVariableVec3(TEXT("User.Flow"), I == 0 ? FVector(-10.8,-18,18) : FVector(11.5,-19,19));
        Flame->RegisterComponent();
        Flame->SetVisibility(FireEnabled);
        if (FireEnabled) Flame->Activate(true);
    }
    CurrentHeat = FMath::Clamp(Heat, 0.f, 1.f);
    CurrentStrength = FMath::Clamp(Strength, 0.f, 1.f);
    LastFireEnabled = FireEnabled;
    ApplyHeat(0.f);
    UpdateMantle(true);
}

void UJanyeolFireComponent::SetFireHeat(float Value) { Heat = FMath::Clamp(Value, 0.f, 1.f); }
void UJanyeolFireComponent::SetFireStrength(float Value) { Strength = FMath::Clamp(Value, 0.f, 1.f); }
void UJanyeolFireComponent::SetFireEnabled(bool Enabled)
{
    FireEnabled = Enabled;
    for (UNiagaraComponent* Flame : Flames)
    {
        if (!IsValid(Flame)) continue;
        Flame->SetVisibility(Enabled);
        if (Enabled && !Flame->IsActive()) Flame->Activate(true);
        else if (!Enabled) Flame->DeactivateImmediate();
    }
    if (BackMantle) BackMantle->SetVisibility(Enabled);
    LastFireEnabled = Enabled;
    ApplyHeat(0.f);
}

void UJanyeolFireComponent::ApplyHeat(float DeltaTime)
{
    CurrentHeat = FMath::FInterpTo(CurrentHeat, FMath::Clamp(Heat, 0.f, 1.f), DeltaTime, TransitionSpeed);
    CurrentStrength = FMath::FInterpTo(CurrentStrength, FMath::Clamp(Strength, 0.f, 1.f), DeltaTime, TransitionSpeed);
    const float Enabled = FireEnabled ? CurrentStrength : 0.f;
    if (MouthMID) MouthMID->SetScalarParameterValue(TEXT("FireStrength"), Enabled * (.75f + CurrentHeat * .4f));
    if (MouthRightMID) MouthRightMID->SetScalarParameterValue(TEXT("FireStrength"), Enabled * (.72f + CurrentHeat * .4f));
    if (LowMID) LowMID->SetScalarParameterValue(TEXT("FireStrength"), Enabled * (.28f + CurrentHeat * .12f));
    if (RageMID) RageMID->SetScalarParameterValue(TEXT("FireStrength"), Enabled * CurrentHeat * .32f);
    if (BodyMID) BodyMID->SetScalarParameterValue(TEXT("Heat"), Enabled * (.8f + CurrentHeat * 1.8f));
    if (MantleMID)
    {
        MantleMID->SetScalarParameterValue(TEXT("Heat"), CurrentHeat);
        MantleMID->SetScalarParameterValue(TEXT("FireStrength"), Enabled);
    }
    if (BackMantle) BackMantle->SetVisibility(FireEnabled);
}

void UJanyeolFireComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime, TickType, TickFunction);
    if (!CanCreateEffects())
    {
        if (IsFireReady()) DestroyEffects();
        return;
    }
    if (!IsFireReady() || ResolveMesh() != TargetMesh) RefreshFire();
    if (!IsFireReady()) return;
    if (FireEnabled != LastFireEnabled) SetFireEnabled(FireEnabled);
    ApplyHeat(DeltaTime);
    if (FireEnabled) UpdateMantle(false);
}

void UJanyeolFireComponent::UpdateMantle(bool Rebuild)
{
    if (!IsValid(TargetMesh) || !TargetMesh->GetSkeletalMeshAsset() || !BackMantle) return;
    constexpr int Rows = 33, Cols = 25;
    const TCHAR* Names[] = {TEXT("chest"), TEXT("spine_02"), TEXT("spine_01"), TEXT("pelvis"), TEXT("tail_01")};
    const float Ys[] = {40,12,-10,-31,-65}, Zs[] = {73,67,61,51,34}, Ws[] = {22,28,28,23,9};
    const auto& Ref = TargetMesh->GetSkeletalMeshAsset()->GetRefSkeleton();
    FTransform Rest[5], Pose[5];
    for (int K = 0; K < 5; ++K)
    {
        const int B = Ref.FindBoneIndex(FName(Names[K]));
        if (B == INDEX_NONE) return;
        Rest[K] = Ref.GetRefBonePose()[B];
        for (int P = Ref.GetParentIndex(B); P != INDEX_NONE; P = Ref.GetParentIndex(P)) Rest[K] = Rest[K] * Ref.GetRefBonePose()[P];
        Pose[K] = TargetMesh->GetSocketTransform(FName(Names[K]), RTS_Component);
    }
    TArray<FVector> V, N;
    TArray<FVector2D> UV;
    TArray<int32> Tris;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;
    V.Reserve(Rows * Cols); N.Reserve(Rows * Cols); UV.Reserve(Rows * Cols);
    const float Time = GetWorld()->GetTimeSeconds();
    for (int R = 0; R < Rows; ++R)
    {
        const float Along = float(R) / (Rows - 1), Y = FMath::Lerp(Ys[0], Ys[4], Along);
        int K = 0; while (K < 3 && Y < Ys[K + 1]) ++K;
        const float A = FMath::Clamp((Ys[K] - Y) / (Ys[K] - Ys[K + 1]), 0.f, 1.f);
        const float Top = FMath::Lerp(Zs[K], Zs[K + 1], A), Width = FMath::Lerp(Ws[K], Ws[K + 1], A);
        for (int C = 0; C < Cols; ++C)
        {
            const float U = float(C) / (Cols - 1), S = U * 2 - 1;
            const float Envelope = FMath::Sin(Along * PI);
            const float Billow = (5.f + 12.f * CurrentHeat) * Envelope * (.65f + .35f * FMath::Sin(Along * 11 - Time * 2.2f + S * 2));
            const FVector P(S * (Width + Billow * .24f), Y, Top - 17 * S * S + Billow);
            V.Add(FMath::Lerp(Pose[K].TransformPosition(Rest[K].InverseTransformPosition(P)), Pose[K + 1].TransformPosition(Rest[K + 1].InverseTransformPosition(P)), A));
            N.Add(FVector(S * .65f, 0, 1).GetSafeNormal()); UV.Add(FVector2D(U, Along));
        }
    }
    if (Rebuild || !BackMantle->GetProcMeshSection(0))
    {
        for (int R = 0; R < Rows - 1; ++R) for (int C = 0; C < Cols - 1; ++C)
        {
            const int I = R * Cols + C;
            Tris.Append({I, I + Cols, I + 1, I + 1, I + Cols, I + Cols + 1});
        }
        BackMantle->CreateMeshSection_LinearColor(0, V, Tris, N, UV, Colors, Tangents, false);
    }
    else BackMantle->UpdateMeshSection_LinearColor(0, V, N, UV, Colors, Tangents);
}
