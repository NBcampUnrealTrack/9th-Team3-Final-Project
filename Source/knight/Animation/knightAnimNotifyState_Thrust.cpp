#include "Animation/knightAnimNotifyState_Thrust.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UknightAnimNotifyState_Thrust::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	AActor* const Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	UknightThrusterComponent* const Thrusters = Owner ? Owner->FindComponentByClass<UknightThrusterComponent>() : nullptr;
	if (!Thrusters)
	{
		return;
	}
	// 앞 구간이 끝나기 전에 다시 시작했으면 앞 분사를 먼저 끈다.
	if (const int32* const OldId = ActiveThrustIds.Find(MeshComp))
	{
		Thrusters->StopThrust(*OldId);
	}
	const FVector Velocity = Owner->GetVelocity().GetSafeNormal2D();
	const FVector MoveDirection = Velocity.IsNearlyZero() ? Owner->GetActorForwardVector() : Velocity;
	const int32 ThrustId = Thrusters->StartThrust(Thrust, MoveDirection);
	if (ThrustId != 0)
	{
		ActiveThrustIds.Add(MeshComp, ThrustId);
	}
}

void UknightAnimNotifyState_Thrust::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	int32 ThrustId = 0;
	if (!ActiveThrustIds.RemoveAndCopyValue(MeshComp, ThrustId))
	{
		return;
	}
	const AActor* const Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (UknightThrusterComponent* const Thrusters = Owner ? Owner->FindComponentByClass<UknightThrusterComponent>() : nullptr)
	{
		Thrusters->StopThrust(ThrustId);
	}
}

FString UknightAnimNotifyState_Thrust::GetNotifyName_Implementation() const
{
	return TEXT("Knight Thrust");
}
