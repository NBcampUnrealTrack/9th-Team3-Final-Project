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

DECLARE_DELEGATE_OneParam(FKnightMaintenanceSlotFocused, FGameplayTag);

/**
 * 로비 정비 화면. 왼쪽 목록(분류, 부위, 장비), 오른쪽 상세 패널.
 * 상세: 선택 장비(없으면 장착 장비) + 탭 [스펙(현재 → 변경 후) | 세트 | 재료(제작만)].
 */
UCLASS()
class KNIGHT_API UKnightMaintenanceWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	static TSubclassOf<UKnightMaintenanceWidget> GetSharedWidgetClass();
	void Open();
	void Close();
	/** Q/E, LB/RB. 제작이 아니면 재료 탭 건너뜀. */
	void CycleInfoTab(int32 Direction);
	/** 목록 줄에서 Enter = 실행. false면 키를 그대로 넘긴다. */
	bool HandleConfirmKey();
	/** Esc: 제작 확인 창 먼저 닫기. 닫을 게 없으면 false. */
	bool HandleBack();
	UWidget* GetFocusButton() const;

	/** 탭·부위 선택 시 카메라 이동용. Open에서는 안 보냄(전신 시작). */
	FKnightMaintenanceSlotFocused OnSlotFocused;

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
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_Status;

	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_Selected;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_PartName;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_PartState;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_PartDesc;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_Bonus;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> Btn_InfoSpec;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> Btn_InfoSet;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> Btn_InfoMaterial;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidgetSwitcher> Switcher_Info;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UVerticalBox> Stats_Rows;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_SetInfo;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_ActiveSets;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UVerticalBox> Box_Materials;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_StageReq;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> Btn_Execute;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> Btn_Unequip;
	/** 제작 확인 창. 공용 모달은 knightUI 모듈이라 여기서 못 씀. */
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> Panel_Confirm;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_ConfirmTitle;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_ConfirmBody;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> Btn_ConfirmOk;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> Btn_ConfirmCancel;

private:
	void ShowConfirm(bool bShow);
	UFUNCTION() void HandleProgressionChanged();
	void SetCategory(int32 NewCategory, bool bFromPlayer);
	void SetSlot(FGameplayTag NewSlot, bool bFromPlayer);
	void SetInfoTab(int32 NewTab);
	void RebuildList();
	void EnsureStatRows();
	void RefreshDetails();
	void RefreshMaterials(const UknightCraftRecipeDefinition* Recipe, UknightProgressionSubsystem* Progression);
	void RefreshTabButtons();
	void SelectEntry(UKnightMaintenanceEntryWidget* Entry);
	void HandleEntryClicked(UKnightMaintenanceEntryWidget* Entry);
	void HandleEntryFocused(UKnightMaintenanceEntryWidget* Entry);
	void RequestExecute();
	void ExecuteSelection();
	void UnequipSelection();
	UknightProgressionSubsystem* GetProgression() const;

	int32 Category = 0; // 0 제작, 1 무기, 2 방어구
	int32 InfoTab = 0;  // 0 스펙, 1 세트, 2 재료
	FGameplayTag Slot;
	bool bBusy = false;
	FText ExecuteLockReason;
	FText UnequipLockReason;
	UPROPERTY(Transient) TObjectPtr<const UknightPartItemDefinition> SelectedPart;
	UPROPERTY(Transient) TObjectPtr<const UknightCraftRecipeDefinition> SelectedRecipe;
	UPROPERTY(Transient) TArray<TObjectPtr<UKnightMaintenanceEntryWidget>> Entries;
	UPROPERTY(Transient) TArray<TObjectPtr<UCommonTextBlock>> StatLabelTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UCommonTextBlock>> StatCurrentTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UCommonTextBlock>> StatArrowTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UCommonTextBlock>> StatAfterTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UCommonTextBlock>> StatDeltaTexts;
};
