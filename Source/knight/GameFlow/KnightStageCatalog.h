#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "KnightStageCatalog.generated.h"

class UWorld;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stage")
	TSoftObjectPtr<UWorld> GameplayMap;
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
