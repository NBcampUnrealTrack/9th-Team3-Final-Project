#include "Environment/KnightBreakableActor.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraFunctionLibrary.h"
#include "knightCharacter.h"

AKnightBreakableActor::AKnightBreakableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(SceneRoot);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	VisualMesh->SetCollisionObjectType(ECC_WorldStatic);
	VisualMesh->SetCollisionResponseToAllChannels(ECR_Block);

	ContactBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ContactBox"));
	ContactBox->SetupAttachment(SceneRoot);
	ContactBox->InitBoxExtent(FVector(100.0, 100.0, 100.0));
	ContactBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ContactBox->SetCollisionObjectType(ECC_WorldDynamic);
	ContactBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	ContactBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ContactBox->SetGenerateOverlapEvents(true);
	ContactBox->OnComponentBeginOverlap.AddDynamic(this, &AKnightBreakableActor::OnContactBeginOverlap);
}

void AKnightBreakableActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (!bAutoFitContactBox || !VisualMesh || !ContactBox || !VisualMesh->GetStaticMesh())
	{
		return;
	}

	// Both components share SceneRoot. Transform the mesh asset bounds into that space.
	const FBox MeshBounds = VisualMesh->GetStaticMesh()->GetBoundingBox().TransformBy(VisualMesh->GetRelativeTransform());
	const FVector SafePadding(
		FMath::Max(0.0, ContactPadding.X),
		FMath::Max(0.0, ContactPadding.Y),
		FMath::Max(0.0, ContactPadding.Z));

	ContactBox->SetRelativeLocation(MeshBounds.GetCenter());
	ContactBox->SetRelativeRotation(FRotator::ZeroRotator);
	ContactBox->SetRelativeScale3D(FVector::OneVector);
	ContactBox->SetBoxExtent(MeshBounds.GetExtent() + SafePadding);
}

void AKnightBreakableActor::OnContactBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const AknightCharacter* Knight = Cast<AknightCharacter>(OtherActor);
	if (!Knight || OtherComp != Knight->GetCapsuleComponent())
	{
		return;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(Knight->GetController());
	if (PlayerController && PlayerController->GetPawn() == Knight)
	{
		BreakObject();
	}
}

void AKnightBreakableActor::BreakObject()
{
	if (bIsBroken)
	{
		return;
	}

	bIsBroken = true;
	ContactBox->SetGenerateOverlapEvents(false);
	ContactBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetVisibility(false, true);

	if (DustEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), DustEffect, GetActorTransform().TransformPosition(DustOffset),
			GetActorRotation(), DustScale, true, true, ENCPoolMethod::None, false);
	}

	Destroy();
}
