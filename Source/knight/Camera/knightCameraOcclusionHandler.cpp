#include "Camera/knightCameraOcclusionHandler.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Interfaces/TargetableInterface.h"

FVector UknightCameraOcclusionHandler::ResolveCameraLocation(const AActor* ViewOwner, const FVector& Pivot, const FVector& DesiredLocation, float DeltaTime)
{
	UWorld* const World = ViewOwner ? ViewOwner->GetWorld() : nullptr;
	if (!World)
	{
		return DesiredLocation;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(knightCameraOcclusion), false, ViewOwner);
	TArray<AActor*> Attached;
	ViewOwner->GetAttachedActors(Attached);
	Params.AddIgnoredActors(Attached);

	float BlockedFraction = 1.f;
	TArray<FHitResult> Hits;
	World->SweepMultiByChannel(Hits, Pivot, DesiredLocation, FQuat::Identity, ProbeChannel, FCollisionShape::MakeSphere(ProbeRadius), Params);
	for (const FHitResult& Hit : Hits)
	{
		if (!Hit.bBlockingHit)
		{
			continue;
		}
		if (bIgnoreTargetableActors && Cast<ITargetableInterface>(Hit.GetActor()))
		{
			continue;
		}
		BlockedFraction = FMath::Min(BlockedFraction, Hit.Time);
	}

	// A multi sweep stops at the first blocking hit. If that was an ignored monster, sweep again past it.
	if (BlockedFraction >= 1.f && Hits.Num() > 0 && Hits.Last().bBlockingHit && bIgnoreTargetableActors)
	{
		FCollisionQueryParams RetryParams = Params;
		for (const FHitResult& Hit : Hits)
		{
			if (Hit.bBlockingHit && Cast<ITargetableInterface>(Hit.GetActor()))
			{
				RetryParams.AddIgnoredActor(Hit.GetActor());
			}
		}
		FHitResult Retry;
		if (World->SweepSingleByChannel(Retry, Pivot, DesiredLocation, FQuat::Identity, ProbeChannel, FCollisionShape::MakeSphere(ProbeRadius), RetryParams))
		{
			BlockedFraction = Retry.Time;
		}
	}

	CurrentFraction = BlockedFraction < CurrentFraction
		? BlockedFraction
		: FMath::FInterpTo(CurrentFraction, BlockedFraction, DeltaTime, RecoverySpeed);
	FVector ResolvedLocation = FMath::Lerp(Pivot, DesiredLocation, CurrentFraction);
	ResolvedLocation.Z = FMath::Max(ResolvedLocation.Z, ViewOwner->GetActorLocation().Z - 8.f);
	return ResolvedLocation;
}
