#include "Camera/knightCameraOcclusionHandler.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Interfaces/TargetableInterface.h"
#include "Environment/knightWaterRippleSubsystem.h"
#include "WaterBodyActor.h"

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
		if (bIgnoreWaterInSweep && Cast<AWaterBody>(Hit.GetActor()))
		{
			continue;
		}
		BlockedFraction = FMath::Min(BlockedFraction, Hit.Time);
	}

	// A multi sweep stops at the first blocking hit. If that was an ignored monster (or water), sweep again past it.
	if (BlockedFraction >= 1.f && Hits.Num() > 0 && Hits.Last().bBlockingHit && (bIgnoreTargetableActors || bIgnoreWaterInSweep))
	{
		FCollisionQueryParams RetryParams = Params;
		for (const FHitResult& Hit : Hits)
		{
			if (Hit.bBlockingHit && ((bIgnoreTargetableActors && Cast<ITargetableInterface>(Hit.GetActor()))
				|| (bIgnoreWaterInSweep && Cast<AWaterBody>(Hit.GetActor()))))
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

	// (2026-10-20) 물 위 유지: 카메라 아래가 물이면 수면 + MinHeightAboveWater 아래로 내리지 않는다.
	// (제미나이 인수인계에 있다고 적혀 있던 기능인데 코드에는 없었다.)
	float SurfaceZ = 0.f;
	if (bKeepAboveWater && UknightWaterRippleSubsystem::FindWaterSurface(World, ResolvedLocation, SurfaceZ))
	{
		ResolvedLocation.Z = FMath::Max(ResolvedLocation.Z, SurfaceZ + MinHeightAboveWater);
	}
	return ResolvedLocation;
}
