#include "Monster/knightMonsterBreakPiece.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

AknightMonsterBreakPiece::AknightMonsterBreakPiece()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = MeshComponent;
	// 바닥과 벽에만 부딪힌다. 거대한 조각에 플레이어가 끼이거나 카메라가 걸리지 않게 나머지는 모두 통과한다.
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetCollisionObjectType(ECC_PhysicsBody);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	MeshComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	MeshComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	MeshComponent->SetGenerateOverlapEvents(false);
	MeshComponent->SetCanEverAffectNavigation(false);
}

void AknightMonsterBreakPiece::Launch(UStaticMesh* Mesh, const FVector& Velocity, const FVector& OutwardDirection)
{
	MeshComponent->SetStaticMesh(Mesh);
	Materials.Reset();
	for (int32 Index = 0; Index < MeshComponent->GetNumMaterials(); ++Index)
	{
		Materials.Add(MeshComponent->CreateAndSetMaterialInstanceDynamic(Index));
	}
	SetMaterialParameter(HeatParameter, 1.f);
	SetMaterialParameter(FadeParameter, 0.f);

	MeshComponent->SetSimulatePhysics(true);
	const FVector Outward = OutwardDirection.GetSafeNormal2D();
	MeshComponent->SetPhysicsLinearVelocity(Velocity + Outward * LaunchSpeed + FVector::UpVector * LaunchUpSpeed);
	MeshComponent->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * FMath::FRandRange(0.5f, 1.f) * MaxSpinSpeed);

	Age = 0.f;
	SetLifeSpan(LifeTime + FadeTime);
}

void AknightMonsterBreakPiece::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	SetMaterialParameter(HeatParameter, 1.f - FMath::Clamp(Age / HeatCoolTime, 0.f, 1.f));
	SetMaterialParameter(FadeParameter, FMath::Clamp((Age - LifeTime) / FadeTime, 0.f, 1.f));
}

void AknightMonsterBreakPiece::SetMaterialParameter(FName Parameter, float Value)
{
	for (UMaterialInstanceDynamic* const Material : Materials)
	{
		if (Material)
		{
			Material->SetScalarParameterValue(Parameter, Value);
		}
	}
}
