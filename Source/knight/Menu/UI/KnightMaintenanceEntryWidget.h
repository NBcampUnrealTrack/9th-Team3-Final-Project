#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "KnightMaintenanceEntryWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UknightPartItemDefinition;
class UknightCraftRecipeDefinition;
class UKnightMaintenanceEntryWidget;

/** 상태 배지 종류. */
enum class EKnightMaintenanceEntryState : uint8
{
	Equipped,
	Owned,
	Craftable,
	MissingMaterials,
};

DECLARE_DELEGATE_OneParam(FKnightMaintenanceEntryFocused, UKnightMaintenanceEntryWidget*);

/** 정비 목록 한 줄: 아이콘, 이름, 상태 배지. 선택 표시는 버튼 스타일. */
UCLASS()
class KNIGHT_API UKnightMaintenanceEntryWidget : public UCommonButtonBase
{
	GENERATED_BODY()
public:
	static TSubclassOf<UKnightMaintenanceEntryWidget> GetSharedWidgetClass();
	void SetEntry(const UknightPartItemDefinition* InPart, const UknightCraftRecipeDefinition* InRecipe,
		EKnightMaintenanceEntryState State, int32 OwnedCount);
	const UknightPartItemDefinition* GetPart() const { return Part; }
	const UknightCraftRecipeDefinition* GetRecipe() const { return Recipe; }

	/** 포커스를 받으면 알림. 키보드 이동 = 선택. */
	FKnightMaintenanceEntryFocused OnEntryFocused;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;

	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Name;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_State;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UBorder> Border_Badge;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_Icon;
private:
	UPROPERTY(Transient) TObjectPtr<const UknightPartItemDefinition> Part;
	UPROPERTY(Transient) TObjectPtr<const UknightCraftRecipeDefinition> Recipe;
};
