#include "Camera/knightCameraFadeHandler.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/TargetableInterface.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"

UknightCameraFadeHandler::UknightCameraFadeHandler()
{
	FadeObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	FadeObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));
	FadeObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	FadeObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
}

void UknightCameraFadeHandler::UpdateFade(const AActor* ViewOwner, const FVector& PivotLocation, const FVector& CameraLocation, float DeltaTime)
{
	const APawn* const Pawn = Cast<APawn>(ViewOwner);
	APlayerController* const PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (PC != ViewController.Get())
	{
		// A different viewer: its hidden list does not contain our entries, so start clean.
		RestoreAll();
		ViewController = PC;
	}

	TSet<TWeakObjectPtr<UPrimitiveComponent>> Occluders;
	GatherOccluders(ViewOwner, PivotLocation, CameraLocation, Occluders);
	bAnyOccluder = !Occluders.IsEmpty();

	for (const TWeakObjectPtr<UPrimitiveComponent>& Occluder : Occluders)
	{
		if (!FadedComponents.Contains(Occluder))
		{
			FadedComponents.Add(Occluder, 1.f);
		}
	}

	for (auto It = FadedComponents.CreateIterator(); It; ++It)
	{
		UPrimitiveComponent* const Component = It.Key().Get();
		if (!Component)
		{
			It.RemoveCurrent();
			continue;
		}
		const bool bOccluding = Occluders.Contains(It.Key());
		if (FadeMode == EknightCameraFadeMode::HideFromView)
		{
			SetHiddenFromView(Component, bOccluding);
			if (!bOccluding)
			{
				It.RemoveCurrent();
			}
			continue;
		}

		const float Target = bOccluding ? FadedOpacity : 1.f;
		float& Opacity = It.Value();
		Opacity = FMath::FInterpTo(Opacity, Target, DeltaTime, bOccluding ? FadeOutSpeed : FadeInSpeed);
		if (!bOccluding && Opacity > 0.99f)
		{
			ApplyOpacity(Component, 1.f);
			It.RemoveCurrent();
			continue;
		}
		ApplyOpacity(Component, Opacity);
	}

	UpdateParameterCollection(ViewOwner, PivotLocation, CameraLocation, DeltaTime);
}

void UknightCameraFadeHandler::RestoreAll()
{
	for (const TPair<TWeakObjectPtr<UPrimitiveComponent>, float>& Entry : FadedComponents)
	{
		if (UPrimitiveComponent* const Component = Entry.Key.Get())
		{
			ApplyOpacity(Component, 1.f);
			SetHiddenFromView(Component, false);
		}
	}
	FadedComponents.Reset();
	bAnyOccluder = false;
	OcclusionAmount = 0.f;
}

void UknightCameraFadeHandler::GatherOccluders(const AActor* ViewOwner, const FVector& PivotLocation, const FVector& CameraLocation,
	TSet<TWeakObjectPtr<UPrimitiveComponent>>& OutOccluders) const
{
	UWorld* const World = ViewOwner ? ViewOwner->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	FCollisionObjectQueryParams ObjectParams;
	for (const TEnumAsByte<EObjectTypeQuery>& ObjectType : FadeObjectTypes)
	{
		ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectType));
	}
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(knightCameraFade), false, ViewOwner);

	TArray<UPrimitiveComponent*> Candidates;
	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(Hits, CameraLocation, PivotLocation, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(ProbeRadius), QueryParams);
	for (const FHitResult& Hit : Hits)
	{
		Candidates.Add(Hit.GetComponent());
	}
	if (CameraProximityRadius > 0.f)
	{
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(Overlaps, CameraLocation, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(CameraProximityRadius), QueryParams);
		for (const FOverlapResult& Overlap : Overlaps)
		{
			Candidates.Add(Overlap.GetComponent());
		}
	}

	TSet<const AActor*> WholeActorsAdded;
	for (UPrimitiveComponent* const Component : Candidates)
	{
		AActor* const Actor = Component ? Component->GetOwner() : nullptr;
		if (!Actor || !ShouldFade(Actor, Component, ViewOwner))
		{
			continue;
		}
		// Monsters fade as a whole (body, armor, weapons); tagged props fade per component.
		if (bFadeTargetableActors && Cast<ITargetableInterface>(Actor))
		{
			if (!WholeActorsAdded.Contains(Actor))
			{
				WholeActorsAdded.Add(Actor);
				TArray<UPrimitiveComponent*> ActorPrimitives;
				Actor->GetComponents<UPrimitiveComponent>(ActorPrimitives);
				for (UPrimitiveComponent* const Primitive : ActorPrimitives)
				{
					if (Primitive->IsVisible())
					{
						OutOccluders.Add(Primitive);
					}
				}
			}
			continue;
		}
		OutOccluders.Add(Component);
	}
}

bool UknightCameraFadeHandler::ShouldFade(const AActor* Actor, const UPrimitiveComponent* Component, const AActor* ViewOwner) const
{
	if (Actor == ViewOwner || Actor->IsAttachedTo(ViewOwner))
	{
		return false;
	}
	if (bFadeTargetableActors && Cast<ITargetableInterface>(Actor))
	{
		return true;
	}
	return !FadeTag.IsNone() && (Actor->ActorHasTag(FadeTag) || Component->ComponentHasTag(FadeTag));
}

void UknightCameraFadeHandler::ApplyOpacity(UPrimitiveComponent* Component, float Opacity) const
{
	if (FadeMode == EknightCameraFadeMode::DitherFade && !FadeParameterName.IsNone())
	{
		Component->SetScalarParameterForCustomPrimitiveData(FadeParameterName, Opacity);
	}
}

void UknightCameraFadeHandler::SetHiddenFromView(UPrimitiveComponent* Component, bool bHidden)
{
	APlayerController* const PC = ViewController.Get();
	if (!PC)
	{
		return;
	}
	if (bHidden)
	{
		PC->HiddenPrimitiveComponents.AddUnique(Component);
	}
	else
	{
		PC->HiddenPrimitiveComponents.Remove(Component);
	}
}

void UknightCameraFadeHandler::UpdateParameterCollection(const AActor* ViewOwner, const FVector& PivotLocation, const FVector& CameraLocation, float DeltaTime)
{
	if (!OcclusionParameterCollection || !ViewOwner)
	{
		return;
	}
	UObject* const WorldContext = const_cast<AActor*>(ViewOwner);
	OcclusionAmount = FMath::FInterpTo(OcclusionAmount, bAnyOccluder ? 1.f : 0.f, DeltaTime, bAnyOccluder ? FadeOutSpeed : FadeInSpeed);
	UKismetMaterialLibrary::SetVectorParameterValue(WorldContext, OcclusionParameterCollection, TEXT("CameraLocation"), FLinearColor(CameraLocation));
	UKismetMaterialLibrary::SetVectorParameterValue(WorldContext, OcclusionParameterCollection, TEXT("PivotLocation"), FLinearColor(PivotLocation));
	UKismetMaterialLibrary::SetScalarParameterValue(WorldContext, OcclusionParameterCollection, TEXT("OcclusionAmount"), OcclusionAmount);
}
