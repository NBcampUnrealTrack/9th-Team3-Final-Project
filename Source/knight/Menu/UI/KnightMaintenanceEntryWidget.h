#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "KnightMaintenanceEntryWidget.generated.h"

class UImage;
class UTextBlock;
class UknightPartItemDefinition;
class UknightCraftRecipeDefinition;

UCLASS()
class KNIGHT_API UKnightMaintenanceEntryWidget : public UCommonButtonBase
{
	GENERATED_BODY()
public:
	static TSubclassOf<UKnightMaintenanceEntryWidget> GetSharedWidgetClass();
	void SetEntry(const UknightPartItemDefinition* InPart, const UknightCraftRecipeDefinition* InRecipe,
		bool bEquipped, int32 OwnedCount);
	const UknightPartItemDefinition* GetPart() const { return Part; }
	const UknightCraftRecipeDefinition* GetRecipe() const { return Recipe; }

protected:
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Name;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_State;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_Icon;
private:
	UPROPERTY(Transient) TObjectPtr<const UknightPartItemDefinition> Part;
	UPROPERTY(Transient) TObjectPtr<const UknightCraftRecipeDefinition> Recipe;
};
