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

	/** Region shown on the stage-select globe panel (e.g. "북부 폐허 도시"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stage|Globe")
	FText RegionName;

	/** Marker position on the stage-select globe in degrees: X = longitude (-180..180), Y = latitude (-90..90). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stage|Globe")
	FVector2D GlobeLocation = FVector2D::ZeroVector;

	/** Angular radius of the stage's region on the globe, in degrees. Land within it is painted and clickable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stage|Globe", meta=(ClampMin=2, ClampMax=45, Units="deg"))
	float GlobeRegionRadius = 14.f;

	/** Optional picture for the globe panel. The panel hides the image area when empty. */
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
