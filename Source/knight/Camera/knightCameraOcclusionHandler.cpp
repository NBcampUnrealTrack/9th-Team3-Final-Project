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
	// (2026-10-26) 벽에 비비면 흔들리던 문제: 당긴 정도는 프레임마다 바뀌고(당길 때 즉시), 벽에 닿은 자리에서 위로 검사하면
	// 닿은 프레임·안 닿은 프레임이 번갈아 40cm를 올렸다 내렸다 했다. → 올리기·시야각은 부드럽게 따라가고,
	// 위로 검사할 때 처음부터 닿아 있는 것(벽 옆면)은 막힌 것으로 치지 않는다.
	SmoothedCloseUp = FMath::FInterpTo(SmoothedCloseUp, 1.f - CurrentFraction, DeltaTime, CloseUpBlendSpeed);
	float AllowedRaise = CloseUpRaise * SmoothedCloseUp;
	if (AllowedRaise > KINDA_SMALL_NUMBER)
	{
		FHitResult RaiseHit;
		if (World->SweepSingleByChannel(RaiseHit, ResolvedLocation, ResolvedLocation + FVector::UpVector * AllowedRaise, FQuat::Identity,
			ProbeChannel, FCollisionShape::MakeSphere(ProbeRadius * 0.5f), SweepParams) && !RaiseHit.bStartPenetrating)
		{
			AllowedRaise = RaiseHit.Distance;
		}
	}
	// 천장에 막혀 낮춰야 할 때는 빨리, 올릴 때는 부드럽게.
	SmoothedRaise = FMath::FInterpTo(SmoothedRaise, AllowedRaise, DeltaTime,
		AllowedRaise < SmoothedRaise ? CloseUpBlendSpeed * 3.f : CloseUpBlendSpeed);
	ResolvedLocation.Z += SmoothedRaise;
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
