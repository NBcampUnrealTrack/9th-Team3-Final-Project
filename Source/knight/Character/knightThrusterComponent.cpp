#include "Character/knightThrusterComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "knight.h"

namespace knightThruster
{
	FknightThrusterNozzle MakeNozzle(const FName Name, const FName Bone, const FVector& Offset, const float Scale)
	{
		FknightThrusterNozzle Nozzle;
		Nozzle.Name = Name;
		Nozzle.Bone = Bone;
		Nozzle.Offset = Offset;
		Nozzle.Scale = Scale;
		return Nozzle;
	}
}

UknightThrusterComponent::UknightThrusterComponent()
{
	// 틱은 디버그 그리기에만 쓴다(UpdateTickEnabled). 뼈 위치를 읽으므로 애니메이션이 끝난 뒤에 그린다.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;

	// 등 양쪽은 예전 구르기·돌진의 등 부스터 위치(가슴뼈에서 뒤로 15, 양옆 12, 위로 4), 등 가운데는 그 사이.
	// 팔꿈치는 아래팔 뼈 시작점(팔꿈치)에서 조금 뒤, 발은 메시의 thruster_l/r 소켓(발 앞꿈치)을 그대로 쓴다.
	using knightThruster::MakeNozzle;
	Nozzles = {
		MakeNozzle(TEXT("Back_C"), TEXT("spine_03"), FVector(-15.f, 0.f, 4.f), 1.f),
		MakeNozzle(TEXT("Back_L"), TEXT("spine_03"), FVector(-15.f, -12.f, 4.f), 1.f),
		MakeNozzle(TEXT("Back_R"), TEXT("spine_03"), FVector(-15.f, 12.f, 4.f), 1.f),
		MakeNozzle(TEXT("Arm_L"), TEXT("lowerarm_l"), FVector(-8.f, 0.f, 0.f), 0.7f),
		MakeNozzle(TEXT("Arm_R"), TEXT("lowerarm_r"), FVector(-8.f, 0.f, 0.f), 0.7f),
		MakeNozzle(TEXT("Foot_L"), TEXT("thruster_l"), FVector::ZeroVector, 1.f),
		MakeNozzle(TEXT("Foot_R"), TEXT("thruster_r"), FVector::ZeroVector, 1.f),
		// 종아리 뒤(정강이 가운데 뼈에서 몸 기준 뒤로 조금). 대쉬공격이 미끄러져 나가는 동안 쓴다.
		MakeNozzle(TEXT("Calf_L"), TEXT("calf_twist_01_l"), FVector(-5.f, 0.f, 0.f), 1.f),
		MakeNozzle(TEXT("Calf_R"), TEXT("calf_twist_01_r"), FVector(-5.f, 0.f, 0.f), 1.f),
	};
}

void UknightThrusterComponent::BeginPlay()
{
	Super::BeginPlay();
	UpdateTickEnabled();
}

int32 UknightThrusterComponent::StartThrust(const FknightThrustSettings& Settings, const FVector& MoveDirection)
{
	USkeletalMeshComponent* const Mesh = GetOwnerMesh();
	UWorld* const World = GetWorld();
	if (!Settings.Effect || Settings.Nozzles.IsEmpty() || !Mesh || !World)
	{
		return 0;
	}

	FActiveThrust Thrust;
	Thrust.Id = NextThrustId++;
	Thrust.EmitDirection = GetEmitDirection(Settings, MoveDirection);
	// 이펙트는 로컬 -Z로 뿜으므로 로컬 Z를 뿜는 방향의 반대로 둔다. 회전은 세계 기준으로 고정한다(몸이 돌아도 뿜는 방향은 그대로).
	const FRotator Rotation = FRotationMatrix::MakeFromZ(-Thrust.EmitDirection).Rotator();

	for (const FName NozzleName : Settings.Nozzles)
	{
		const FknightThrusterNozzle* const Nozzle = FindNozzle(NozzleName);
		if (!Nozzle || !Mesh->DoesSocketExist(Nozzle->Bone))
		{
			UE_LOG(LogknightAbility, Warning, TEXT("Thruster: nozzle %s skipped (missing nozzle or bone)"), *NozzleName.ToString());
			continue;
		}
		// 몸 기준 위치를 지금 자세의 뼈 기준 위치로 바꿔 붙인다. 이후에는 뼈를 따라 움직인다.
		const FTransform BoneTransform = Mesh->GetSocketTransform(Nozzle->Bone);
		const FVector RelativeLocation = BoneTransform.InverseTransformPosition(GetNozzleWorldLocation(*Mesh, *Nozzle));
		UNiagaraComponent* const Jet = UNiagaraFunctionLibrary::SpawnSystemAttached(Settings.Effect, Mesh, Nozzle->Bone,
			RelativeLocation, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
		if (!Jet)
		{
			continue;
		}
		Jet->SetUsingAbsoluteRotation(true);
		Jet->SetWorldRotation(Rotation);
		Jet->SetWorldScale3D(FVector(Settings.Scale * Nozzle->Scale));
		Thrust.Jets.Add(Jet);
	}
	if (Thrust.Jets.IsEmpty())
	{
		return 0;
	}

	if (Settings.Duration > 0.f)
	{
		World->GetTimerManager().SetTimer(Thrust.StopTimer, FTimerDelegate::CreateUObject(this, &ThisClass::StopThrust, Thrust.Id),
			Settings.Duration, false);
	}
	if (bDebugDrawThrusts)
	{
		UE_LOG(LogknightAbility, Log, TEXT("Thruster: start #%d, %d jets, emit %s, scale %.2f, duration %.2f"),
			Thrust.Id, Thrust.Jets.Num(), *Thrust.EmitDirection.ToCompactString(), Settings.Scale, Settings.Duration);
	}
	const int32 Id = Thrust.Id;
	ActiveThrusts.Add(MoveTemp(Thrust));
	UpdateTickEnabled();
	return Id;
}

void UknightThrusterComponent::StopThrust(int32 ThrustId)
{
	const int32 Index = ActiveThrusts.IndexOfByPredicate([ThrustId](const FActiveThrust& Thrust) { return Thrust.Id == ThrustId; });
	if (Index == INDEX_NONE)
	{
		return;
	}
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActiveThrusts[Index].StopTimer);
	}
	for (const TWeakObjectPtr<UNiagaraComponent>& Jet : ActiveThrusts[Index].Jets)
	{
		if (Jet.IsValid())
		{
			// 이미 나온 불꽃은 끝까지 보이고, 다 사라지면 컴포넌트가 스스로 지워진다.
			Jet->Deactivate();
		}
	}
	ActiveThrusts.RemoveAt(Index);
	UpdateTickEnabled();
}

void UknightThrusterComponent::StopAllThrusts()
{
	while (!ActiveThrusts.IsEmpty())
	{
		StopThrust(ActiveThrusts.Last().Id);
	}
}

void UknightThrusterComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const USkeletalMeshComponent* const Mesh = GetOwnerMesh();
	UWorld* const World = GetWorld();
	if (!Mesh || !World)
	{
		return;
	}
	if (bDebugShowNozzles)
	{
		for (const FknightThrusterNozzle& Nozzle : Nozzles)
		{
			if (Mesh->DoesSocketExist(Nozzle.Bone))
			{
				DrawDebugSphere(World, GetNozzleWorldLocation(*Mesh, Nozzle), 5.f * Nozzle.Scale, 8, FColor::Yellow, false, 0.f, SDPG_Foreground);
			}
		}
	}
	if (bDebugDrawThrusts)
	{
		for (const FActiveThrust& Thrust : ActiveThrusts)
		{
			for (const TWeakObjectPtr<UNiagaraComponent>& Jet : Thrust.Jets)
			{
				if (Jet.IsValid())
				{
					const FVector Location = Jet->GetComponentLocation();
					DrawDebugSphere(World, Location, 6.f, 8, FColor::Green, false, 0.f, SDPG_Foreground);
					DrawDebugLine(World, Location, Location + Thrust.EmitDirection * 80.f, FColor::Red, false, 0.f, SDPG_Foreground, 2.f);
				}
			}
		}
	}
}

void UknightThrusterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAllThrusts();
	Super::EndPlay(EndPlayReason);
}

const FknightThrusterNozzle* UknightThrusterComponent::FindNozzle(FName Name) const
{
	return Nozzles.FindByPredicate([Name](const FknightThrusterNozzle& Nozzle) { return Nozzle.Name == Name; });
}

USkeletalMeshComponent* UknightThrusterComponent::GetOwnerMesh() const
{
	const ACharacter* const Character = Cast<ACharacter>(GetOwner());
	return Character ? Character->GetMesh() : nullptr;
}

FVector UknightThrusterComponent::GetNozzleWorldLocation(const USkeletalMeshComponent& Mesh, const FknightThrusterNozzle& Nozzle) const
{
	const AActor* const Owner = GetOwner();
	const FQuat ActorQuat = Owner ? Owner->GetActorQuat() : FQuat::Identity;
	return Mesh.GetSocketLocation(Nozzle.Bone) + ActorQuat.RotateVector(Nozzle.Offset);
}

FVector UknightThrusterComponent::GetEmitDirection(const FknightThrustSettings& Settings, const FVector& MoveDirection) const
{
	const AActor* const Owner = GetOwner();
	const FVector ActorBack = Owner ? -Owner->GetActorForwardVector() : -FVector::ForwardVector;
	FVector Back;
	switch (Settings.Direction)
	{
	case EknightThrustDirection::Down:
		return -FVector::UpVector;
	case EknightThrustDirection::Up:
		return FVector::UpVector;
	case EknightThrustDirection::OppositeMove:
		Back = MoveDirection.IsNearlyZero() ? ActorBack : -MoveDirection;
		break;
	case EknightThrustDirection::ActorBack:
	default:
		Back = ActorBack;
		break;
	}
	const float Down = FMath::DegreesToRadians(Settings.DownAngle);
	return (Back.GetSafeNormal2D() * FMath::Cos(Down) - FVector::UpVector * FMath::Sin(Down)).GetSafeNormal();
}

void UknightThrusterComponent::UpdateTickEnabled()
{
	SetComponentTickEnabled(bDebugShowNozzles || (bDebugDrawThrusts && !ActiveThrusts.IsEmpty()));
}
