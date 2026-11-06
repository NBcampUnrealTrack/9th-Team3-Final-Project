#include "Codex/KnightCodexWidget.h"

#include "Codex/KnightCodexData.h"
#include "Monster/knightMonsterDefinition.h"
#include "Raid/knightRaidDefinition.h"
#include "knightItemDefinition.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Progression/knightSaveSubsystem.h"
#include "Quest/KnightQuestData.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Texture2D.h"
#include "knight.h"

#define LOCTEXT_NAMESPACE "KnightCodex"

namespace
{
	const FLinearColor SelectedColor(FColor(0x18, 0x66, 0xAC));
	const FLinearColor NormalColor(FColor(0x1B, 0x2B, 0x3A));
	const FLinearColor LockedColor(FColor(0x15, 0x22, 0x2C));
	FText OrPending(const FText& Value) { return Value.IsEmpty() ? LOCTEXT("Pending", "설명 준비 중") : Value; }
	void SetImage(UImage* Image, UTexture2D* Texture, UWidget* Container = nullptr, bool bReserveSpace = false)
	{
		if (!Image) return;
		Image->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Texture) Image->SetBrushFromTexture(Texture, true);
		else Image->SetBrush(FSlateBrush());
		if (Container) Container->SetVisibility(Texture || bReserveSpace ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	struct FQuestDropGroup
	{
		const UknightItemDefinition* Item = nullptr;
		TArray<FText> Sources;
	};
	FText RewardCount(const FknightRaidReward& Reward)
	{
		return Reward.MinCount == Reward.MaxCount
			? FText::Format(LOCTEXT("Count", "수량 {0}"), FText::AsNumber(Reward.MinCount))
			: FText::Format(LOCTEXT("CountRange", "수량 {0}–{1}"), FText::AsNumber(Reward.MinCount), FText::AsNumber(Reward.MaxCount));
	}
	FText PartSource(const FGameplayTag& Tag)
	{
		FString Part = Tag.ToString();
		int32 Dot = INDEX_NONE;
		if (Part.FindLastChar(TEXT('.'), Dot)) Part.RightChopInline(Dot + 1);
		return FText::Format(LOCTEXT("BreakSource", "부위 파괴: {0}"), FText::FromString(Part));
	}
}

void UKnightCodexSectionButton::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
}

void UKnightCodexSectionButton::HandleClicked()
{
	OnSectionSelected.Broadcast(Category, SectionIndex);
}

void UKnightCodexBossEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button_Select) Button_Select->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
}

void UKnightCodexBossEntryWidget::Configure(FName InId, const FText& Name, UTexture2D* Icon, bool bUnlocked)
{
	BossId = InId;
	bEntryUnlocked = bUnlocked;
	if (Text_Name) Text_Name->SetText(Name);
	if (Text_Name) Text_Name->SetColorAndOpacity(FSlateColor(bUnlocked
		? FLinearColor(0.94f, 0.97f, 1.f, 1.f) : FLinearColor(0.54f, 0.65f, 0.74f, 1.f)));
	SetImage(Image_Icon, bUnlocked ? Icon : nullptr, GetWidgetFromName(TEXT("Size_Icon")));
	if (Text_LockState) Text_LockState->SetText(bUnlocked ? LOCTEXT("Unlocked", "해금") : LOCTEXT("Locked", "미해금"));
	if (Row_LockState) Row_LockState->SetVisibility(ESlateVisibility::HitTestInvisible);
	else if (Text_LockState) Text_LockState->SetVisibility(ESlateVisibility::HitTestInvisible);
	SetSelected(bEntrySelected);
}

void UKnightCodexBossEntryWidget::SetSelected(bool bSelected)
{
	bEntrySelected = bSelected;
	if (UKnightCodexArchiveButton* Button = Cast<UKnightCodexArchiveButton>(Button_Select)) Button->SetRecordState(bSelected, !bEntryUnlocked);
	else if (Button_Select) Button_Select->SetBackgroundColor(bSelected ? SelectedColor : bEntryUnlocked ? NormalColor : LockedColor);
}

void UKnightCodexBossEntryWidget::HandleClicked()
{
	if (!BossId.IsNone()) OnSelected.Broadcast(BossId);
}

void UKnightCodexDropEntryWidget::Configure(const FknightRaidReward& Reward)
{
	const UknightItemDefinition* Item = Reward.Item;
	if (Text_Name) Text_Name->SetText(Item && !Item->DisplayName.IsEmpty() ? Item->DisplayName : LOCTEXT("UnknownItem", "아이템 정보 없음"));
	SetImage(Image_Icon, Item ? Item->Icon.LoadSynchronous() : nullptr, GetWidgetFromName(TEXT("Size_ItemIcon")), true);
	TArray<FText> Details;
	if (Reward.MinCount == Reward.MaxCount)
	{
		Details.Add(FText::Format(LOCTEXT("Count", "수량 {0}"), FText::AsNumber(Reward.MinCount)));
	}
	else
	{
		Details.Add(FText::Format(LOCTEXT("CountRange", "수량 {0}–{1}"), FText::AsNumber(Reward.MinCount), FText::AsNumber(Reward.MaxCount)));
	}
	Details.Add(FText::Format(LOCTEXT("Chance", "확률 {0}"), FText::AsPercent(Reward.Chance)));
	if (Reward.RequiredBrokenPart.IsValid())
	{
		Details.Add(FText::Format(LOCTEXT("PartCondition", "부위 파괴: {0}"), FText::FromName(Reward.RequiredBrokenPart.GetTagName())));
	}
	if (Text_Details) Text_Details->SetText(FText::Join(LOCTEXT("DetailSeparator", " · "), Details));
}

void UKnightCodexDropEntryWidget::ConfigureQuestDrop(const UknightItemDefinition* Item, const TArray<FText>& Sources)
{
	if (Text_Name) Text_Name->SetText(Item && !Item->DisplayName.IsEmpty() ? Item->DisplayName : LOCTEXT("UnknownItem", "아이템 정보 없음"));
	SetImage(Image_Icon, Item ? Item->Icon.LoadSynchronous() : nullptr, GetWidgetFromName(TEXT("Size_ItemIcon")), true);
	if (Text_Details) Text_Details->SetText(FText::Join(FText::FromString(TEXT("\n")), Sources));
}

TSubclassOf<UKnightCodexWidget> UKnightCodexWidget::GetSharedWidgetClass()
{
	// Not cached in a static: the class can be unloaded when the menu map is left, leaving a stale pointer.
	const TSubclassOf<UKnightCodexWidget> Class = LoadClass<UKnightCodexWidget>(nullptr,
		TEXT("/Game/Knight/UI/Menu/Codex/WBP_KnightCodex.WBP_KnightCodex_C"));
	return Class ? Class : TSubclassOf<UKnightCodexWidget>(StaticClass());
}

void UKnightCodexWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button_World) Button_World->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleWorld);
	if (Button_Mech) Button_Mech->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMech);
	if (Button_Monster) Button_Monster->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMonster);
	if (Button_Close) Button_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::RequestClose);
}

void UKnightCodexWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Optional category artwork is supplied through the Designer brush, never generated.
	for (const TCHAR* Category : {TEXT("World"), TEXT("Mech"), TEXT("Monster")})
	{
		UImage* Image = Cast<UImage>(GetWidgetFromName(*FString::Printf(TEXT("Image_Category_%s"), Category)));
		UWidget* Container = GetWidgetFromName(*FString::Printf(TEXT("Size_CategoryIcon_%s"), Category));
		const bool bHasArtwork = Image && Image->GetBrush().GetResourceObject();
		if (Image) Image->SetVisibility(bHasArtwork ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Container) Container->SetVisibility(bHasArtwork ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UknightProgressionSubsystem* Progression = GameInstance->GetSubsystem<UknightProgressionSubsystem>())
			Progression->OnProgressionChanged.AddUniqueDynamic(this, &ThisClass::HandleProgressionChanged);
		if (UknightSaveSubsystem* Saves = GameInstance->GetSubsystem<UknightSaveSubsystem>())
			Saves->OnActiveSlotChanged.AddUniqueDynamic(this, &ThisClass::HandleActiveSlotChanged);
	}
}

void UKnightCodexWidget::NativeDestruct()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UknightProgressionSubsystem* Progression = GameInstance->GetSubsystem<UknightProgressionSubsystem>())
			Progression->OnProgressionChanged.RemoveDynamic(this, &ThisClass::HandleProgressionChanged);
		if (UknightSaveSubsystem* Saves = GameInstance->GetSubsystem<UknightSaveSubsystem>())
			Saves->OnActiveSlotChanged.RemoveDynamic(this, &ThisClass::HandleActiveSlotChanged);
	}
	for (UKnightCodexBossEntryWidget* Entry : GeneratedEntries)
	{
		if (Entry) Entry->OnSelected.RemoveDynamic(this, &ThisClass::HandleBossSelected);
	}
	GeneratedEntries.Reset();
	for (int32 Category = 0; Category < 2; ++Category)
	{
		for (const TWeakObjectPtr<UKnightCodexSectionButton>& Button : ReadingButtons[Category])
			if (Button.IsValid()) Button->OnSectionSelected.RemoveDynamic(this, &ThisClass::HandleSectionSelected);
		ReadingButtons[Category].Reset();
		ReadingSectionIndices[Category].Reset();
	}
	Super::NativeDestruct();
}

void UKnightCodexWidget::Open()
{
	LoadedData = CodexData.LoadSynchronous();
	if (!LoadedData)
	{
		LoadedData = LoadObject<UKnightCodexData>(nullptr, TEXT("/Game/Knight/Data/DA_Codex.DA_Codex"));
	}
	if (!LoadedData) UE_LOG(Logknight, Warning, TEXT("Codex data asset is missing."));
	if (Text_WorldTitle) Text_WorldTitle->SetText(LoadedData && !LoadedData->WorldTitle.IsEmpty() ? LoadedData->WorldTitle : LOCTEXT("World", "세계관"));
	if (Text_WorldBody) Text_WorldBody->SetText(OrPending(LoadedData ? LoadedData->WorldBody : FText::GetEmpty()));
	if (Text_MechTitle) Text_MechTitle->SetText(LoadedData && !LoadedData->MechTitle.IsEmpty() ? LoadedData->MechTitle : LOCTEXT("Mech", "기체 설명"));
	if (Text_MechBody) Text_MechBody->SetText(OrPending(LoadedData ? LoadedData->MechBody : FText::GetEmpty()));
	RebuildTextPage(TEXT("Box_WorldEntries"), Text_WorldBody,
		Text_WorldTitle ? Text_WorldTitle->GetText() : LOCTEXT("World", "세계관"),
		LoadedData ? LoadedData->WorldSections : TArray<FKnightCodexTextSection>(), 0);
	RebuildTextPage(TEXT("Box_MechEntries"), Text_MechBody,
		Text_MechTitle ? Text_MechTitle->GetText() : LOCTEXT("Mech", "기체 설명"),
		LoadedData ? LoadedData->MechSections : TArray<FKnightCodexTextSection>(), 1);
	SelectedBossId = NAME_None;
	RebuildBosses();
	SelectCategory(0);
}

void UKnightCodexWidget::RequestClose() { OnClosed.Broadcast(); }
void UKnightCodexWidget::HandleWorld() { SelectCategory(0); }
void UKnightCodexWidget::HandleMech() { SelectCategory(1); }
void UKnightCodexWidget::HandleMonster() { SelectCategory(2); }
void UKnightCodexWidget::HandleBossSelected(FName BossId) { SelectBoss(BossId); }

bool UKnightCodexWidget::IsBossUnlocked(const FKnightCodexBossEntry& Boss) const
{
	const UknightProgressionSubsystem* Progression = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>() : nullptr;
	return Progression && !Boss.RequiredQuestId.IsNone() && Progression->IsQuestCompleted(Boss.RequiredQuestId);
}

void UKnightCodexWidget::HandleProgressionChanged()
{
	if (LoadedData) RefreshUnlockStates();
}

void UKnightCodexWidget::HandleActiveSlotChanged(const FString& SlotId)
{
	if (LoadedData) RefreshUnlockStates();
}

void UKnightCodexWidget::RefreshUnlockStates()
{
	for (UKnightCodexBossEntryWidget* Entry : GeneratedEntries)
	{
		const FKnightCodexBossEntry* Boss = Entry ? FindBoss(Entry->GetBossId()) : nullptr;
		if (!Boss) continue;
		const bool bUnlocked = IsBossUnlocked(*Boss);
		const FText Name = !Boss->DisplayNameOverride.IsEmpty() ? Boss->DisplayNameOverride
			: Boss->MonsterDefinition && !Boss->MonsterDefinition->DisplayName.IsEmpty() ? Boss->MonsterDefinition->DisplayName : FText::FromName(Boss->BossId);
		Entry->Configure(Boss->BossId, Name, bUnlocked ? Boss->ListIcon.LoadSynchronous() : nullptr, bUnlocked);
	}
	RefreshBoss();
}

void UKnightCodexWidget::RebuildTextPage(FName ListName, UTextBlock* LegacyBody,
	const FText& Title, const TArray<FKnightCodexTextSection>& Sections, int32 Category)
{
	UVerticalBox* List = Cast<UVerticalBox>(GetWidgetFromName(ListName));
	if (!List || !WidgetTree || Category < 0 || Category > 1) return;
	for (const TWeakObjectPtr<UKnightCodexSectionButton>& Button : ReadingButtons[Category])
		if (Button.IsValid()) Button->OnSectionSelected.RemoveDynamic(this, &ThisClass::HandleSectionSelected);
	ReadingButtons[Category].Reset();
	ReadingSectionIndices[Category].Reset();
	ReadingPageTitles[Category] = Title;
	List->ClearChildren();
	// An existing source record may be internal-only. It stays in DA_Codex; the reader never renders it.
	for (int32 SourceIndex = 0; SourceIndex < Sections.Num(); ++SourceIndex)
	{
		const FKnightCodexTextSection& Section = Sections[SourceIndex];
		if (Section.bShowInReader && !Section.GetReaderBody().IsEmpty()) ReadingSectionIndices[Category].Add(SourceIndex);
	}
	// Old assets with only WorldBody / MechBody keep their single readable entry.
	if (Sections.IsEmpty()) ReadingSectionIndices[Category].Add(INDEX_NONE);
	for (int32 Index = 0; Index < ReadingSectionIndices[Category].Num(); ++Index)
	{
		const int32 SourceIndex = ReadingSectionIndices[Category][Index];
		const FText EntryName = Sections.IsValidIndex(SourceIndex) ? Sections[SourceIndex].GetReaderHeading() : Title;
		UKnightCodexSectionButton* Entry = WidgetTree->ConstructWidget<UKnightCodexSectionButton>();
		Entry->SetSection(Category, Index);
		Entry->OnSectionSelected.AddUniqueDynamic(this, &ThisClass::HandleSectionSelected);
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Entry->AddChild(Row);
		if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Row->Slot))
		{
			ButtonSlot->SetPadding(FMargin(18.f, 8.f, 14.f, 8.f));
			ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		KnightCodexPresentation::TextStyle(Label, 17, false);
		Label->SetText(EntryName);
		Label->SetAutoWrapText(false);
		Label->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label);
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		USizeBox* Height = WidgetTree->ConstructWidget<USizeBox>();
		Height->SetHeightOverride(62.f);
		Height->AddChild(Entry);
		List->AddChildToVerticalBox(Height)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		ReadingButtons[Category].Add(Entry);
	}
	if (ReadingButtons[Category].IsEmpty())
	{
		if (LegacyBody) LegacyBody->SetText(LOCTEXT("NoReaderRecords", "등록된 기록이 없습니다."));
		UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>();
		KnightCodexPresentation::TextStyle(Empty, 16, false, true);
		Empty->SetText(LOCTEXT("NoReaderRecords", "등록된 기록이 없습니다."));
		List->AddChildToVerticalBox(Empty);
	}
	else SelectReadingSection(Category, 0);
}

void UKnightCodexWidget::HandleSectionSelected(int32 Category, int32 SectionIndex)
{
	if (Category == ActiveCategory) SelectReadingSection(Category, SectionIndex);
}

void UKnightCodexWidget::SelectReadingSection(int32 Category, int32 SectionIndex)
{
	if (Category < 0 || Category > 1 || !ReadingButtons[Category].IsValidIndex(SectionIndex)) return;
	for (int32 Index = 0; Index < ReadingButtons[Category].Num(); ++Index)
		if (UKnightCodexSectionButton* Button = ReadingButtons[Category][Index].Get()) Button->SetRecordState(Index == SectionIndex);
	UTextBlock* Title = Category == 0 ? Text_WorldTitle : Text_MechTitle;
	UTextBlock* Body = Category == 0 ? Text_WorldBody : Text_MechBody;
	const TArray<FKnightCodexTextSection>* Sections = LoadedData ? (Category == 0 ? &LoadedData->WorldSections : &LoadedData->MechSections) : nullptr;
	const int32 SourceIndex = ReadingSectionIndices[Category][SectionIndex];
	if (Sections && Sections->IsValidIndex(SourceIndex))
	{
		if (Title) Title->SetText((*Sections)[SourceIndex].GetReaderHeading());
		if (Body) Body->SetText((*Sections)[SourceIndex].GetReaderBody());
	}
	else
	{
		if (Title) Title->SetText(ReadingPageTitles[Category]);
		if (Body) Body->SetText(OrPending(LoadedData ? (Category == 0 ? LoadedData->WorldBody : LoadedData->MechBody) : FText::GetEmpty()));
	}
	// The selected title stays outside this scroll box. Every record opens at its first paragraph.
	ResetReadingScroll(Category == 0 ? TEXT("Scroll_World") : TEXT("Scroll_Mech"));
	if (UScrollBox* List = Cast<UScrollBox>(GetWidgetFromName(Category == 0 ? TEXT("Scroll_WorldEntries") : TEXT("Scroll_MechEntries"))))
		List->ScrollWidgetIntoView(ReadingButtons[Category][SectionIndex].Get(), false);
}

void UKnightCodexWidget::ResetReadingScroll(FName ScrollName)
{
	if (UScrollBox* Scroll = Cast<UScrollBox>(GetWidgetFromName(ScrollName))) Scroll->ScrollToStart();
}

void UKnightCodexWidget::SelectCategory(int32 Category)
{
	if (Category < 0 || Category > 2) return;
	ActiveCategory = Category;
	if (Switcher_Content) Switcher_Content->SetActiveWidgetIndex(Category);
	if (UWidgetSwitcher* EntrySwitcher = Cast<UWidgetSwitcher>(GetWidgetFromName(TEXT("Switcher_Entries"))))
		EntrySwitcher->SetActiveWidgetIndex(Category);
	if (Category < 2) SelectReadingSection(Category, 0);
	if (Category == 0) { ResetReadingScroll(TEXT("Scroll_World")); ResetReadingScroll(TEXT("Scroll_WorldEntries")); }
	else if (Category == 1) { ResetReadingScroll(TEXT("Scroll_Mech")); ResetReadingScroll(TEXT("Scroll_MechEntries")); }
	else ResetReadingScroll(TEXT("Scroll_BossDetail"));
	UButton* Categories[] = {Button_World, Button_Mech, Button_Monster};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (UKnightCodexArchiveButton* Button = Cast<UKnightCodexArchiveButton>(Categories[Index])) Button->SetRecordState(Index == Category);
		else if (Categories[Index]) Categories[Index]->SetBackgroundColor(Index == Category ? SelectedColor : NormalColor);
	}
	if (UTextBlock* Heading = Cast<UTextBlock>(GetWidgetFromName(TEXT("Label_CodexEntries"))))
		Heading->SetText(Category == 0 ? LOCTEXT("WorldRecords", "세계관 기록") : Category == 1 ? LOCTEXT("MechRecords", "기체 기록") : LOCTEXT("MonsterRecords", "몬스터 기록"));
	if (Category == 2 && SelectedBossId.IsNone() && !GeneratedEntries.IsEmpty())
	{
		SelectBoss(GeneratedEntries[0]->GetBossId());
	}
	if (UButton* Focus = GetFocusButton()) Focus->SetUserFocus(GetOwningPlayer());
}

UButton* UKnightCodexWidget::GetFocusButton() const
{
	switch (ActiveCategory)
	{
	case 1: return Button_Mech;
	case 2: return Button_Monster;
	default: return Button_World;
	}
}

const FKnightCodexBossEntry* UKnightCodexWidget::FindBoss(FName BossId) const
{
	if (!LoadedData) return nullptr;
	return LoadedData->Bosses.FindByPredicate([BossId](const FKnightCodexBossEntry& Entry) { return Entry.BossId == BossId; });
}

void UKnightCodexWidget::RebuildBosses()
{
	for (UKnightCodexBossEntryWidget* Entry : GeneratedEntries)
	{
		if (Entry) Entry->OnSelected.RemoveDynamic(this, &ThisClass::HandleBossSelected);
	}
	GeneratedEntries.Reset();
	if (Box_Bosses) Box_Bosses->ClearChildren();
	TSet<FName> SeenIds;
	if (LoadedData && Box_Bosses && GetOwningPlayer())
	{
		for (const FKnightCodexBossEntry& Boss : LoadedData->Bosses)
		{
			if (Boss.BossId.IsNone() || SeenIds.Contains(Boss.BossId))
			{
				UE_LOG(Logknight, Warning, TEXT("Codex skipped empty or duplicate boss ID %s."), *Boss.BossId.ToString());
				continue;
			}
			SeenIds.Add(Boss.BossId);
			TSubclassOf<UKnightCodexBossEntryWidget> Class = BossEntryClass;
			if (!Class) Class = LoadClass<UKnightCodexBossEntryWidget>(nullptr, TEXT("/Game/Knight/UI/Menu/Codex/WBP_KnightCodexBossEntry.WBP_KnightCodexBossEntry_C"));
			if (!Class) continue;
			UKnightCodexBossEntryWidget* Entry = CreateWidget<UKnightCodexBossEntryWidget>(GetOwningPlayer(), Class);
			if (!Entry) continue;
			const FText Name = !Boss.DisplayNameOverride.IsEmpty() ? Boss.DisplayNameOverride
				: Boss.MonsterDefinition && !Boss.MonsterDefinition->DisplayName.IsEmpty() ? Boss.MonsterDefinition->DisplayName : FText::FromName(Boss.BossId);
			const bool bUnlocked = IsBossUnlocked(Boss);
			Entry->Configure(Boss.BossId, Name, bUnlocked ? Boss.ListIcon.LoadSynchronous() : nullptr, bUnlocked);
			Entry->OnSelected.AddUniqueDynamic(this, &ThisClass::HandleBossSelected);
			Box_Bosses->AddChildToVerticalBox(Entry)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
			GeneratedEntries.Add(Entry);
		}
	}
	if (Text_NoBosses) Text_NoBosses->SetVisibility(GeneratedEntries.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	ClearBossDetail();
}

bool UKnightCodexWidget::SelectBoss(FName BossId)
{
	if (!FindBoss(BossId)) return false;
	SelectedBossId = BossId;
	RefreshBoss();
	ResetReadingScroll(TEXT("Scroll_BossDetail"));
	return true;
}

void UKnightCodexWidget::ClearBossDetail()
{
	SetBossDetailLocked(false);
	if (Text_BossName) Text_BossName->SetText(LOCTEXT("ChooseBoss", "보스를 선택해 주세요"));
	if (Text_BossDescription) Text_BossDescription->SetText(FText::GetEmpty());
	if (Text_BossExtra) Text_BossExtra->SetVisibility(ESlateVisibility::Collapsed);
	SetImage(Image_Portrait, nullptr, GetWidgetFromName(TEXT("Size_Portrait")));
	if (Text_NoPortrait) Text_NoPortrait->SetVisibility(ESlateVisibility::Collapsed);
	if (Box_Drops) Box_Drops->ClearChildren();
	if (Text_NoDrops) Text_NoDrops->SetVisibility(ESlateVisibility::Collapsed);
}

void UKnightCodexWidget::SetBossDetailLocked(bool bLocked)
{
	if (UWidget* Label = GetWidgetFromName(TEXT("Label_Drops")))
		Label->SetVisibility(bLocked ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UKnightCodexWidget::RefreshBoss()
{
	const FKnightCodexBossEntry* Boss = FindBoss(SelectedBossId);
	if (!Boss) { ClearBossDetail(); return; }
	for (UKnightCodexBossEntryWidget* Entry : GeneratedEntries)
	{
		if (Entry) Entry->SetSelected(Entry->GetBossId() == SelectedBossId);
	}
	const FText Name = !Boss->DisplayNameOverride.IsEmpty() ? Boss->DisplayNameOverride
		: Boss->MonsterDefinition && !Boss->MonsterDefinition->DisplayName.IsEmpty() ? Boss->MonsterDefinition->DisplayName : FText::FromName(Boss->BossId);
	if (Text_BossName) Text_BossName->SetText(Name);
	const bool bLocked = !IsBossUnlocked(*Boss);
	SetBossDetailLocked(bLocked);
	if (bLocked)
	{
		if (Text_BossDescription) Text_BossDescription->SetText(LOCTEXT("QuestLocked", "관련 퀘스트 완료 시 공개됩니다."));
		if (Text_BossExtra) Text_BossExtra->SetVisibility(ESlateVisibility::Collapsed);
		SetImage(Image_Portrait, nullptr, GetWidgetFromName(TEXT("Size_Portrait")));
		if (Text_NoPortrait)
		{
			Text_NoPortrait->SetText(LOCTEXT("IllustrationLocked", "미해금 기록"));
			Text_NoPortrait->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		if (Box_Drops) Box_Drops->ClearChildren();
		if (Text_NoDrops) Text_NoDrops->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	if (Text_BossDescription) Text_BossDescription->SetText(OrPending(Boss->bUseReaderText ? Boss->ReaderDescription : Boss->Description));
	if (Text_BossExtra)
	{
		const FText& Extra = Boss->bUseReaderText ? Boss->ReaderAdditionalDescription : Boss->AdditionalDescription;
		Text_BossExtra->SetText(Extra);
		Text_BossExtra->SetVisibility(Extra.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	UTexture2D* Portrait = Boss->Portrait.LoadSynchronous();
	SetImage(Image_Portrait, Portrait, GetWidgetFromName(TEXT("Size_Portrait")));
	if (Text_NoPortrait)
	{
		Text_NoPortrait->SetText(LOCTEXT("IllustrationUnavailable", "등록된 이미지가 없습니다."));
		Text_NoPortrait->SetVisibility(Portrait ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (Box_Drops) Box_Drops->ClearChildren();
	bool bDataUnavailable = false;
	bool bFoundQuest = false;
	TMap<FPrimaryAssetId, FQuestDropGroup> QuestDrops;
	const UKnightQuestCatalog* Catalog = UKnightQuestSettings::LoadCatalog();
	if (!Catalog)
	{
		bDataUnavailable = true;
		UE_LOG(Logknight, Warning, TEXT("Codex quest catalog could not be loaded for %s."), *Boss->BossId.ToString());
	}
	else
	{
		TArray<UKnightQuestDefinition*> Quests;
		Catalog->GetQuestDefinitions(Quests);
		// The catalog lookup excludes null, empty and duplicated IDs. These are bad data, not empty rewards.
		bDataUnavailable = Quests.Num() != Catalog->Quests.Num();
		Quests.Sort([](const UKnightQuestDefinition& A, const UKnightQuestDefinition& B)
		{
			if (A.Difficulty != B.Difficulty) return A.Difficulty < B.Difficulty;
			return A.QuestId.ToString() < B.QuestId.ToString();
		});
		for (const UKnightQuestDefinition* Quest : Quests)
		{
			if (Quest->BossId != Boss->BossId) continue;
			// BossId is the stable quest/Codex key. Some planned Hard quests have no boss class/definition yet.
			if (!Boss->MonsterDefinition || (Quest->BossDefinition && Quest->BossDefinition != Boss->MonsterDefinition))
			{
				bDataUnavailable = true;
				UE_LOG(Logknight, Warning, TEXT("Codex %s has a quest with a mismatched monster definition: %s"),
					*Boss->BossId.ToString(), *Quest->GetPathName());
				continue;
			}
			bFoundQuest = true;
			const FText Difficulty = Quest->Difficulty == EKnightQuestDifficulty::Hard
				? LOCTEXT("HardDifficulty", "어려움") : LOCTEXT("NormalDifficulty", "보통");
			auto AddReward = [&](const FknightRaidReward& Reward, const FText& Source)
			{
				const UknightItemDefinition* Item = Reward.Item;
				const FPrimaryAssetId ItemId = Item ? Item->GetPrimaryAssetId() : FPrimaryAssetId();
				if (!ItemId.IsValid())
				{
					bDataUnavailable = true;
					UE_LOG(Logknight, Warning, TEXT("Codex quest %s has an unavailable reward item."), *Quest->GetPathName());
					return;
				}
				FQuestDropGroup& Group = QuestDrops.FindOrAdd(ItemId);
				Group.Item = Item;
				Group.Sources.Add(FText::Format(LOCTEXT("QuestRewardSource", "{0} · {1} · {2} · 확률 {3}"),
					Difficulty, Source, RewardCount(Reward), FText::AsPercent(Reward.Chance)));
			};
			for (const FknightRaidReward& Reward : Quest->ClearRewards)
			{
				AddReward(Reward, LOCTEXT("ClearSource", "완료"));
			}
			for (const FKnightQuestPartBreakReward& Entry : Quest->PartBreakRewards)
			{
				if (!Entry.Reward.RequiredBrokenPart.IsValid())
				{
					bDataUnavailable = true;
					continue;
				}
				AddReward(Entry.Reward, PartSource(Entry.Reward.RequiredBrokenPart));
			}
		}
	}
	const UknightRaidDefinition* Raid = Boss->RaidDefinition;
	const bool bMatchingRaid = Raid && (!Raid->TargetMonster || Raid->TargetMonster == Boss->MonsterDefinition);
	if (!bFoundQuest && Catalog && !bDataUnavailable && Raid && !bMatchingRaid)
	{
		bDataUnavailable = true;
		UE_LOG(Logknight, Warning, TEXT("Codex %s has a raid targeting a different monster; rewards hidden."), *Boss->BossId.ToString());
	}
	if (Box_Drops && GetOwningPlayer())
	{
		TSubclassOf<UKnightCodexDropEntryWidget> Class = DropEntryClass;
		if (!Class) Class = LoadClass<UKnightCodexDropEntryWidget>(nullptr, TEXT("/Game/Knight/UI/Menu/Codex/WBP_KnightCodexDropEntry.WBP_KnightCodexDropEntry_C"));
		if (Class)
		{
			if (bFoundQuest)
			{
				TArray<FPrimaryAssetId> ItemIds;
				QuestDrops.GetKeys(ItemIds);
				ItemIds.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B) { return A.ToString() < B.ToString(); });
				for (const FPrimaryAssetId& ItemId : ItemIds)
				{
					const FQuestDropGroup& Group = QuestDrops.FindChecked(ItemId);
					UKnightCodexDropEntryWidget* Row = CreateWidget<UKnightCodexDropEntryWidget>(GetOwningPlayer(), Class);
					if (Row) { Row->ConfigureQuestDrop(Group.Item, Group.Sources); Box_Drops->AddChildToVerticalBox(Row); }
				}
			}
			else if (Catalog && !bDataUnavailable && bMatchingRaid)
			{
				for (const FknightRaidReward& Reward : Raid->Rewards)
				{
					UKnightCodexDropEntryWidget* Row = CreateWidget<UKnightCodexDropEntryWidget>(GetOwningPlayer(), Class);
					if (Row) { Row->Configure(Reward); Box_Drops->AddChildToVerticalBox(Row); }
				}
			}
		}
		else bDataUnavailable = true;
	}
	if (Text_NoDrops)
	{
		Text_NoDrops->SetText(bDataUnavailable ? LOCTEXT("DropsUnavailable", "일부 보상 정보를 불러올 수 없습니다.")
			: LOCTEXT("NoDrops", "획득 가능한 아이템이 없습니다."));
		Text_NoDrops->SetVisibility(bDataUnavailable || !Box_Drops || Box_Drops->GetChildrenCount() == 0
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

#undef LOCTEXT_NAMESPACE
