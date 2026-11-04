#include "Animation/knightAnimNotifyState_ContactImpact.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/knightAnimNotify_GroundImpact.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Environment/knightWaterRippleSubsystem.h"
#include "TimerManager.h"

void UknightAnimNotifyState_ContactImpact::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	Triggered.Add(MeshComp, false);
	if (IsTouching(MeshComp))
	{
		Trigger(MeshComp, Animation);
	}
}

void UknightAnimNotifyState_ContactImpact::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	if (!Triggered.FindRef(MeshComp) && IsTouching(MeshComp))
	{
		Trigger(MeshComp, Animation);
	}
}

void UknightAnimNotifyState_ContactImpact::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (!Triggered.FindRef(MeshComp))
	{
		Trigger(MeshComp, Animation);
	}
	Triggered.Remove(MeshComp);
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UknightAnimNotifyState_ContactImpact::GetNotifyName_Implementation() const
{
	return TEXT("Contact Impact");
}

bool UknightAnimNotifyState_ContactImpact::IsTouching(const USkeletalMeshComponent* MeshComp) const
{
	const UWorld* const World = MeshComp ? MeshComp->GetWorld() : nullptr;
	if (!World || MeshComp->GetNumBones() < 2)
	{
		return false;
	}
	// 루트는 메시 원점(공중 높이를 메시 오프셋으로 내는 몬스터는 몸보다 훨씬 아래)에 있으므로 뺀다.
	FVector Lowest(0.f, 0.f, UE_BIG_NUMBER);
	for (int32 Index = 1; Index < MeshComp->GetNumBones(); ++Index)
	{
		const FVector Location = MeshComp->GetBoneLocation(MeshComp->GetBoneName(Index));
		if (Location.Z < Lowest.Z)
		{
			Lowest = Location;
		}
	}
	float SurfaceZ = 0.f;
	if (UknightWaterRippleSubsystem::FindWaterSurface(World, Lowest, SurfaceZ) && Lowest.Z - SurfaceZ <= MaxGroundDistance)
	{
		return Lowest.Z - SurfaceZ <= ContactHeight;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(knightContactImpact), false, MeshComp->GetOwner());
	return World->LineTraceSingleByObjectType(Hit, Lowest + FVector(0.f, 0.f, 300.f), Lowest - FVector(0.f, 0.f, MaxGroundDistance),
		FCollisionObjectQueryParams(ECC_WorldStatic), Params) && Lowest.Z - Hit.ImpactPoint.Z <= ContactHeight;
}

void UknightAnimNotifyState_ContactImpact::Trigger(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	if (!MeshComp)
	{
		return;
	}
	Triggered.Add(MeshComp, true);
	const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MeshComp->GetOwner());
	if (ASC && ASC->HasAnyMatchingGameplayTags(BlockedTags))
	{
		return;
	}
	UWorld* const World = MeshComp->GetWorld();
	for (const FknightContactImpactStep& Step : Steps)
	{
		if (!Step.Impact)
		{
			continue;
		}
		if (Step.Delay <= 0.f || !World)
		{
			Step.Impact->Notify(MeshComp, Animation, FAnimNotifyEventReference());
			continue;
		}
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Step.Impact.Get(),
			[Impact = Step.Impact, WeakMesh = TWeakObjectPtr<USkeletalMeshComponent>(MeshComp), WeakAnimation = TWeakObjectPtr<UAnimSequenceBase>(Animation)]
			{
				if (USkeletalMeshComponent* const Mesh = WeakMesh.Get())
				{
					Impact->Notify(Mesh, WeakAnimation.Get(), FAnimNotifyEventReference());
				}
			}), Step.Delay, false);
	}
}
