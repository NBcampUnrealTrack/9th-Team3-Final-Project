#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "KnightMaintenanceWidget.generated.h"

class UCommonButtonBase;
class UCommonTextBlock;
class UImage;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class UWidgetSwitcher;
class UknightCraftRecipeDefinition;
class UknightPartItemDefinition;
class UKnightMaintenanceEntryWidget;
class UknightProgressionSubsystem;

UCLASS()
class KNIGHT_API UKnightMaintenanceWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	static TSubclassOf<UKnightMaintenanceWidget> GetSharedWidgetClass();
	void Open();
	void Close();
	void CyclePage(int32 Direction);
	void TogglePreview();
	UWidget* GetFocusButton() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> Btn_Craft;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> Btn_Weapon;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> Btn_Armor;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> Btn_Head;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> Btn_Torso;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> Btn_Legs;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> Btn_Arms;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> Btn_SlotWeapon;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UScrollBox> List_Items;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidgetSwitcher> Switcher_Info;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UVerticalBox> Stats_Rows;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_ActiveSets;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_SetInfo;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_Detail;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_Page;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_Status;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> Btn_PageLeft;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> Btn_PageRight;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> Btn_Execute;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> Btn_Unequip;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> Btn_Preview;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_Selected;

private:
	UFUNCTION() void HandleProgressionChanged();
	void SetCategory(int32 NewCategory);
	void SetSlot(FGameplayTag NewSlot);
	void RebuildList();
	void EnsureStatRows();
	void RefreshDetails();
	void HandleEntryClicked(UKnightMaintenanceEntryWidget* Entry);
	void ExecuteSelection();
	void UnequipSelection();
	UknightProgressionSubsystem* GetProgression() const;

	int32 Category = 0; // 0 craft, 1 weapon, 2 armor
	FGameplayTag Slot;
	int32 Page = 0;
	bool bPreview = false;
	bool bBusy = false;
	UPROPERTY(Transient) TObjectPtr<const UknightPartItemDefinition> SelectedPart;
	UPROPERTY(Transient) TObjectPtr<const UknightCraftRecipeDefinition> SelectedRecipe;
	UPROPERTY(Transient) TArray<TObjectPtr<UKnightMaintenanceEntryWidget>> Entries;
	UPROPERTY(Transient) TArray<TObjectPtr<UCommonTextBlock>> StatCurrentTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UCommonTextBlock>> StatDeltaTexts;
};
