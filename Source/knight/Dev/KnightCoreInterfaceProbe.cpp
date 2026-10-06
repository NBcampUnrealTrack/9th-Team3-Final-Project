#include "Dev/KnightCoreInterfaceProbe.h"

#include "Components/SceneComponent.h"

AKnightCoreInterfaceProbe::AKnightCoreInterfaceProbe()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AKnightCoreInterfaceProbe::GetTargetPoints(TArray<FknightTargetPoint>& OutPoints) const
{
	OutPoints.Reset();
	FknightTargetPoint Point;
	Point.Component = RootComponent;
	OutPoints.Add(Point);
}

FText AKnightCoreInterfaceProbe::GetInteractionPrompt_Implementation() const
{
	return NSLOCTEXT("KnightCoreProbe", "Interact", "Inspect core probe");
}
