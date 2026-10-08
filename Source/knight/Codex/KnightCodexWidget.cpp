#include "Codex/KnightCodexWidget.h"

#include "Codex/KnightCodexData.h"
#include "Monster/knightMonsterDefinition.h"
#include "Raid/knightRaidDefinition.h"
#include "knightItemDefinition.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Texture2D.h"
#include "knight.h"

#define LOCTEXT_NAMESPACE "KnightCodex"

namespace
{
	const FLinearColor SelectedColor(FColor(0xFF, 0x7A, 0x1A));
	const FLinearColor NormalColor(FColor(0x35, 0x53, 0x73));
	FText OrPending(const FText& Value) { return Value.IsEmpty() ? LOCTEXT("Pending", "설명 준비 중") : Value; }
	void SetImage(UImage* Image, UTexture2D* Texture)
	{
		if (!Image) return;
		Image->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Texture) Image->SetBrushFromTexture(Texture, true);
	}
}

void UKnightCodexBossEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button_Select) Button_Select->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
}

void UKnightCodexBossEntryWidget::Configure(FName InId, const FText& Name, UTexture2D* Icon)
{
	BossId = InId;
	if (Text_Name) Text_Name->SetText(Name);
	SetImage(Image_Icon, Icon);
}

void UKnightCodexBossEntryWidget::SetSelected(bool bSelected)
{
	if (Button_Select) Button_Select->SetBackgroundColor(bSelected ? SelectedColor : NormalColor);
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
	static TSubclassOf<UKnightCodexWidget> Class = LoadClass<UKnightCodexWidget>(nullptr,
		TEXT("/Game/Knight/UI/Codex/WBP_Codex.WBP_Codex_C"));
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

void UKnightCodexWidget::NativeDestruct()
{
	for (UKnightCodexBossEntryWidget* Entry : GeneratedEntries)
	{
		if (Entry) Entry->OnSelected.RemoveDynamic(this, &ThisClass::HandleBossSelected);
	}
	GeneratedEntries.Reset();
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
	SelectedBossId = NAME_None;
	RebuildBosses();
	SelectCategory(0);
}

void UKnightCodexWidget::RequestClose() { OnClosed.Broadcast(); }
void UKnightCodexWidget::HandleWorld() { SelectCategory(0); }
void UKnightCodexWidget::HandleMech() { SelectCategory(1); }
void UKnightCodexWidget::HandleMonster() { SelectCategory(2); }
void UKnightCodexWidget::HandleBossSelected(FName BossId) { SelectBoss(BossId); }

void UKnightCodexWidget::SelectCategory(int32 Category)
{
	if (Category < 0 || Category > 2) return;
	ActiveCategory = Category;
	if (Switcher_Content) Switcher_Content->SetActiveWidgetIndex(Category);
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

FText UKnightCodexWidget::GetDisplayedBossName() const
{
	return Text_BossName ? Text_BossName->GetText() : FText::GetEmpty();
}

FText UKnightCodexWidget::GetDisplayedBossDescription() const
{
	return Text_BossDescription ? Text_BossDescription->GetText() : FText::GetEmpty();
}

UTexture2D* UKnightCodexWidget::GetDisplayedPortrait() const
{
	return Image_Portrait ? Cast<UTexture2D>(Image_Portrait->GetBrush().GetResourceObject()) : nullptr;
}

FText UKnightCodexWidget::GetDisplayedWorldBody() const
{
	return Text_WorldBody ? Text_WorldBody->GetText() : FText::GetEmpty();
}

int32 UKnightCodexWidget::GetDropRowCount() const
{
	return Box_Drops ? Box_Drops->GetChildrenCount() : 0;
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
			if (!Class) Class = LoadClass<UKnightCodexBossEntryWidget>(nullptr, TEXT("/Game/Knight/UI/Codex/WBP_CodexBossEntry.WBP_CodexBossEntry_C"));
			if (!Class) continue;
			UKnightCodexBossEntryWidget* Entry = CreateWidget<UKnightCodexBossEntryWidget>(GetOwningPlayer(), Class);
			if (!Entry) continue;
			const FText Name = !Boss.DisplayNameOverride.IsEmpty() ? Boss.DisplayNameOverride
				: Boss.MonsterDefinition && !Boss.MonsterDefinition->DisplayName.IsEmpty() ? Boss.MonsterDefinition->DisplayName : FText::FromName(Boss.BossId);
			Entry->Configure(Boss.BossId, Name, Boss.ListIcon.LoadSynchronous());
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
	return true;
}

void UKnightCodexWidget::ClearBossDetail()
{
	if (Text_BossName) Text_BossName->SetText(LOCTEXT("ChooseBoss", "보스를 선택해 주세요"));
	if (Text_BossDescription) Text_BossDescription->SetText(FText::GetEmpty());
	if (Text_BossExtra) Text_BossExtra->SetVisibility(ESlateVisibility::Collapsed);
	SetImage(Image_Portrait, nullptr);
	if (Text_NoPortrait) Text_NoPortrait->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (Box_Drops) Box_Drops->ClearChildren();
	if (Text_NoDrops) Text_NoDrops->SetVisibility(ESlateVisibility::Collapsed);
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
		if (!Class) Class = LoadClass<UKnightCodexDropEntryWidget>(nullptr, TEXT("/Game/Knight/UI/Codex/WBP_CodexDropEntry.WBP_CodexDropEntry_C"));
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
