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
class UknightWeaponItemDefinition;
class UknightSocketItemDefinition;
class UKnightMaintenanceEntryWidget;
class UknightProgressionSubsystem;
class UCanvasPanel;

enum class EKnightMaintenanceDestination : uint8
{
	Crafting,
	Enhancement,
	Sockets
};

DECLARE_DELEGATE_OneParam(FKnightMaintenanceSlotFocused, FGameplayTag);

/**
 * 로비 정비 목적지에서 재사용하는 화면. 제작 목적지는 제작/장비/소재 탭을 제공하고,
 * 강화와 소켓은 각각 기존 기능 패널만 연다.
 */
UCLASS()
class KNIGHT_API UKnightMaintenanceWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	static TSubclassOf<UKnightMaintenanceWidget> GetSharedWidgetClass();
	void Open(EKnightMaintenanceDestination InDestination);
	void Close();
	EKnightMaintenanceDestination GetDestination() const { return Destination; }
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
	UFUNCTION() void HandleActiveSlotChanged(const FString& SlotId);
	void SetCategory(int32 NewCategory, bool bFromPlayer);
	void SetSlot(FGameplayTag NewSlot, bool bFromPlayer);
	void SetInfoTab(int32 NewTab);
	void RebuildList();
	void EnsureStatRows();
	void RefreshDetails();
	void RefreshMaterials(const UknightCraftRecipeDefinition* Recipe, UknightProgressionSubsystem* Progression);
	void RefreshWeaponUpgradePanel();
	void RebuildSocketList();
	void RefreshSocketDetails();
	void RequestWeaponUpgrade();
	void RefreshTabButtons();
	void RefreshCurrency();
	void RefreshMaterialContainer();
	void SelectEntry(UKnightMaintenanceEntryWidget* Entry);
	void HandleEntryClicked(UKnightMaintenanceEntryWidget* Entry);
	void HandleEntryFocused(UKnightMaintenanceEntryWidget* Entry);
	void RequestExecute();
	void ExecuteSelection();
	void UnequipSelection();
	UknightProgressionSubsystem* GetProgression() const;

	int32 Category = 0; // 0 제작, 1 강화, 2 장비, 3 소켓, 4 소재(후속 단계용 컨테이너)
	EKnightMaintenanceDestination Destination = EKnightMaintenanceDestination::Crafting;
	int32 InfoTab = 0;  // 0 스펙, 1 세트, 2 재료
	FGameplayTag Slot;
	bool bBusy = false;
	FText ExecuteLockReason;
	FText UnequipLockReason;
	FText UpgradeResultMessage;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> UpgradePanel;
	UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> UpgradeHeading;
	UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> UpgradeAttack;
	UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> UpgradeMoney;
	UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> UpgradeGate;
	UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> UpgradeChance;
	UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> UpgradeResult;
	UPROPERTY(Transient) TObjectPtr<UCommonButtonBase> UpgradeButton;
	UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> CurrencyText;
	UPROPERTY(Transient) TArray<TObjectPtr<UCommonButtonBase>> SocketListButtons;
	UPROPERTY(Transient) TObjectPtr<const UknightSocketItemDefinition> SelectedSocketItem;
	int32 SelectedSocketSlot = INDEX_NONE;
	UPROPERTY(Transient) TObjectPtr<const UknightPartItemDefinition> SelectedPart;
	UPROPERTY(Transient) TObjectPtr<const UknightCraftRecipeDefinition> SelectedRecipe;
	UPROPERTY(Transient) TArray<TObjectPtr<UKnightMaintenanceEntryWidget>> Entries;
	UPROPERTY(Transient) TArray<TObjectPtr<UCommonTextBlock>> StatLabelTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UCommonTextBlock>> StatCurrentTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UCommonTextBlock>> StatArrowTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UCommonTextBlock>> StatAfterTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UCommonTextBlock>> StatDeltaTexts;
};
