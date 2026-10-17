#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "KnightCodexData.generated.h"

class UTexture2D;
class UknightMonsterDefinition;
class UknightRaidDefinition;

/** Plain editable prose; headings are rendered by the widget, not Markdown syntax. */
USTRUCT(BlueprintType)
struct KNIGHT_API FKnightCodexTextSection
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Text")
	FText Heading;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Text", meta=(MultiLine=true))
	FText Body;

	/** 2 = section heading, 3 = subsection heading. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Text", meta=(ClampMin="2", ClampMax="3"))
	int32 HeadingLevel = 2;
};

/** Only presentation fields live here. Combat and rewards remain in their own definitions. */
USTRUCT(BlueprintType)
struct KNIGHT_API FKnightCodexBossEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss")
	FName BossId;

	/** Stable quest ID. Empty means locked until a completion quest is assigned; raid clears do not count. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Unlock")
	FName RequiredQuestId;

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

	/** Displayed in order. WorldBody remains the fallback for older assets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World", meta=(TitleProperty="Heading"))
	TArray<FKnightCodexTextSection> WorldSections;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mech")
	FText MechTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mech", meta=(MultiLine=true))
	FText MechBody;

	/** Displayed in order. MechBody remains the fallback for older assets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mech", meta=(TitleProperty="Heading"))
	TArray<FKnightCodexTextSection> MechSections;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bosses", meta=(TitleProperty="BossId"))
	TArray<FKnightCodexBossEntry> Bosses;
};
