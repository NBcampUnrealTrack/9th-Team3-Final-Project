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
	// BlendTime은 지상↔공중 전체를 옮기는 시간이라, 중간 높이에서 시작하면 남은 만큼만 걸린다.
	const float Target = bInAirborne ? 1.f : 0.f;
	SetAirAlpha(Target, BlendTime * FMath::Abs(Target - AirAlpha));
}

void UknightMonsterHoverComponent::SetAirAlpha(float Target, float BlendTime)
{
	AirAlphaTarget = FMath::Clamp(Target, 0.f, 1.f);
	AirAlphaSpeed = BlendTime > 0.f ? FMath::Abs(AirAlphaTarget - AirAlpha) / BlendTime : 0.f;
	if (AirAlphaSpeed <= 0.f)
	{
		AirAlpha = AirAlphaTarget;
	}
}

void UknightMonsterHoverComponent::FallToGround(float GravityAccel)
{
	const USkeletalMeshComponent* const MeshComponent = Mesh.Get();
	if (!MeshComponent || GravityAccel <= 0.f)
	{
		return;
	}
	FallStartLowest = MeshComponent->GetRelativeLocation().Z + GetLowestFallBone(*MeshComponent, MeshComponent->GetComponentLocation().Z);
	FallElapsed = 0.f;
	FallGravity = GravityAccel;
	bFalling = true;
	const ACharacter* const Owner = Cast<ACharacter>(GetOwner());
	const float Floor = Owner && Owner->GetCapsuleComponent() ? -Owner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
	FallDuration = FMath::Sqrt(2.f * FMath::Max(FallStartLowest - Floor, 0.f) / GravityAccel);
	SetComponentTickEnabled(true);
}

float UknightMonsterHoverComponent::GetLowestFallBone(const USkeletalMeshComponent& MeshComponent, float MeshOriginZ) const
{
	const TArray<FName>& Bones = !BodyBones.IsEmpty() ? BodyBones : FootBones;
	float Lowest = TNumericLimits<float>::Max();
	if (!Bones.IsEmpty())
	{
		for (const FName Bone : Bones)
		{
			Lowest = FMath::Min(Lowest, MeshComponent.GetSocketLocation(Bone).Z - MeshOriginZ);
		}
		return Lowest;
	}
	for (int32 Index = 0; Index < MeshComponent.GetNumBones(); ++Index)
	{
		Lowest = FMath::Min(Lowest, MeshComponent.GetSocketLocation(MeshComponent.GetBoneName(Index)).Z - MeshOriginZ);
	}
	return Lowest == TNumericLimits<float>::Max() ? 0.f : Lowest;
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

	if (bFalling)
	{
		// 이번 프레임 자세로 내려앉을 높이(Natural)보다 중력으로 떨어진 높이(Desired)가 높으면 그만큼 받친다.
		FallElapsed += DeltaTime;
		const float Desired = FallStartLowest - 0.5f * FallGravity * FMath::Square(FallElapsed);
		const float Natural = Height + GetLowestFallBone(*MeshComponent, MeshOriginZ);
		if (Desired > Natural)
		{
			Height += Desired - Natural;
		}
		// 사망 애니메이션이 잠깐 몸을 들어 올려도 계속 떨어진다. 바닥(캡슐 밑면)까지 떨어지면 끝난다.
		const ACharacter* const Owner = Cast<ACharacter>(GetOwner());
		const float Floor = Owner && Owner->GetCapsuleComponent() ? -Owner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
		if (Desired <= Floor)
		{
			bFalling = false;
		}
	}

	FVector Location = MeshComponent->GetRelativeLocation();
	LastHeightDelta = Height - Location.Z;
	if (!FMath::IsNearlyEqual(Location.Z, Height, 0.1f))
	{
		Location.Z = Height;
		MeshComponent->SetRelativeLocation(Location);
	}
}
