#include "Monster/knightMonsterHoverComponent.h"

#include "Components/CapsuleComponent.h"
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
	SetComponentTickEnabled(Mesh.IsValid() && ((HoverDrop > 0.f && !FootBones.IsEmpty()) || AirLift > 0.f || !BodyBones.IsEmpty()));
}

void UknightMonsterHoverComponent::SetAirborne(bool bInAirborne, float BlendTime)
{
	AirAlphaTarget = bInAirborne ? 1.f : 0.f;
	AirAlphaSpeed = BlendTime > 0.f ? 1.f / BlendTime : 0.f;
	if (AirAlphaSpeed <= 0.f)
	{
		AirAlpha = AirAlphaTarget;
	}
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
	float Drop = 0.f;
	if (HoverDrop > 0.f && !FootBones.IsEmpty())
	{
		float LowestFoot = TNumericLimits<float>::Max();
		for (const FName Bone : FootBones)
		{
			LowestFoot = FMath::Min(LowestFoot, MeshComponent->GetSocketLocation(Bone).Z - MeshOriginZ);
		}
		Drop = FMath::Clamp(LowestFoot - FootGroundHeight, 0.f, HoverDrop);
	}
	if (AirAlpha != AirAlphaTarget)
	{
		AirAlpha = FMath::FInterpConstantTo(AirAlpha, AirAlphaTarget, DeltaTime, AirAlphaSpeed);
	}
	// 부드럽게 오르내리도록 보간 비율에 이즈를 건다.
	const float Air = FMath::SmoothStep(0.f, 1.f, AirAlpha);
	float Height = BaseMeshHeight - Drop * (1.f - Air) + AirLift * Air;
	const ACharacter* const Character = Cast<ACharacter>(GetOwner());
	if (AirAlpha > 0.f && AirCeiling > 0.f && Character && Character->GetCapsuleComponent())
	{
		// 캡슐 원점 기준: 뼈 높이 = Height + (뼈 - 메시 원점) ≤ -캡슐 반높이 + AirCeiling
		const float BoneAboveOrigin = MeshComponent->GetSocketLocation(AirCeilingBone).Z - MeshOriginZ;
		Height = FMath::Min(Height, AirCeiling - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - BoneAboveOrigin);
	}
	if (!BodyBones.IsEmpty() && Character && Character->GetCapsuleComponent())
	{
		// 천장보다 바닥이 먼저다. 캡슐 원점 기준: 뼈 높이 = Height + (뼈 - 메시 원점) ≥ -캡슐 반높이 + BodyClearance
		float LowestBody = TNumericLimits<float>::Max();
		for (const FName Bone : BodyBones)
		{
			LowestBody = FMath::Min(LowestBody, MeshComponent->GetSocketLocation(Bone).Z - MeshOriginZ);
		}
		Height = FMath::Max(Height, BodyClearance - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - LowestBody);
	}

	FVector Location = MeshComponent->GetRelativeLocation();
	if (!FMath::IsNearlyEqual(Location.Z, Height, 0.1f))
	{
		Location.Z = Height;
		MeshComponent->SetRelativeLocation(Location);
	}
}
