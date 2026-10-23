#include "Camera/knightCameraOcclusionHandler.h"

#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
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

	// 카메라를 막지 않는 것: 몬스터(페이드 핸들러가 처리), 물(아래 수면 높이 제한), (2026-10-23) 보이지 않는 벽.
	const auto IsIgnoredHit = [this](const FHitResult& Hit)
	{
		const AActor* const Actor = Hit.GetActor();
		if (bIgnoreTargetableActors && Cast<ITargetableInterface>(Actor))
		{
			return true;
		}
		if (bIgnoreWaterInSweep && Cast<AWaterBody>(Actor))
		{
			return true;
		}
		const UPrimitiveComponent* const Component = Hit.GetComponent();
		return bIgnoreInvisibleBlockers && ((Actor && Actor->IsHidden()) || (Component && (Component->bHiddenInGame || !Component->IsVisible())));
	};

	// 멀티 스윕은 처음 막는 것에서 멈춘다. 그게 무시할 것이면 그 액터를 빼고 다시 스윕한다(무시할 것이 연달아 있을 수 있어 몇 번까지).
	float BlockedFraction = 1.f;
	FCollisionQueryParams SweepParams = Params;
	for (int32 Attempt = 0; Attempt < 4; ++Attempt)
	{
		TArray<FHitResult> Hits;
		World->SweepMultiByChannel(Hits, Pivot, DesiredLocation, FQuat::Identity, ProbeChannel, FCollisionShape::MakeSphere(ProbeRadius), SweepParams);
		bool bSkippedBlockingHit = false;
		for (const FHitResult& Hit : Hits)
		{
			if (!Hit.bBlockingHit)
			{
				continue;
			}
			if (IsIgnoredHit(Hit))
			{
				if (AActor* const Actor = Hit.GetActor())
				{
					SweepParams.AddIgnoredActor(Actor);
				}
				else if (const UPrimitiveComponent* const Component = Hit.GetComponent())
				{
					SweepParams.AddIgnoredComponent(Component);
				}
				bSkippedBlockingHit = true;
				continue;
			}
			BlockedFraction = FMath::Min(BlockedFraction, Hit.Time);
		}
		if (BlockedFraction < 1.f || !bSkippedBlockingHit)
		{
			break;
		}
	}

	CurrentFraction = BlockedFraction < CurrentFraction
		? BlockedFraction
		: FMath::FInterpTo(CurrentFraction, BlockedFraction, DeltaTime, RecoverySpeed);
	FVector ResolvedLocation = FMath::Lerp(Pivot, DesiredLocation, CurrentFraction);

	// (2026-10-23) 당겨진 만큼 카메라를 올린다(시야각은 카메라 컴포넌트가 GetCloseUpExtraFOV로 넓힘). 위가 막히면 막힌 데까지만.
	const float PullIn = 1.f - CurrentFraction;
	if (CloseUpRaise > 0.f && PullIn > KINDA_SMALL_NUMBER)
	{
		const FVector RaiseEnd = ResolvedLocation + FVector::UpVector * (CloseUpRaise * PullIn);
		FHitResult RaiseHit;
		ResolvedLocation = World->SweepSingleByChannel(RaiseHit, ResolvedLocation, RaiseEnd, FQuat::Identity, ProbeChannel,
			FCollisionShape::MakeSphere(ProbeRadius), SweepParams) ? RaiseHit.Location : RaiseEnd;
	}
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
