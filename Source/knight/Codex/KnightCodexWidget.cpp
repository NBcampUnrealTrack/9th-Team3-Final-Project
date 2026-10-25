#include "Codex/KnightCodexWidget.h"

#include "Codex/KnightCodexData.h"
#include "Monster/knightMonsterDefinition.h"
#include "Raid/knightRaidDefinition.h"
#include "knightItemDefinition.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Progression/knightSaveSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
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
	TSubclassOf<UCommonTextStyle> CodexStyle(const TCHAR* Name)
	{
		return LoadClass<UCommonTextStyle>(nullptr,
			*FString::Printf(TEXT("/Game/Knight/UI/Style/%s.%s_C"), Name, Name));
	}
	FText OrPending(const FText& Value) { return Value.IsEmpty() ? LOCTEXT("Pending", "설명 준비 중") : Value; }
	void SetImage(UImage* Image, UTexture2D* Texture)
	{
		if (!Image) return;
		Image->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Texture) Image->SetBrushFromTexture(Texture, true);
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
	SetImage(Image_Icon, bUnlocked ? Icon : nullptr);
	if (Row_LockState) Row_LockState->SetVisibility(bUnlocked ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	else if (Text_LockState) Text_LockState->SetVisibility(bUnlocked ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	SetSelected(bEntrySelected);
}

void UKnightCodexBossEntryWidget::SetSelected(bool bSelected)
{
	bEntrySelected = bSelected;
	if (Button_Select) Button_Select->SetBackgroundColor(bSelected ? SelectedColor : bEntryUnlocked ? NormalColor : LockedColor);
}

void UKnightCodexBossEntryWidget::HandleClicked()
{
	if (!BossId.IsNone()) OnSelected.Broadcast(BossId);
}

void UKnightCodexDropEntryWidget::Configure(const FknightRaidReward& Reward)
{
	const UknightItemDefinition* Item = Reward.Item;
	if (Text_Name) Text_Name->SetText(Item && !Item->DisplayName.IsEmpty() ? Item->DisplayName : LOCTEXT("UnknownItem", "아이템 정보 없음"));
	SetImage(Image_Icon, Item ? Item->Icon.LoadSynchronous() : nullptr);
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
		ReadingAnchors[Category].Reset();
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
	RebuildTextPage(TEXT("Column_World"), TEXT("Box_WorldEntries"), Text_WorldBody,
		Text_WorldTitle ? Text_WorldTitle->GetText() : LOCTEXT("World", "세계관"),
		LoadedData ? LoadedData->WorldSections : TArray<FKnightCodexTextSection>(), 0);
	RebuildTextPage(TEXT("Column_Mech"), TEXT("Box_MechEntries"), Text_MechBody,
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

void UKnightCodexWidget::RebuildTextPage(FName ColumnName, FName ListName, UTextBlock* LegacyBody,
	const FText& Title, const TArray<FKnightCodexTextSection>& Sections, int32 Category)
{
	UVerticalBox* Column = Cast<UVerticalBox>(GetWidgetFromName(ColumnName));
	UVerticalBox* List = Cast<UVerticalBox>(GetWidgetFromName(ListName));
	if (!Column || !List || !WidgetTree || Category < 0 || Category > 1) return;
	for (const TWeakObjectPtr<UKnightCodexSectionButton>& Button : ReadingButtons[Category])
		if (Button.IsValid()) Button->OnSectionSelected.RemoveDynamic(this, &ThisClass::HandleSectionSelected);
	ReadingButtons[Category].Reset();
	ReadingAnchors[Category].Reset();
	List->ClearChildren();
	const FName DynamicName(*FString::Printf(TEXT("Dynamic_%s"), *ColumnName.ToString()));
	UVerticalBox* SectionBox = Cast<UVerticalBox>(GetWidgetFromName(DynamicName));
	if (!SectionBox)
	{
		SectionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), DynamicName);
		Column->AddChildToVerticalBox(SectionBox);
	}
	SectionBox->ClearChildren();
	SectionBox->SetVisibility(Sections.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (LegacyBody) LegacyBody->SetVisibility(Sections.IsEmpty() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	const int32 EntryCount = FMath::Max(1, Sections.Num());
	for (int32 Index = 0; Index < EntryCount; ++Index)
	{
		const FKnightCodexTextSection* SectionPtr = Sections.IsValidIndex(Index) ? &Sections[Index] : nullptr;
		const FText EntryName = SectionPtr && !SectionPtr->Heading.IsEmpty() ? SectionPtr->Heading : Title;
		UKnightCodexSectionButton* Entry = WidgetTree->ConstructWidget<UKnightCodexSectionButton>();
		Entry->SetSection(Category, Index);
		Entry->SetBackgroundColor(NormalColor);
		Entry->OnSectionSelected.AddUniqueDynamic(this, &ThisClass::HandleSectionSelected);
		UCommonTextBlock* Label = WidgetTree->ConstructWidget<UCommonTextBlock>();
		Label->SetText(EntryName);
		Label->SetAutoWrapText(true);
		if (const TSubclassOf<UCommonTextStyle> ButtonStyle = CodexStyle(TEXT("TS_Knight_Button"))) Label->SetStyle(ButtonStyle);
		Entry->AddChild(Label);
		if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Label->Slot)) ButtonSlot->SetPadding(FMargin(12.f, 10.f));
		USizeBox* EntryHeight = WidgetTree->ConstructWidget<USizeBox>();
		EntryHeight->SetHeightOverride(72.f);
		EntryHeight->AddChild(Entry);
		UVerticalBoxSlot* EntrySlot = List->AddChildToVerticalBox(EntryHeight);
		EntrySlot->SetPadding(FMargin(0.f, 0.f, 0.f, 5.f));
		ReadingButtons[Category].Add(Entry);
		if (!SectionPtr)
		{
			ReadingAnchors[Category].Add(Cast<UWidget>(LegacyBody));
			continue;
		}
		const FKnightCodexTextSection& Section = *SectionPtr;
		UWidget* Anchor = nullptr;
		if (!Section.Heading.IsEmpty())
		{
			UCommonTextBlock* Heading = WidgetTree->ConstructWidget<UCommonTextBlock>();
			Heading->SetText(Section.Heading);
			Heading->SetAutoWrapText(true);
			if (const TSubclassOf<UCommonTextStyle> Style = CodexStyle(Section.HeadingLevel >= 3 ? TEXT("TS_Knight_Button") : TEXT("TS_Knight_Subhead")))
				Heading->SetStyle(Style);
			UVerticalBoxSlot* HeadingSlot = SectionBox->AddChildToVerticalBox(Heading);
			HeadingSlot->SetPadding(FMargin(0.f, Section.HeadingLevel >= 3 ? 8.f : 18.f, 0.f, 8.f));
			Anchor = Heading;
		}
		if (!Section.Body.IsEmpty())
		{
			UCommonTextBlock* Body = WidgetTree->ConstructWidget<UCommonTextBlock>();
			Body->SetText(Section.Body);
			Body->SetAutoWrapText(true);
			if (const TSubclassOf<UCommonTextStyle> Style = CodexStyle(TEXT("TS_Knight_Body"))) Body->SetStyle(Style);
			UVerticalBoxSlot* BodySlot = SectionBox->AddChildToVerticalBox(Body);
			BodySlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
			if (!Anchor) Anchor = Body;
		}
		ReadingAnchors[Category].Add(Anchor);
	}
	SelectReadingSection(Category, 0, false);
}

void UKnightCodexWidget::HandleSectionSelected(int32 Category, int32 SectionIndex)
{
	if (Category == ActiveCategory) SelectReadingSection(Category, SectionIndex, true);
}

void UKnightCodexWidget::SelectReadingSection(int32 Category, int32 SectionIndex, bool bScroll)
{
	if (Category < 0 || Category > 1 || !ReadingButtons[Category].IsValidIndex(SectionIndex)) return;
	for (int32 Index = 0; Index < ReadingButtons[Category].Num(); ++Index)
		if (UKnightCodexSectionButton* Button = ReadingButtons[Category][Index].Get())
			Button->SetBackgroundColor(Index == SectionIndex ? SelectedColor : NormalColor);
	if (!bScroll) return;
	UScrollBox* Scroll = Cast<UScrollBox>(GetWidgetFromName(Category == 0 ? TEXT("Scroll_World") : TEXT("Scroll_Mech")));
	if (Scroll && ReadingAnchors[Category].IsValidIndex(SectionIndex))
	{
		if (UWidget* Anchor = ReadingAnchors[Category][SectionIndex].Get()) Scroll->ScrollWidgetIntoView(Anchor);
		else Scroll->ScrollToStart();
	}
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
	if (Category < 2) SelectReadingSection(Category, 0, false);
	if (Category == 0) ResetReadingScroll(TEXT("Scroll_World"));
	else if (Category == 1) ResetReadingScroll(TEXT("Scroll_Mech"));
	else ResetReadingScroll(TEXT("Scroll_BossDetail"));
	if (Button_World) Button_World->SetBackgroundColor(Category == 0 ? SelectedColor : NormalColor);
	if (Button_Mech) Button_Mech->SetBackgroundColor(Category == 1 ? SelectedColor : NormalColor);
	if (Button_Monster) Button_Monster->SetBackgroundColor(Category == 2 ? SelectedColor : NormalColor);
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
			Box_Bosses->AddChildToVerticalBox(Entry);
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
	SetImage(Image_Portrait, nullptr);
	if (Text_NoPortrait) Text_NoPortrait->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (Box_Drops) Box_Drops->ClearChildren();
	if (Text_NoDrops) Text_NoDrops->SetVisibility(ESlateVisibility::Collapsed);
}

void UKnightCodexWidget::SetBossDetailLocked(bool bLocked)
{
	for (const TCHAR* Name : {TEXT("Size_Portrait"), TEXT("Label_Drops")})
	{
		if (UWidget* Widget = GetWidgetFromName(Name))
			Widget->SetVisibility(bLocked ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
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
		SetImage(Image_Portrait, nullptr);
		if (Text_NoPortrait) Text_NoPortrait->SetVisibility(ESlateVisibility::Collapsed);
		if (Box_Drops) Box_Drops->ClearChildren();
		if (Text_NoDrops) Text_NoDrops->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	if (Text_BossDescription) Text_BossDescription->SetText(OrPending(Boss->Description));
	if (Text_BossExtra)
	{
		Text_BossExtra->SetText(Boss->AdditionalDescription);
		Text_BossExtra->SetVisibility(Boss->AdditionalDescription.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	UTexture2D* Portrait = Boss->Portrait.LoadSynchronous();
	SetImage(Image_Portrait, Portrait);
	if (Text_NoPortrait) Text_NoPortrait->SetVisibility(Portrait ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (Box_Drops) Box_Drops->ClearChildren();
	const UknightRaidDefinition* Raid = Boss->RaidDefinition;
	const bool bMatchingRaid = Raid && (!Raid->TargetMonster || Raid->TargetMonster == Boss->MonsterDefinition);
	if (Raid && !bMatchingRaid)
	{
		UE_LOG(Logknight, Warning, TEXT("Codex %s has a raid targeting a different monster; rewards hidden."), *Boss->BossId.ToString());
	}
	if (bMatchingRaid && Box_Drops && GetOwningPlayer())
	{
		TSubclassOf<UKnightCodexDropEntryWidget> Class = DropEntryClass;
		if (!Class) Class = LoadClass<UKnightCodexDropEntryWidget>(nullptr, TEXT("/Game/Knight/UI/Menu/Codex/WBP_KnightCodexDropEntry.WBP_KnightCodexDropEntry_C"));
		if (Class)
		{
			for (const FknightRaidReward& Reward : Raid->Rewards)
			{
				UKnightCodexDropEntryWidget* Row = CreateWidget<UKnightCodexDropEntryWidget>(GetOwningPlayer(), Class);
				if (Row) { Row->Configure(Reward); Box_Drops->AddChildToVerticalBox(Row); }
			}
		}
	}
	if (Text_NoDrops) Text_NoDrops->SetVisibility(Box_Drops && Box_Drops->GetChildrenCount() > 0 ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

#undef LOCTEXT_NAMESPACE
