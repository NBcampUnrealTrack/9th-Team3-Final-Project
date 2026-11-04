#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "KnightStageCatalog.generated.h"

class UTexture2D;
class UWorld;
class UknightRaidDefinition;

USTRUCT(BlueprintType)
struct KNIGHT_API FKnightStageDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stage")
	FName StageId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stage")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stage", meta=(MultiLine=true))
	FText Description;

	/** Region shown on the stage-select map panel (e.g. "북부 폐허 도시"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stage|Globe")
	FText RegionName;

	/** Marker position on the stage-select map in degrees: X = longitude (-180..180), Y = latitude (-75..75, the map edge). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stage|Globe")
	FVector2D GlobeLocation = FVector2D::ZeroVector;

	/** Radius of the stage's region on the map, in degrees of latitude. The area within it is painted and clickable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stage|Globe", meta=(ClampMin=2, ClampMax=45, Units="deg"))
	float GlobeRegionRadius = 14.f;

	/** Optional picture for the map panel. The panel hides the image area when empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stage|Globe")
	TSoftObjectPtr<UTexture2D> Thumbnail;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stage")
	TSoftObjectPtr<UWorld> GameplayMap;

	/** Raid rules (target monster, time limit). Optional; loaded by the raid GameMode on arrival. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stage")
	TSoftObjectPtr<UknightRaidDefinition> RaidDefinition;

	/** Listed only in editor PIE for isolated developer validation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stage")
	bool bDevelopmentOnly = false;

	/** Keep the stage launchable through a quest placement without drawing a second region. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stage")
	bool bShowOnMap = true;

	/** Permit travel to an unfinished raid map without a target monster; the GameMode remains in exploration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stage")
	bool bAllowMapExploration = false;
};

/** Definitions only; referencing this asset does not load the gameplay maps. */
UCLASS(BlueprintType)
class KNIGHT_API UKnightStageCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stage")
	TArray<FKnightStageDefinition> Stages;
};
