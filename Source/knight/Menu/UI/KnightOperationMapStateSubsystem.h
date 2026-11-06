#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "KnightOperationMapStateSubsystem.generated.h"

/** Session-only browsing context. Never serialized into a player save. */
struct FKnightOperationMapContext
{
	FName RegionId;
	FName QuestId;
	FVector2D Center = FVector2D::ZeroVector;
	float VisibleLatitude = 110.f;
};

UCLASS()
class KNIGHT_API UKnightOperationMapStateSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	TMap<FString, FKnightOperationMapContext> Profiles;
};
