#include "Environment/KnightStormBarrier.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "knightCharacter.h"

AKnightStormBarrier::AKnightStormBarrier()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	WallCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("WallCollision"));
	WallCollision->SetupAttachment(SceneRoot);
	WallCollision->InitBoxExtent(FVector(500.0, 30.0, 350.0));
	WallCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	WallCollision->SetCollisionObjectType(ECC_WorldStatic);
	WallCollision->SetCollisionResponseToAllChannels(ECR_Block);
	WallCollision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	WallCollision->SetGenerateOverlapEvents(false);
	WallCollision->CanCharacterStepUpOn = ECB_No;

	DetectionCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectionCollision"));
	DetectionCollision->SetupAttachment(SceneRoot);
	DetectionCollision->InitBoxExtent(FVector(550.0, 220.0, 380.0));
	DetectionCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DetectionCollision->SetCollisionObjectType(ECC_WorldDynamic);
	DetectionCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	DetectionCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	DetectionCollision->SetGenerateOverlapEvents(true);
	DetectionCollision->SetCanEverAffectNavigation(false);
	DetectionCollision->OnComponentBeginOverlap.AddDynamic(this, &AKnightStormBarrier::OnDetectionBeginOverlap);
	DetectionCollision->OnComponentEndOverlap.AddDynamic(this, &AKnightStormBarrier::OnDetectionEndOverlap);

}

void AKnightStormBarrier::BeginPlay()
{
	Super::BeginPlay();

	// Only change the referenced actor in the game world, never in construction/editor preview.
	if (IsValid(TargetStormActor.Get()))
	{
		TargetStormActor->SetDestroyOnSystemFinish(false);
		if (UNiagaraComponent* StormComponent = GetTargetStormComponent())
		{
			StormComponent->SetAutoActivate(false);
			StormComponent->SetAutoDestroy(false);
			StormComponent->DeactivateImmediate();
			StormComponent->SetVisibility(false);
		}
	}

	// A placed actor may already contain the player when play starts.
	bStormEnabled = false;
	DetectionCollision->UpdateOverlaps();
	RefreshStormState();
}

bool AKnightStormBarrier::IsControlledKnightCapsule(const AActor* OtherActor, const UPrimitiveComponent* OtherComp) const
{
	const AknightCharacter* Knight = Cast<AknightCharacter>(OtherActor);
	if (!Knight || OtherComp != Knight->GetCapsuleComponent())
	{
		return false;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(Knight->GetController());
	return PlayerController && PlayerController->GetPawn() == Knight;
}

UNiagaraComponent* AKnightStormBarrier::GetTargetStormComponent() const
{
	if (!IsValid(TargetStormActor.Get()))
	{
		return nullptr;
	}

	UNiagaraComponent* StormComponent = TargetStormActor->GetNiagaraComponent();
	return IsValid(StormComponent) ? StormComponent : nullptr;
}

void AKnightStormBarrier::OnDetectionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (IsControlledKnightCapsule(OtherActor, OtherComp))
	{
		SetStormEnabled(true);
	}
}

void AKnightStormBarrier::OnDetectionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// Keep the storm on while any controlled Knight capsule is still inside.
	RefreshStormState();
}

void AKnightStormBarrier::RefreshStormState()
{
	TArray<UPrimitiveComponent*> OverlappingComponents;
	DetectionCollision->GetOverlappingComponents(OverlappingComponents);
	for (const UPrimitiveComponent* Component : OverlappingComponents)
	{
		if (Component && IsControlledKnightCapsule(Component->GetOwner(), Component))
		{
			SetStormEnabled(true);
			return;
		}
	}

	SetStormEnabled(false);
}

void AKnightStormBarrier::SetStormEnabled(bool bEnable)
{
	if (bEnable)
	{
		if (bStormEnabled)
		{
			return;
		}

		UNiagaraComponent* StormComponent = GetTargetStormComponent();
		if (!StormComponent || !StormComponent->GetAsset())
		{
			return;
		}

		bStormEnabled = true;
		StormComponent->SetVisibility(true);
		StormComponent->Activate(true);
	}
	else
	{
		if (!bStormEnabled)
		{
			return;
		}

		bStormEnabled = false;
		if (UNiagaraComponent* StormComponent = GetTargetStormComponent())
		{
			StormComponent->DeactivateImmediate();
			StormComponent->SetVisibility(false);
		}
	}
}
