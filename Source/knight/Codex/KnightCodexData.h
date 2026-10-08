#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "KnightCodexData.generated.h"

class UTexture2D;
class UknightMonsterDefinition;
class UknightRaidDefinition;

/** Only presentation fields live here. Combat and rewards remain in their own definitions. */
USTRUCT(BlueprintType)
struct KNIGHT_API FKnightCodexBossEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss")
	FName BossId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss")
	TObjectPtr<const UknightMonsterDefinition> MonsterDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss")
	TObjectPtr<const UknightRaidDefinition> RaidDefinition;

	/** Empty uses MonsterDefinition.DisplayName. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss")
	FText DisplayNameOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss")
	TSoftObjectPtr<UTexture2D> ListIcon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss")
	TSoftObjectPtr<UTexture2D> Portrait;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss", meta=(MultiLine=true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss", meta=(MultiLine=true))
	FText AdditionalDescription;
};

/** Array order is the displayed order. Add a row here without editing C++ or WBP graphs. */
UCLASS(BlueprintType)
class KNIGHT_API UKnightCodexData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World")
	FText WorldTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World", meta=(MultiLine=true))
	FText WorldBody;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mech")
	FText MechTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mech", meta=(MultiLine=true))
	FText MechBody;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bosses", meta=(TitleProperty="BossId"))
	TArray<FKnightCodexBossEntry> Bosses;
};
