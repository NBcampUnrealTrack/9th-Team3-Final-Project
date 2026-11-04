#pragma once

#include "CoreMinimal.h"
#include "Codex/KnightCodexData.h"
#include "Codex/KnightCodexPresentation.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "KnightCodexWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UWidget;
class UVerticalBox;
class UWidgetSwitcher;
class UTexture2D;
class UScrollBox;
class UKnightCodexData;
class UknightItemDefinition;
struct FKnightCodexBossEntry;
struct FknightRaidReward;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FKnightCodexClosed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKnightCodexBossSelected, FName, BossId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FKnightCodexSectionSelected, int32, Category, int32, SectionIndex);

/** A small native list button keeps each prose section selectable without Blueprint graph bindings. */
UCLASS()
class KNIGHT_API UKnightCodexSectionButton : public UKnightCodexArchiveButton
{
	GENERATED_BODY()
public:
	UPROPERTY() FKnightCodexSectionSelected OnSectionSelected;
	void SetSection(int32 InCategory, int32 InIndex) { Category = InCategory; SectionIndex = InIndex; }
protected:
	virtual void SynchronizeProperties() override;
private:
	UFUNCTION() void HandleClicked();
	int32 Category = 0;
	int32 SectionIndex = 0;
};

UCLASS()
class KNIGHT_API UKnightCodexBossEntryWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable, Category="Knight|Codex") FKnightCodexBossSelected OnSelected;
	void Configure(FName InId, const FText& Name, UTexture2D* Icon, bool bUnlocked);
	void SetSelected(bool bSelected);
	FName GetBossId() const { return BossId; }
protected:
	virtual void NativeOnInitialized() override;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Button_Select;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Name;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UImage> Image_Icon;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_LockState;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWidget> Row_LockState;
private:
	UFUNCTION() void HandleClicked();
	FName BossId;
	bool bEntrySelected = false;
	bool bEntryUnlocked = false;
};

UCLASS()
class KNIGHT_API UKnightCodexDropEntryWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Configure(const FknightRaidReward& Reward);
	void ConfigureQuestDrop(const UknightItemDefinition* Item, const TArray<FText>& Sources);
protected:
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Name;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Details;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UImage> Image_Icon;
};

UCLASS()
class KNIGHT_API UKnightCodexWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	static TSubclassOf<UKnightCodexWidget> GetSharedWidgetClass();
	UPROPERTY(BlueprintAssignable, Category="Knight|Codex") FKnightCodexClosed OnClosed;
	UFUNCTION(BlueprintCallable, Category="Knight|Codex") void Open();
	UFUNCTION(BlueprintCallable, Category="Knight|Codex") void RequestClose();
	UFUNCTION(BlueprintCallable, Category="Knight|Codex") void SelectCategory(int32 Category);
	UFUNCTION(BlueprintCallable, Category="Knight|Codex") bool SelectBoss(FName BossId);
	UButton* GetFocusButton() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Knight|Codex") TSoftObjectPtr<UKnightCodexData> CodexData;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Knight|Codex") TSubclassOf<UKnightCodexBossEntryWidget> BossEntryClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Knight|Codex") TSubclassOf<UKnightCodexDropEntryWidget> DropEntryClass;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Button_World;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Button_Mech;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Button_Monster;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Button_Close;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidgetSwitcher> Switcher_Content;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_WorldTitle;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_WorldBody;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_MechTitle;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_MechBody;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UVerticalBox> Box_Bosses;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_NoBosses;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_BossName;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_BossDescription;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_BossExtra;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_NoPortrait;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UImage> Image_Portrait;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UVerticalBox> Box_Drops;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_NoDrops;
private:
	UFUNCTION() void HandleWorld();
	UFUNCTION() void HandleMech();
	UFUNCTION() void HandleMonster();
	UFUNCTION() void HandleBossSelected(FName BossId);
	UFUNCTION() void HandleProgressionChanged();
	UFUNCTION() void HandleActiveSlotChanged(const FString& SlotId);
	void RebuildBosses();
	void RefreshUnlockStates();
	bool IsBossUnlocked(const FKnightCodexBossEntry& Boss) const;
	void SetBossDetailLocked(bool bLocked);
	UFUNCTION() void HandleSectionSelected(int32 Category, int32 SectionIndex);
	void RebuildTextPage(FName ListName, UTextBlock* LegacyBody, const FText& Title,
		const TArray<FKnightCodexTextSection>& Sections, int32 Category);
	void SelectReadingSection(int32 Category, int32 SectionIndex);
	void ResetReadingScroll(FName ScrollName);
	void RefreshBoss();
	void ClearBossDetail();
	const FKnightCodexBossEntry* FindBoss(FName BossId) const;
	UPROPERTY(Transient) TObjectPtr<UKnightCodexData> LoadedData;
	UPROPERTY(Transient) TArray<TObjectPtr<UKnightCodexBossEntryWidget>> GeneratedEntries;
	TArray<TWeakObjectPtr<UKnightCodexSectionButton>> ReadingButtons[2];
	TArray<int32> ReadingSectionIndices[2];
	FText ReadingPageTitles[2];
	FName SelectedBossId;
	int32 ActiveCategory = 0;
};
