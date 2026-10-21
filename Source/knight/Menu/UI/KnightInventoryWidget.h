#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UObject/PrimaryAssetId.h"
#include "KnightInventoryWidget.generated.h"

class UButton;
class UCommonTextBlock;
class UknightItemDefinition;
class UknightProgressionSubsystem;
class UScrollBox;
class UVerticalBox;

DECLARE_MULTICAST_DELEGATE_OneParam(FKnightInventoryItemSelected, const UknightItemDefinition*);

/** A read-only row. The item count and equipped flag are supplied by the progression view. */
UCLASS()
class KNIGHT_API UKnightInventoryEntryWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	FKnightInventoryItemSelected OnSelected;
	void Configure(const UknightItemDefinition* InItem, int32 Count, bool bEquipped, const FLinearColor& NameColor);
	void SetSelected(bool bInSelected);
	const UknightItemDefinition* GetItem() const { return Item; }
	UButton* GetButton() const { return Button; }
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
private:
	UFUNCTION() void HandleClicked();
	void ApplyConfiguration();
	UPROPERTY(Transient) TObjectPtr<const UknightItemDefinition> Item;
	UPROPERTY(Transient) TObjectPtr<UButton> Button;
	UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> NameText;
	UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> CountText;
	UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> EquippedText;
	int32 DisplayCount = 0;
	bool bDisplayEquipped = false;
	bool bSelected = false;
	FLinearColor DisplayNameColor = FLinearColor::White;
};

/** Main-menu inventory; never owns item quantities or writes to a save. */
UCLASS()
class KNIGHT_API UKnightInventoryWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Open();
	void Close();
	UButton* GetFocusButton() const;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	UFUNCTION() void ShowEquipment();
	UFUNCTION() void ShowMaterials();
	UFUNCTION() void ShowSockets();
	UFUNCTION() void HandleProgressionChanged();
	UFUNCTION() void HandleActiveSlotChanged(const FString& SlotId);
	void SetCategory(int32 NewCategory);
	void Refresh();
	void SelectItem(const UknightItemDefinition* Item);
	void RefreshDetail();
	UknightProgressionSubsystem* GetProgression() const;
	UPROPERTY(Transient) TObjectPtr<UButton> EquipmentButton;
	UPROPERTY(Transient) TObjectPtr<UButton> MaterialButton;
	UPROPERTY(Transient) TObjectPtr<UButton> SocketButton;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> ItemList;
	UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> DetailName;
	UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> DetailDescription;
	UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> ProfileText;
	UPROPERTY(Transient) TArray<TObjectPtr<UKnightInventoryEntryWidget>> Entries;
	FPrimaryAssetId SelectedItemId;
	int32 Category = 0;
};
