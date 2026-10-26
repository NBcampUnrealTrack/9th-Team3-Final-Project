#include "Combat/knightGhostTrailActor.h"

#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

AknightGhostTrailActor::AknightGhostTrailActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	SetActorEnableCollision(false);
	bReplicates = false;

	USceneComponent* const Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

AknightGhostTrailActor* AknightGhostTrailActor::SpawnGhostFromCharacter(
	const ACharacter* Character,
	UMaterialInterface* GhostMaterial,
	const FLinearColor& Color,
	const float InLifetime,
	const float InInitialOpacity)
{
	if (!Character || !Character->GetWorld())
	{
		return nullptr;
	}

	UMaterialInterface* EffectiveMat = GhostMaterial;
	if (!EffectiveMat)
	{
		EffectiveMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Knight/Abilities/Cues/M_Knight_GhostTrail.M_Knight_GhostTrail"));
	}
	if (!EffectiveMat)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	AknightGhostTrailActor* const Ghost = Character->GetWorld()->SpawnActor<AknightGhostTrailActor>(
		AknightGhostTrailActor::StaticClass(), Character->GetActorTransform(), SpawnParams);
	if (Ghost)
	{
		Ghost->CaptureCharacterMeshes(*Character, EffectiveMat, Color, InLifetime, InInitialOpacity);
	}
	return Ghost;
}

void AknightGhostTrailActor::CaptureCharacterMeshes(
	const ACharacter& Character,
	UMaterialInterface* BaseMaterial,
	const FLinearColor& Color,
	const float InLifetime,
	const float InInitialOpacity)
{
	Lifetime = FMath::Max(InLifetime, 0.05f);
	InitialOpacity = FMath::Clamp(InInitialOpacity, 0.05f, 1.f);
	ElapsedTime = 0.f;

	GhostMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	if (!GhostMID)
	{
		Destroy();
		return;
	}
	GhostMID->SetVectorParameterValue(TEXT("GhostColor"), Color);
	GhostMID->SetScalarParameterValue(TEXT("GhostOpacity"), InitialOpacity);

	TArray<UMeshComponent*> MeshComponents;
	Character.GetComponents<UMeshComponent>(MeshComponents);

	int32 CopiedCount = 0;
	for (UMeshComponent* const MeshComp : MeshComponents)
	{
		if (!MeshComp || !MeshComp->IsVisible() || MeshComp->bHiddenInGame)
		{
			continue;
		}

		if (USkeletalMeshComponent* const SkelComp = Cast<USkeletalMeshComponent>(MeshComp))
		{
			USkeletalMesh* const SkelAsset = SkelComp->GetSkeletalMeshAsset();
			if (!SkelAsset)
			{
				continue;
			}
			UPoseableMeshComponent* const PoseComp = NewObject<UPoseableMeshComponent>(this);
			PoseComp->SetupAttachment(GetRootComponent());
			PoseComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PoseComp->SetCastShadow(false);
			PoseComp->bReceivesDecals = false;
			PoseComp->SetSkinnedAssetAndUpdate(SkelAsset);
			PoseComp->RegisterComponent();
			PoseComp->SetWorldTransform(SkelComp->GetComponentTransform());
			PoseComp->CopyPoseFromSkeletalComponent(SkelComp);

			const int32 NumMats = PoseComp->GetNumMaterials();
			for (int32 MatIdx = 0; MatIdx < NumMats; ++MatIdx)
			{
				PoseComp->SetMaterial(MatIdx, GhostMID);
			}
			++CopiedCount;
		}
		else if (UStaticMeshComponent* const StatComp = Cast<UStaticMeshComponent>(MeshComp))
		{
			UStaticMesh* const StatAsset = StatComp->GetStaticMesh();
			if (!StatAsset)
			{
				continue;
			}
			UStaticMeshComponent* const GhostStatComp = NewObject<UStaticMeshComponent>(this);
			GhostStatComp->SetupAttachment(GetRootComponent());
			GhostStatComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			GhostStatComp->SetCastShadow(false);
			GhostStatComp->bReceivesDecals = false;
			GhostStatComp->SetStaticMesh(StatAsset);
			GhostStatComp->RegisterComponent();
			GhostStatComp->SetWorldTransform(StatComp->GetComponentTransform());

			const int32 NumMats = GhostStatComp->GetNumMaterials();
			for (int32 MatIdx = 0; MatIdx < NumMats; ++MatIdx)
			{
				GhostStatComp->SetMaterial(MatIdx, GhostMID);
			}
			++CopiedCount;
		}
	}

	if (CopiedCount == 0)
	{
		Destroy();
	}
}

void AknightGhostTrailActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ElapsedTime += DeltaSeconds;
	if (ElapsedTime >= Lifetime || !GhostMID)
	{
		Destroy();
		return;
	}

	const float RemainingRatio = FMath::Clamp(1.f - (ElapsedTime / Lifetime), 0.f, 1.f);
	GhostMID->SetScalarParameterValue(TEXT("GhostOpacity"), InitialOpacity * RemainingRatio * RemainingRatio);
}
