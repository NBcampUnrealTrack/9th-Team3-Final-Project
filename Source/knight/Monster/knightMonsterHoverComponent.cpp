#include "Monster/knightMonsterHoverComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "knight.h"
#include "knightPerfCounters.h"

UknightMonsterHoverComponent::UknightMonsterHoverComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 이번 프레임 포즈가 나온 뒤에 높이를 정한다.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UknightMonsterHoverComponent::BeginPlay()
{
	Super::BeginPlay();

	const ACharacter* const Character = Cast<ACharacter>(GetOwner());
	Mesh = Character ? Character->GetMesh() : nullptr;
	if (Mesh.IsValid())
	{
		BaseMeshHeight = Mesh->GetRelativeLocation().Z;
	}
	SetComponentTickEnabled(Mesh.IsValid() && HoverDrop > 0.f && !FootBones.IsEmpty());
}

void UknightMonsterHoverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	KNIGHT_PERF_SCOPE("Monster.Hover");

	USkeletalMeshComponent* const MeshComponent = Mesh.Get();
	if (!MeshComponent)
	{
		return;
	}
	// 메시 원점(애니메이션의 바닥) 기준 발 높이. 메시를 옮겨도 이 값은 바뀌지 않는다.
	const float MeshOriginZ = MeshComponent->GetComponentLocation().Z;
	float LowestFoot = TNumericLimits<float>::Max();
	for (const FName Bone : FootBones)
	{
		LowestFoot = FMath::Min(LowestFoot, MeshComponent->GetSocketLocation(Bone).Z - MeshOriginZ);
	}
	const float Drop = FMath::Clamp(LowestFoot - FootGroundHeight, 0.f, HoverDrop);

	FVector Location = MeshComponent->GetRelativeLocation();
	if (!FMath::IsNearlyEqual(Location.Z, BaseMeshHeight - Drop, 0.1f))
	{
		Location.Z = BaseMeshHeight - Drop;
		MeshComponent->SetRelativeLocation(Location);
	}
}
