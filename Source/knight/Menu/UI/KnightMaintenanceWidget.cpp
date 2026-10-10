#include "Menu/UI/KnightMaintenanceWidget.h"

#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "CommonButtonBase.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "Item/knightEquipmentData.h"
#include "Item/knightEquipmentStats.h"
#include "Item/knightPartsComponent.h"
#include "Menu/UI/KnightMaintenanceEntryWidget.h"
#include "Progression/knightProgressionSubsystem.h"
#include "System/knightGameplayTags.h"

namespace
{
	FText PartName(const UknightPartItemDefinition* Part)
	{
		return Part ? (Part->DisplayName.IsEmpty() ? FText::FromString(Part->GetName()) : Part->DisplayName) : INVTEXT("장비 선택");
	}
	FString StatLine(const FknightPartStatModifier& Mod)
	{
		return FString::Printf(TEXT("%s  %s%.2f"), *Mod.Attribute.GetName(), Mod.Value >= 0 ? TEXT("+") : TEXT(""), Mod.Value);
	}
	FString StatsText(const FknightEquipmentStats& Stats)
	{
		if (!Stats.bComplete) return FString::Printf(TEXT("계산 불가: %s"), *Stats.IncompleteReason);
		const TPair<FGameplayAttribute, const TCHAR*> Rows[] = {
			{ UknightCoreAttributeSet::GetMaxHealthAttribute(), TEXT("최대 체력") },
			{ UknightAttributeSet::GetMaxBoostGaugeAttribute(), TEXT("최대 부스트") },
			{ UknightCoreAttributeSet::GetAttackPowerAttribute(), TEXT("공격력") },
			{ UknightCoreAttributeSet::GetDefenseAttribute(), TEXT("방어력") },
			{ UknightCoreAttributeSet::GetMaxPoiseAttribute(), TEXT("강인도") },
			{ UknightCoreAttributeSet::GetCriticalChanceAttribute(), TEXT("치명타 확률") },
			{ UknightAttributeSet::GetLoadCapacityAttribute(), TEXT("적재 한도") } };
		FString Text;
		for (const auto& Row : Rows) Text += FString::Printf(TEXT("%s  %.2f\n"), Row.Value, Stats.Values.FindRef(Row.Key));
		return Text;
	}
}

TSubclassOf<UKnightMaintenanceWidget> UKnightMaintenanceWidget::GetSharedWidgetClass()
{
	return LoadClass<UKnightMaintenanceWidget>(nullptr,
		TEXT("/Game/Knight/UI/Menu/Maintenance/WBP_KnightMaintenance.WBP_KnightMaintenance_C"));
}

void UKnightMaintenanceWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Btn_Craft) Btn_Craft->OnClicked().AddWeakLambda(this, [this]() { SetCategory(0); });
	if (Btn_Weapon) Btn_Weapon->OnClicked().AddWeakLambda(this, [this]() { SetCategory(1); });
	if (Btn_Armor) Btn_Armor->OnClicked().AddWeakLambda(this, [this]() { SetCategory(2); });
	if (Btn_Head) Btn_Head->OnClicked().AddWeakLambda(this, [this]() { SetSlot(knightGameplayTags::Equipment_Slot_Head); });
	if (Btn_Torso) Btn_Torso->OnClicked().AddWeakLambda(this, [this]() { SetSlot(knightGameplayTags::Equipment_Slot_Torso); });
	if (Btn_Legs) Btn_Legs->OnClicked().AddWeakLambda(this, [this]() { SetSlot(knightGameplayTags::Equipment_Slot_Legs); });
	if (Btn_Arms) Btn_Arms->OnClicked().AddWeakLambda(this, [this]() { SetSlot(knightGameplayTags::Equipment_Slot_Arms); });
	if (Btn_SlotWeapon) Btn_SlotWeapon->OnClicked().AddWeakLambda(this, [this]() { SetSlot(knightGameplayTags::Equipment_Slot_Weapon); });
	if (Btn_PageLeft) Btn_PageLeft->OnClicked().AddWeakLambda(this, [this]() { CyclePage(-1); });
	if (Btn_PageRight) Btn_PageRight->OnClicked().AddWeakLambda(this, [this]() { CyclePage(1); });
	if (Btn_Preview) Btn_Preview->OnClicked().AddWeakLambda(this, [this]() { TogglePreview(); });
	if (Btn_Execute) Btn_Execute->OnClicked().AddWeakLambda(this, [this]() { ExecuteSelection(); });
	if (Btn_Unequip) Btn_Unequip->OnClicked().AddWeakLambda(this, [this]() { UnequipSelection(); });
	if (UknightProgressionSubsystem* Progression = GetProgression())
		Progression->OnProgressionChanged.AddUniqueDynamic(this, &ThisClass::HandleProgressionChanged);
}

void UKnightMaintenanceWidget::NativeDestruct()
{
	if (UknightProgressionSubsystem* Progression = GetProgression())
		Progression->OnProgressionChanged.RemoveDynamic(this, &ThisClass::HandleProgressionChanged);
	Super::NativeDestruct();
}

UknightProgressionSubsystem* UKnightMaintenanceWidget::GetProgression() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>() : nullptr;
}

void UKnightMaintenanceWidget::Open()
{
	bPreview = false;
	Page = 0;
	SetCategory(0);
	CyclePage(0);
}

void UKnightMaintenanceWidget::Close()
{
	bPreview = false;
	SelectedPart = nullptr;
	SelectedRecipe = nullptr;
}

UWidget* UKnightMaintenanceWidget::GetFocusButton() const { return Btn_Craft; }

void UKnightMaintenanceWidget::SetCategory(int32 NewCategory)
{
	Category = FMath::Clamp(NewCategory, 0, 2);
	Slot = Category == 1 ? knightGameplayTags::Equipment_Slot_Weapon : knightGameplayTags::Equipment_Slot_Head;
	bPreview = false;
	SelectedPart = nullptr;
	SelectedRecipe = nullptr;
	if (Btn_SlotWeapon) Btn_SlotWeapon->SetVisibility(Category == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	for (UCommonButtonBase* Button : { Btn_Head.Get(), Btn_Torso.Get(), Btn_Legs.Get(), Btn_Arms.Get() })
		if (Button) Button->SetVisibility(Category == 1 ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (Btn_Craft) Btn_Craft->SetRenderOpacity(Category == 0 ? 1.f : 0.55f);
	if (Btn_Weapon) Btn_Weapon->SetRenderOpacity(Category == 1 ? 1.f : 0.55f);
	if (Btn_Armor) Btn_Armor->SetRenderOpacity(Category == 2 ? 1.f : 0.55f);
	RebuildList();
}

void UKnightMaintenanceWidget::SetSlot(FGameplayTag NewSlot)
{
	if (Category == 1 || (Category == 2 && NewSlot == knightGameplayTags::Equipment_Slot_Weapon)) return;
	Slot = NewSlot;
	bPreview = false;
	SelectedPart = nullptr;
	SelectedRecipe = nullptr;
	RebuildList();
}

void UKnightMaintenanceWidget::RebuildList()
{
	if (!List_Items) return;
	List_Items->ClearChildren();
	Entries.Reset();
	const auto* Catalog = UknightEquipmentSettings::LoadCatalog();
	UknightProgressionSubsystem* Progression = GetProgression();
	const knightEquipmentStats::FLoadout EffectiveLoadout = knightEquipmentStats::ReadLoadout(Progression, Catalog);
	const TSubclassOf<UKnightMaintenanceEntryWidget> EntryClass = UKnightMaintenanceEntryWidget::GetSharedWidgetClass();
	if (!EntryClass) { if (Txt_Status) Txt_Status->SetText(INVTEXT("장비 목록 위젯이 없습니다.")); return; }
	auto AddEntry = [this, EntryClass, Progression, &EffectiveLoadout](const UknightPartItemDefinition* Part, const UknightCraftRecipeDefinition* Recipe)
	{
		if (!Part) return;
		UKnightMaintenanceEntryWidget* Entry = CreateWidget<UKnightMaintenanceEntryWidget>(GetOwningPlayer(), EntryClass);
		if (!Entry) return;
		Entry->SetEntry(Part, Recipe, EffectiveLoadout.FindRef(Part->SlotTag) == Part,
			Progression ? Progression->GetItemCount(Part) : 0);
		Entry->OnClicked().AddUObject(this, &ThisClass::HandleEntryClicked, Entry);
		List_Items->AddChild(Entry);
		Entries.Add(Entry);
	};
	if (Category == 0 && Catalog)
	{
		for (const UknightCraftRecipeDefinition* Recipe : Catalog->Recipes)
			if (Recipe && Recipe->Result && Recipe->Result->SlotTag == Slot) AddEntry(Recipe->Result, Recipe);
	}
	else if (Category != 0 && Progression)
	{
		TSet<const UknightPartItemDefinition*> Seen;
		TArray<FknightItemStack> Owned;
		Progression->GetOwnedItems(Owned);
		for (const FknightItemStack& Stack : Owned)
		{
			const UknightPartItemDefinition* Part = Cast<UknightPartItemDefinition>(Stack.Item);
			if (Part && Part->SlotTag == Slot && Stack.Count > 0 && !Seen.Contains(Part))
			{
				Seen.Add(Part);
				AddEntry(Part, nullptr);
			}
		}
		if (const UknightPartItemDefinition* Equipped = EffectiveLoadout.FindRef(Slot))
			if (!Seen.Contains(Equipped)) AddEntry(Equipped, nullptr);
	}
	for (UCommonButtonBase* Button : { Btn_Head.Get(), Btn_Torso.Get(), Btn_Legs.Get(), Btn_Arms.Get(), Btn_SlotWeapon.Get() })
		if (Button) Button->SetRenderOpacity((Button == Btn_Head && Slot == knightGameplayTags::Equipment_Slot_Head)
			|| (Button == Btn_Torso && Slot == knightGameplayTags::Equipment_Slot_Torso)
			|| (Button == Btn_Legs && Slot == knightGameplayTags::Equipment_Slot_Legs)
			|| (Button == Btn_Arms && Slot == knightGameplayTags::Equipment_Slot_Arms)
			|| (Button == Btn_SlotWeapon && Slot == knightGameplayTags::Equipment_Slot_Weapon) ? 1.f : 0.55f);
	if (Txt_Status) Txt_Status->SetText(Entries.IsEmpty() ? INVTEXT("표시할 장비가 없습니다.") : FText::GetEmpty());
	RefreshDetails();
}

void UKnightMaintenanceWidget::HandleEntryClicked(UKnightMaintenanceEntryWidget* Entry)
{
	SelectedPart = Entry ? Entry->GetPart() : nullptr;
	SelectedRecipe = Entry ? Entry->GetRecipe() : nullptr;
	bPreview = false;
	for (UKnightMaintenanceEntryWidget* Candidate : Entries)
		if (Candidate) Candidate->SetRenderOpacity(Candidate == Entry ? 1.f : 0.65f);
	RefreshDetails();
}

void UKnightMaintenanceWidget::CyclePage(int32 Direction)
{
	Page = (Page + Direction + 2) % 2;
	if (Switcher_Info) Switcher_Info->SetActiveWidgetIndex(Page);
	if (Txt_Page) Txt_Page->SetText(Page == 0 ? INVTEXT("1/2") : INVTEXT("2/2"));
}

void UKnightMaintenanceWidget::TogglePreview()
{
	if (!SelectedPart) return;
	bPreview = !bPreview;
	RefreshDetails();
}

void UKnightMaintenanceWidget::RefreshDetails()
{
	UknightProgressionSubsystem* Progression = GetProgression();
	const UknightMaintenanceCatalog* Catalog = UknightEquipmentSettings::LoadCatalog();
	const knightEquipmentStats::FLoadout Loadout = knightEquipmentStats::ReadLoadout(Progression, Catalog);
	const FknightEquipmentStats Current = knightEquipmentStats::Calculate(Loadout, Catalog, Progression);
	if (Txt_CurrentStats) Txt_CurrentStats->SetText(FText::FromString(StatsText(Current)));
	if (Txt_PreviewStats)
	{
		FString PreviewText;
		if (bPreview && SelectedPart)
		{
			auto Changed = Loadout;
			Changed.Add(SelectedPart->SlotTag, SelectedPart);
			const FknightEquipmentStats Predicted = knightEquipmentStats::Calculate(Changed, Catalog, Progression);
			if (Current.bComplete && Predicted.bComplete)
			{
				PreviewText = TEXT("교체 후 예상\n") + StatsText(Predicted) + TEXT("\n변화\n");
				for (const auto& Stat : Predicted.Values)
				{
					const float Difference = Stat.Value - Current.Values.FindRef(Stat.Key);
					if (!FMath::IsNearlyZero(Difference)) PreviewText += FString::Printf(TEXT("%s  %+.2f\n"), *Stat.Key.GetName(), Difference);
				}
			}
			else PreviewText = TEXT("조건부 효과가 있어 정확한 미리보기를 계산할 수 없습니다.");
		}
		if (PreviewText.IsEmpty()) PreviewText = TEXT("장비를 선택하고 N을 눌러 교체 결과를 미리 보세요.");
		Txt_PreviewStats->SetText(FText::FromString(PreviewText));
	}
	if (Txt_PreviewHint) Txt_PreviewHint->SetText(bPreview ? INVTEXT("미리보기 활성 · 실제 장비/세이브는 그대로") : INVTEXT("N · 스탯 미리보기"));
	FString Details = PartName(SelectedPart).ToString();
	if (SelectedPart)
	{
		Details += TEXT("\n") + SelectedPart->Description.ToString() + TEXT("\n\n장비 자체 보너스\n");
		for (const FknightPartStatModifier& Mod : SelectedPart->StatModifiers) Details += StatLine(Mod) + TEXT("\n");
		if (SelectedPart->SetTag.IsValid())
		{
			Details += FString::Printf(TEXT("\n세트 %s · 현재 %d개\n"), *SelectedPart->SetTag.ToString(), Current.SetCounts.FindRef(SelectedPart->SetTag));
			if (Catalog) for (const UknightEquipmentSetDefinition* Set : Catalog->Sets)
			{
				if (!Set || Set->SetTag != SelectedPart->SetTag) continue;
				Details += FString::Printf(TEXT("%s\n%d개 %s [%s]\n%d개 %s [%s]\n"), *Set->DisplayName.ToString(),
						Set->TwoPiece.RequiredCount, *Set->TwoPiece.Description.ToString(),
						Current.SetCounts.FindRef(Set->SetTag) >= Set->TwoPiece.RequiredCount ? TEXT("활성") : TEXT("비활성"),
						Set->FourPiece.RequiredCount, *Set->FourPiece.Description.ToString(),
						Current.SetCounts.FindRef(Set->SetTag) >= Set->FourPiece.RequiredCount ? TEXT("활성") : TEXT("비활성"));
				break;
			}
		}
		if (SelectedRecipe)
		{
			Details += TEXT("\n필요 재료\n");
			for (const FknightItemCost& Cost : SelectedRecipe->Costs)
			{
				Details += FString::Printf(TEXT("%s  %d / %d\n"), Cost.Item ? *Cost.Item->DisplayName.ToString() : TEXT("없는 아이템"),
					Progression && Cost.Item ? Progression->GetItemCount(Cost.Item) : 0, Cost.Count);
			}
			if (!SelectedRecipe->RequiredClearedStageId.IsNone())
				Details += FString::Printf(TEXT("필요 스테이지: %s\n"), *SelectedRecipe->RequiredClearedStageId.ToString());
		}
	}
	if (Txt_Detail) Txt_Detail->SetText(FText::FromString(Details));
	if (Img_Selected)
	{
		if (SelectedPart && SelectedPart->Icon.LoadSynchronous())
		{
			Img_Selected->SetBrushFromTexture(SelectedPart->Icon.Get());
			Img_Selected->SetVisibility(ESlateVisibility::Visible);
		}
		else Img_Selected->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Btn_Execute)
	{
		Btn_Execute->SetVisibility(SelectedPart ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Btn_Execute->SetIsEnabled(!bBusy && SelectedPart && Progression && (SelectedRecipe
			? Progression->CanCraftPart(SelectedRecipe) : Progression->GetItemCount(SelectedPart) > 0));
	}
	if (Btn_Unequip)
	{
		Btn_Unequip->SetVisibility(Category == 0 ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		Btn_Unequip->SetIsEnabled(!bBusy && SelectedPart && Loadout.FindRef(SelectedPart->SlotTag) == SelectedPart);
	}
	if (Btn_Preview) Btn_Preview->SetIsEnabled(SelectedPart != nullptr);
}

void UKnightMaintenanceWidget::ExecuteSelection()
{
	if (bBusy || !SelectedPart) return;
	UknightProgressionSubsystem* Progression = GetProgression();
	if (!Progression) return;
	bBusy = true;
	bool bSuccess = false;
	if (Category == 0) bSuccess = SelectedRecipe && Progression->CraftPart(SelectedRecipe);
	else if (Progression->GetItemCount(SelectedPart) > 0)
	{
		UknightPartsComponent* LiveParts = GetOwningPlayerPawn() ? GetOwningPlayerPawn()->FindComponentByClass<UknightPartsComponent>() : nullptr;
		bool bCanEquip = LiveParts && LiveParts->CanEquipPart(SelectedPart);
		if (!LiveParts)
		{
			// The lobby has no pawn. Check the same current-loadout weight against its capacity before saving.
			const UknightMaintenanceCatalog* Catalog = UknightEquipmentSettings::LoadCatalog();
			if (Catalog)
			{
				const knightEquipmentStats::FLoadout Loadout = knightEquipmentStats::ReadLoadout(Progression, Catalog);
				const FknightEquipmentStats Stats = knightEquipmentStats::Calculate(Loadout, Catalog, Progression);
				float TotalWeight = 0.f;
				for (const auto& Entry : Loadout)
					if (Entry.Value) TotalWeight += Entry.Value->Weight;
				const UknightPartItemDefinition* Replaced = Loadout.FindRef(SelectedPart->SlotTag);
				const float Capacity = Stats.Values.FindRef(UknightAttributeSet::GetLoadCapacityAttribute());
				bCanEquip = Stats.bComplete && knightPartsLoad::FitsLoadCapacity(
					TotalWeight, Replaced ? Replaced->Weight : 0.f, SelectedPart->Weight, Capacity);
			}
		}
		if (bCanEquip)
		{
			const UknightPartItemDefinition* PreviousLivePart = LiveParts ? LiveParts->GetEquippedPart(SelectedPart->SlotTag) : nullptr;
			bSuccess = !LiveParts || LiveParts->EquipPart(SelectedPart, Progression->GetUpgradeLevel(SelectedPart));
			if (bSuccess && !Progression->SetEquippedPart(SelectedPart))
			{
				if (LiveParts)
				{
					if (PreviousLivePart) LiveParts->EquipPart(PreviousLivePart, Progression->GetUpgradeLevel(PreviousLivePart));
					else LiveParts->UnequipSlot(SelectedPart->SlotTag);
				}
				bSuccess = false;
			}
		}
	}
	bBusy = false;
	bPreview = false;
	RebuildList();
	if (Txt_Status) Txt_Status->SetText(bSuccess ? INVTEXT("변경사항을 저장했습니다.") : INVTEXT("요청 실패: 재료·소유 상태·적재 한도를 확인하세요."));
}

void UKnightMaintenanceWidget::UnequipSelection()
{
	if (bBusy || !SelectedPart || Category == 0) return;
	UknightProgressionSubsystem* Progression = GetProgression();
	if (!Progression || knightEquipmentStats::ReadLoadout(Progression, UknightEquipmentSettings::LoadCatalog()).FindRef(SelectedPart->SlotTag) != SelectedPart) return;
	bBusy = true;
	if (UknightPartsComponent* LiveParts = GetOwningPlayerPawn() ? GetOwningPlayerPawn()->FindComponentByClass<UknightPartsComponent>() : nullptr)
		LiveParts->UnequipSlot(SelectedPart->SlotTag);
	Progression->ClearEquippedSlot(SelectedPart->SlotTag);
	bBusy = false;
	bPreview = false;
	RebuildList();
	if (Txt_Status) Txt_Status->SetText(INVTEXT("장착을 해제했습니다."));
}

void UKnightMaintenanceWidget::HandleProgressionChanged()
{
	if (bBusy) return;
	RebuildList();
}
