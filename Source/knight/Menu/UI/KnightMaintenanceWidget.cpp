#include "Menu/UI/KnightMaintenanceWidget.h"

#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "GameFlow/KnightGameFlowSubsystem.h"
#include "GameFlow/KnightStageCatalog.h"
#include "Item/knightEquipmentData.h"
#include "Item/knightEquipmentStats.h"
#include "Item/knightPartsComponent.h"
#include "Menu/UI/KnightMaintenanceEntryWidget.h"
#include "Progression/knightProgressionSubsystem.h"
#include "System/knightGameplayTags.h"

#define LOCTEXT_NAMESPACE "KnightMaintenance"

namespace
{
	// 스타일 가이드 색 (sRGB).
	const FLinearColor Ember(FColor(0xFF, 0x7A, 0x1A));
	const FLinearColor Frost(FColor(0xA8, 0xC8, 0xF0));
	const FLinearColor Mist(FColor(0x8F, 0xA3, 0xBF));
	const FLinearColor DimColor(FColor(0x4A, 0x5B, 0x75));
	const FLinearColor White(FColor(0xF4, 0xF8, 0xFC));
	const FLinearColor Increase(FColor(0x4B, 0xE8, 0x80));
	const FLinearColor Alert(FColor(0xE5, 0x48, 0x4D));

	enum EInfoTab : int32 { InfoSpec = 0, InfoSet = 1, InfoMaterial = 2 };

	struct FStatRow
	{
		FGameplayAttribute Attribute;
		FText Label;
		/** 배율·확률은 % 표기. */
		bool bPercent = false;
	};

	const TArray<FStatRow>& StatRows()
	{
		static const TArray<FStatRow> Rows = {
			{ UknightCoreAttributeSet::GetMaxHealthAttribute(), LOCTEXT("StatMaxHealth", "최대 체력"), false },
			{ UknightAttributeSet::GetMaxBoostGaugeAttribute(), LOCTEXT("StatMaxBoost", "최대 부스트"), false },
			{ UknightCoreAttributeSet::GetAttackPowerAttribute(), LOCTEXT("StatAttack", "공격력"), false },
			{ UknightCoreAttributeSet::GetDefenseAttribute(), LOCTEXT("StatDefense", "방어력"), false },
			{ UknightAttributeSet::GetWalkSpeedAttribute(), LOCTEXT("StatWalkSpeed", "걷기 속도"), true },
			{ UknightAttributeSet::GetRunSpeedAttribute(), LOCTEXT("StatRunSpeed", "달리기 속도"), true },
			{ UknightCoreAttributeSet::GetMaxPoiseAttribute(), LOCTEXT("StatPoise", "강인도"), false },
			{ UknightCoreAttributeSet::GetCriticalChanceAttribute(), LOCTEXT("StatCritical", "치명타 확률"), true },
			{ UknightAttributeSet::GetLoadCapacityAttribute(), LOCTEXT("StatLoad", "적재 한도"), false } };
		return Rows;
	}

	/** 스펙 탭에 없는 스탯의 표시 이름. 속성 이름으로 찾는다. */
	struct FExtraStat
	{
		const TCHAR* Name;
		FText Label;
		bool bPercent;
		/** 시간·회복량은 소수 한 자리. */
		bool bDecimal;
	};

	const TArray<FExtraStat>& ExtraStats()
	{
		static const TArray<FExtraStat> Stats = {
			{ TEXT("MaxHeadHealth"), LOCTEXT("StatHeadHealth", "머리 내구도"), false, false },
			{ TEXT("MaxTorsoHealth"), LOCTEXT("StatTorsoHealth", "몸통 내구도"), false, false },
			{ TEXT("MaxArmsHealth"), LOCTEXT("StatArmsHealth", "팔 내구도"), false, false },
			{ TEXT("MaxLegsHealth"), LOCTEXT("StatLegsHealth", "다리 내구도"), false, false },
			{ TEXT("BoostRecoveryRate"), LOCTEXT("StatBoostRecovery", "부스트 회복"), false, true },
			{ TEXT("BoostRecoveryDelay"), LOCTEXT("StatBoostDelay", "부스트 회복 지연"), false, true },
			{ TEXT("Weight"), LOCTEXT("StatWeight", "중량"), false, false },
			{ TEXT("ActionSpeed"), LOCTEXT("StatActionSpeed", "행동 속도"), true, false },
			{ TEXT("JumpSpeed"), LOCTEXT("StatJumpSpeed", "점프 속도"), true, false },
			{ TEXT("DodgeSpeed"), LOCTEXT("StatDodgeSpeed", "회피 속도"), true, false },
			{ TEXT("DodgeCooldown"), LOCTEXT("StatDodgeCooldown", "회피 대기 시간"), false, true },
			{ TEXT("PerfectGuardWindow"), LOCTEXT("StatPerfectGuard", "완벽 방어 시간"), false, true },
			{ TEXT("RepairEfficiency"), LOCTEXT("StatRepairEfficiency", "수리 효율"), true, false },
			{ TEXT("RepairKitCount"), LOCTEXT("StatRepairKits", "수리 키트"), false, false },
			{ TEXT("PoiseRecoveryRate"), LOCTEXT("StatPoiseRecovery", "강인도 회복"), false, true },
			{ TEXT("StaggerPower"), LOCTEXT("StatStaggerPower", "경직력"), false, false },
			{ TEXT("GuardEfficiency"), LOCTEXT("StatGuardEfficiency", "방어 효율"), true, false },
			{ TEXT("DamageTakenMultiplier"), LOCTEXT("StatDamageTaken", "받는 피해"), true, false },
			{ TEXT("PoiseDamageTakenMultiplier"), LOCTEXT("StatPoiseDamageTaken", "받는 경직 피해"), true, false },
			{ TEXT("CriticalDamageMultiplier"), LOCTEXT("StatCriticalDamage", "치명타 피해"), true, false } };
		return Stats;
	}

	const FStatRow* FindStatRow(const FGameplayAttribute& Attribute)
	{
		return StatRows().FindByPredicate([&Attribute](const FStatRow& Row) { return Row.Attribute == Attribute; });
	}

	const FExtraStat* FindExtraStat(const FGameplayAttribute& Attribute)
	{
		const FString Name = Attribute.GetName();
		return ExtraStats().FindByPredicate([&Name](const FExtraStat& Stat) { return Name == Stat.Name; });
	}

	/** 표시용 정수. % 스탯은 100배 (0.05 -> 5). */
	int32 DisplayNumber(float Value, bool bPercent)
	{
		return FMath::RoundToInt(bPercent ? Value * 100.f : Value);
	}

	FText FormatNumber(int32 Number, bool bPercent, bool bSigned)
	{
		FNumberFormattingOptions Options;
		Options.AlwaysSign = bSigned;
		const FText Text = FText::AsNumber(Number, &Options);
		return bPercent ? FText::Format(LOCTEXT("PercentNumber", "{0}%"), Text) : Text;
	}

	FText StatLabel(const FGameplayAttribute& Attribute)
	{
		if (const FStatRow* Row = FindStatRow(Attribute)) return Row->Label;
		if (const FExtraStat* Extra = FindExtraStat(Attribute)) return Extra->Label;
		return FText::FromString(Attribute.GetName());
	}

	bool IsPercentStat(const FGameplayAttribute& Attribute)
	{
		const FStatRow* Row = FindStatRow(Attribute);
		const FExtraStat* Extra = Row ? nullptr : FindExtraStat(Attribute);
		return (Row && Row->bPercent) || (Extra && Extra->bPercent);
	}

	/** "공격력 +8", "치명타 확률 +5%". 곱연산은 % 변화로. */
	FText ModifierText(const FknightPartStatModifier& Mod)
	{
		const FExtraStat* Extra = FindExtraStat(Mod.Attribute);
		const bool bCompound = Mod.ModifierOp == EGameplayModOp::MultiplyCompound;
		const bool bMultiply = bCompound || Mod.ModifierOp == EGameplayModOp::MultiplyAdditive;
		// Compound는 배수(1.1), Additive는 변화량(0.1).
		const float Value = bCompound ? Mod.Value - 1.f : Mod.Value;
		const bool bPercent = bMultiply || IsPercentStat(Mod.Attribute);
		FText Number;
		if (!bPercent && Extra && Extra->bDecimal)
		{
			FNumberFormattingOptions Options;
			Options.AlwaysSign = true;
			Options.MinimumFractionalDigits = 0;
			Options.MaximumFractionalDigits = 1;
			Number = FText::AsNumber(Value, &Options);
		}
		else Number = FormatNumber(DisplayNumber(Value, bPercent), bPercent, true);
		return FText::Format(LOCTEXT("ModifierLine", "{0} {1}"), StatLabel(Mod.Attribute), Number);
	}

	FText JoinModifiers(const TArray<FknightPartStatModifier>& Mods)
	{
		TArray<FText> Parts;
		for (const FknightPartStatModifier& Mod : Mods) Parts.Add(ModifierText(Mod));
		return FText::Join(LOCTEXT("ModifierSeparator", "   "), Parts);
	}

	FText PartName(const UknightPartItemDefinition* Part)
	{
		return Part->DisplayName.IsEmpty() ? FText::FromString(Part->GetName()) : Part->DisplayName;
	}

	FText SetName(const UknightEquipmentSetDefinition* Set)
	{
		const FText Name = Set->DisplayName.IsEmpty() ? FText::FromString(Set->SetTag.ToString()) : Set->DisplayName;
		return Name.ToString().EndsWith(TEXT("세트")) ? Name : FText::Format(LOCTEXT("SetNameSuffix", "{0} 세트"), Name);
	}

	/** 설명에 수치가 다 있으면 설명만, 아니면 수치를 덧붙인다. */
	FText TierEffectText(const FknightSetTier& Tier)
	{
		const FText Numbers = JoinModifiers(Tier.StatModifiers);
		if (Tier.Description.IsEmpty()) return Numbers;
		bool bDescribed = true;
		for (const FknightPartStatModifier& Mod : Tier.StatModifiers)
		{
			bDescribed &= Tier.Description.ToString().Contains(FString::FromInt(FMath::Abs(DisplayNumber(Mod.Value, IsPercentStat(Mod.Attribute)))));
		}
		return bDescribed || Numbers.IsEmpty() ? Tier.Description : FText::Format(LOCTEXT("TierWithNumbers", "{0} ({1})"), Tier.Description, Numbers);
	}

	const UknightEquipmentSetDefinition* FindSet(const UknightMaintenanceCatalog* Catalog, const FGameplayTag& Tag)
	{
		if (!Catalog || !Tag.IsValid()) return nullptr;
		for (const UknightEquipmentSetDefinition* Set : Catalog->Sets)
			if (Set && Set->SetTag == Tag) return Set;
		return nullptr;
	}

	FText ActiveSetsText(const UknightMaintenanceCatalog* Catalog, const FknightEquipmentStats& Current)
	{
		if (!Catalog) return FText::GetEmpty();
		TArray<FText> Lines;
		TSet<FGameplayTag> Seen;
		for (const UknightEquipmentSetDefinition* Set : Catalog->Sets)
		{
			if (!Set || !Set->SetTag.IsValid() || Seen.Contains(Set->SetTag)) continue;
			Seen.Add(Set->SetTag);
			const int32 Count = Current.SetCounts.FindRef(Set->SetTag);
			for (const FknightSetTier* Tier : { &Set->TwoPiece, &Set->FourPiece })
			{
				if (Tier->StatModifiers.IsEmpty() || Count < Tier->RequiredCount) continue;
				Lines.Add(FText::Format(LOCTEXT("ActiveSetLine", "{0} {1}세트 · {2}"), SetName(Set), Tier->RequiredCount, TierEffectText(*Tier)));
			}
		}
		if (Lines.IsEmpty()) return LOCTEXT("NoActiveSets", "활성 세트 없음");
		return FText::Join(FText::FromString(TEXT("\n")), Lines);
	}

	FText SetInfoText(const UknightPartItemDefinition* Part, const UknightMaintenanceCatalog* Catalog,
		const FknightEquipmentStats& Current, const FknightEquipmentStats* Predicted)
	{
		if (!Part) return FText::GetEmpty();
		if (!Part->SetTag.IsValid()) return LOCTEXT("NoSet", "세트 없음");
		const UknightEquipmentSetDefinition* Set = FindSet(Catalog, Part->SetTag);
		if (!Set) return LOCTEXT("SetMissing", "세트 정보를 찾을 수 없습니다.");
		const int32 CurrentCount = Current.SetCounts.FindRef(Set->SetTag);
		const int32 PredictedCount = Predicted ? Predicted->SetCounts.FindRef(Set->SetTag) : CurrentCount;
		TArray<FText> Lines;
		Lines.Add(CurrentCount == PredictedCount
			? FText::Format(LOCTEXT("SetHeader", "{0} · 장착 {1}개"), SetName(Set), CurrentCount)
			: FText::Format(LOCTEXT("SetHeaderChange", "{0} · 장착 {1}개 → {2}개"), SetName(Set), CurrentCount, PredictedCount));
		for (const FknightSetTier* Tier : { &Set->TwoPiece, &Set->FourPiece })
		{
			if (Tier->StatModifiers.IsEmpty()) continue;
			const bool bNow = CurrentCount >= Tier->RequiredCount;
			const bool bAfter = PredictedCount >= Tier->RequiredCount;
			const FText State = bNow == bAfter
				? (bNow ? LOCTEXT("TierActive", "활성") : LOCTEXT("TierInactive", "미활성"))
				: (bAfter ? LOCTEXT("TierWillActivate", "장착 시 활성") : LOCTEXT("TierWillDeactivate", "장착 시 해제"));
			Lines.Add(FText::Format(LOCTEXT("SetTierLine", "{0}세트 · {1}  [{2}]"), Tier->RequiredCount, TierEffectText(*Tier), State));
		}
		return FText::Join(FText::FromString(TEXT("\n")), Lines);
	}

	/** SetIsSelected(false)는 토글 버튼만 해제됨 -> ClearSelection 사용. */
	void SetSelected(UCommonButtonBase* Button, bool bSelected)
	{
		if (!Button || Button->GetSelected() == bSelected) return;
		if (bSelected) Button->SetIsSelected(true, false);
		else Button->ClearSelection();
	}

	/** 잠금: 나이트 버튼은 흐림 + 흔들림 + 이유 표시(knightUI). */
	void SetButtonLocked(UCommonButtonBase* Button, bool bLocked, const FText& Reason)
	{
		if (!Button) return;
		if (UFunction* const Function = Button->FindFunction(TEXT("SetLockedWithReason")))
		{
			struct { bool bInLocked; FText InReason; } Params{ bLocked, Reason };
			Button->ProcessEvent(Function, &Params);
		}
		else Button->SetIsLocked(bLocked);
	}

	/** 버튼 글자 변경. SetButtonText가 knightUI에 있어 이름으로 호출. */
	void SetButtonLabel(UCommonButtonBase* Button, const FText& Label)
	{
		UFunction* const Function = Button ? Button->FindFunction(TEXT("SetButtonText")) : nullptr;
		if (!Function) return;
		struct { FText InText; } Params{ Label };
		Button->ProcessEvent(Function, &Params);
	}

	UCommonTextBlock* NewRowText(UWidgetTree* Tree, TSubclassOf<UCommonTextStyle> Style, ETextJustify::Type Justify)
	{
		UCommonTextBlock* const Text = Tree->ConstructWidget<UCommonTextBlock>();
		if (Style) Text->SetStyle(Style);
		Text->SetJustification(Justify);
		return Text;
	}

	void AddFixed(UHorizontalBox* Row, UWidgetTree* Tree, UWidget* Content, float Width)
	{
		USizeBox* const Box = Tree->ConstructWidget<USizeBox>();
		Box->SetWidthOverride(Width);
		Box->AddChild(Content);
		Row->AddChildToHorizontalBox(Box)->SetVerticalAlignment(VAlign_Center);
	}

	TSubclassOf<UCommonTextStyle> BodyStyle()
	{
		return LoadClass<UCommonTextStyle>(nullptr, TEXT("/Game/Knight/UI/Style/TS_Knight_Body.TS_Knight_Body_C"));
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
	// 선택된 탭도 포커스 가능하게 (키보드 이동).
	for (UCommonButtonBase* Button : { Btn_Craft.Get(), Btn_Weapon.Get(), Btn_Armor.Get(), Btn_Head.Get(), Btn_Torso.Get(), Btn_Legs.Get(),
		Btn_Arms.Get(), Btn_SlotWeapon.Get(), Btn_InfoSpec.Get(), Btn_InfoSet.Get(), Btn_InfoMaterial.Get() })
	{
		if (!Button) continue;
		Button->SetIsSelectable(true);
		Button->SetIsInteractableWhenSelected(true);
	}
	if (Btn_Craft) Btn_Craft->OnClicked().AddWeakLambda(this, [this]() { SetCategory(0, true); });
	if (Btn_Weapon) Btn_Weapon->OnClicked().AddWeakLambda(this, [this]() { SetCategory(1, true); });
	if (Btn_Armor) Btn_Armor->OnClicked().AddWeakLambda(this, [this]() { SetCategory(2, true); });
	if (Btn_Head) Btn_Head->OnClicked().AddWeakLambda(this, [this]() { SetSlot(knightGameplayTags::Equipment_Slot_Head, true); });
	if (Btn_Torso) Btn_Torso->OnClicked().AddWeakLambda(this, [this]() { SetSlot(knightGameplayTags::Equipment_Slot_Torso, true); });
	if (Btn_Legs) Btn_Legs->OnClicked().AddWeakLambda(this, [this]() { SetSlot(knightGameplayTags::Equipment_Slot_Legs, true); });
	if (Btn_Arms) Btn_Arms->OnClicked().AddWeakLambda(this, [this]() { SetSlot(knightGameplayTags::Equipment_Slot_Arms, true); });
	if (Btn_SlotWeapon) Btn_SlotWeapon->OnClicked().AddWeakLambda(this, [this]() { SetSlot(knightGameplayTags::Equipment_Slot_Weapon, true); });
	if (Btn_InfoSpec) Btn_InfoSpec->OnClicked().AddWeakLambda(this, [this]() { SetInfoTab(InfoSpec); });
	if (Btn_InfoSet) Btn_InfoSet->OnClicked().AddWeakLambda(this, [this]() { SetInfoTab(InfoSet); });
	if (Btn_InfoMaterial) Btn_InfoMaterial->OnClicked().AddWeakLambda(this, [this]() { SetInfoTab(InfoMaterial); });
	if (Btn_Execute) Btn_Execute->OnClicked().AddWeakLambda(this, [this]() { RequestExecute(); });
	if (Btn_Unequip) Btn_Unequip->OnClicked().AddWeakLambda(this, [this]() { UnequipSelection(); });
	if (Btn_Execute) Btn_Execute->OnLockClicked().AddWeakLambda(this, [this]() { if (Txt_Status) Txt_Status->SetText(ExecuteLockReason); });
	if (Btn_Unequip) Btn_Unequip->OnLockClicked().AddWeakLambda(this, [this]() { if (Txt_Status) Txt_Status->SetText(UnequipLockReason); });
	if (Btn_ConfirmOk) Btn_ConfirmOk->OnClicked().AddWeakLambda(this, [this]() { ShowConfirm(false); ExecuteSelection(); });
	if (Btn_ConfirmCancel) Btn_ConfirmCancel->OnClicked().AddWeakLambda(this, [this]() { ShowConfirm(false); });
	ShowConfirm(false);
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
	ShowConfirm(false);
	InfoTab = InfoSpec;
	if (Txt_Status) Txt_Status->SetText(FText::GetEmpty());
	SetCategory(0, false);
}

void UKnightMaintenanceWidget::Close()
{
	ShowConfirm(false);
	SelectedPart = nullptr;
	SelectedRecipe = nullptr;
}

UWidget* UKnightMaintenanceWidget::GetFocusButton() const { return Btn_Craft; }

void UKnightMaintenanceWidget::SetCategory(int32 NewCategory, bool bFromPlayer)
{
	Category = FMath::Clamp(NewCategory, 0, 2);
	// 무기는 무기 부위만, 방어구는 무기 부위 제외. 가능하면 현재 부위 유지.
	if (Category == 1) Slot = knightGameplayTags::Equipment_Slot_Weapon;
	else if (!Slot.IsValid() || (Category == 2 && Slot == knightGameplayTags::Equipment_Slot_Weapon)) Slot = knightGameplayTags::Equipment_Slot_Head;
	SelectedPart = nullptr;
	SelectedRecipe = nullptr;
	if (Btn_SlotWeapon) Btn_SlotWeapon->SetVisibility(Category == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	for (UCommonButtonBase* Button : { Btn_Head.Get(), Btn_Torso.Get(), Btn_Legs.Get(), Btn_Arms.Get() })
		if (Button) Button->SetVisibility(Category == 1 ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (Btn_InfoMaterial) Btn_InfoMaterial->SetVisibility(Category == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (InfoTab == InfoMaterial && Category != 0) InfoTab = InfoSpec;
	if (Switcher_Info) Switcher_Info->SetActiveWidgetIndex(InfoTab);
	RebuildList();
	if (bFromPlayer) OnSlotFocused.ExecuteIfBound(Slot);
}

void UKnightMaintenanceWidget::SetSlot(FGameplayTag NewSlot, bool bFromPlayer)
{
	if (Category == 1 || (Category == 2 && NewSlot == knightGameplayTags::Equipment_Slot_Weapon)) return;
	Slot = NewSlot;
	SelectedPart = nullptr;
	SelectedRecipe = nullptr;
	RebuildList();
	if (bFromPlayer) OnSlotFocused.ExecuteIfBound(Slot);
}

void UKnightMaintenanceWidget::SetInfoTab(int32 NewTab)
{
	InfoTab = (NewTab == InfoMaterial && Category != 0) ? InfoSpec : FMath::Clamp(NewTab, 0, 2);
	if (Switcher_Info) Switcher_Info->SetActiveWidgetIndex(InfoTab);
	RefreshTabButtons();
}

void UKnightMaintenanceWidget::CycleInfoTab(int32 Direction)
{
	const int32 Count = Category == 0 ? 3 : 2;
	SetInfoTab(((InfoTab + Direction) % Count + Count) % Count);
}

void UKnightMaintenanceWidget::RefreshTabButtons()
{
	const auto Select = [](UCommonButtonBase* Button, bool bSelected) { SetSelected(Button, bSelected); };

	Select(Btn_Craft, Category == 0);
	Select(Btn_Weapon, Category == 1);
	Select(Btn_Armor, Category == 2);
	Select(Btn_Head, Slot == knightGameplayTags::Equipment_Slot_Head);
	Select(Btn_Torso, Slot == knightGameplayTags::Equipment_Slot_Torso);
	Select(Btn_Legs, Slot == knightGameplayTags::Equipment_Slot_Legs);
	Select(Btn_Arms, Slot == knightGameplayTags::Equipment_Slot_Arms);
	Select(Btn_SlotWeapon, Slot == knightGameplayTags::Equipment_Slot_Weapon);
	Select(Btn_InfoSpec, InfoTab == InfoSpec);
	Select(Btn_InfoSet, InfoTab == InfoSet);
	Select(Btn_InfoMaterial, InfoTab == InfoMaterial);
}

void UKnightMaintenanceWidget::RebuildList()
{
	if (!List_Items) return;
	// 줄을 다시 만들면 포커스가 사라지므로 기억했다가 복원.
	const UknightPartItemDefinition* FocusedPart = nullptr;
	for (const UKnightMaintenanceEntryWidget* Entry : Entries)
		if (Entry && (Entry->HasAnyUserFocus() || Entry->HasFocusedDescendants())) FocusedPart = Entry->GetPart();
	List_Items->ClearChildren();
	Entries.Reset();
	const auto* Catalog = UknightEquipmentSettings::LoadCatalog();
	UknightProgressionSubsystem* Progression = GetProgression();
	const knightEquipmentStats::FLoadout EffectiveLoadout = knightEquipmentStats::ReadLoadout(Progression, Catalog);
	const TSubclassOf<UKnightMaintenanceEntryWidget> EntryClass = UKnightMaintenanceEntryWidget::GetSharedWidgetClass();
	if (!EntryClass) { if (Txt_Status) Txt_Status->SetText(LOCTEXT("NoEntryClass", "장비 목록 위젯이 없습니다.")); return; }
	auto AddEntry = [this, EntryClass, Progression, &EffectiveLoadout](const UknightPartItemDefinition* Part, const UknightCraftRecipeDefinition* Recipe)
	{
		if (!Part) return;
		UKnightMaintenanceEntryWidget* Entry = CreateWidget<UKnightMaintenanceEntryWidget>(GetOwningPlayer(), EntryClass);
		if (!Entry) return;
		const int32 Owned = Progression ? Progression->GetItemCount(Part) : 0;
		EKnightMaintenanceEntryState State = EKnightMaintenanceEntryState::Owned;
		if (Recipe) State = Progression && Progression->CanCraftPart(Recipe) ? EKnightMaintenanceEntryState::Craftable : EKnightMaintenanceEntryState::MissingMaterials;
		else if (EffectiveLoadout.FindRef(Part->SlotTag) == Part) State = EKnightMaintenanceEntryState::Equipped;
		Entry->SetEntry(Part, Recipe, State, Owned);
		Entry->OnClicked().AddUObject(this, &ThisClass::HandleEntryClicked, Entry);
		Entry->OnEntryFocused.BindUObject(this, &ThisClass::HandleEntryFocused);
		// 오른쪽 여백: 스크롤바 자리.
		if (UScrollBoxSlot* EntrySlot = Cast<UScrollBoxSlot>(List_Items->AddChild(Entry))) EntrySlot->SetPadding(FMargin(0.f, 0.f, 10.f, 6.f));
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
		// 장착 장비 먼저, 나머지는 보유 순.
		if (const UknightPartItemDefinition* Equipped = EffectiveLoadout.FindRef(Slot))
		{
			Seen.Add(Equipped);
			AddEntry(Equipped, nullptr);
		}
		for (const FknightItemStack& Stack : Owned)
		{
			const UknightPartItemDefinition* Part = Cast<UknightPartItemDefinition>(Stack.Item);
			if (Part && Part->SlotTag == Slot && Stack.Count > 0 && !Seen.Contains(Part))
			{
				Seen.Add(Part);
				AddEntry(Part, nullptr);
			}
		}
	}
	// 다시 만든 뒤에도 선택 유지.
	UKnightMaintenanceEntryWidget* Reselect = nullptr;
	for (UKnightMaintenanceEntryWidget* Entry : Entries)
		if (Entry && SelectedPart && Entry->GetPart() == SelectedPart && Entry->GetRecipe() == SelectedRecipe) Reselect = Entry;
	if (!Reselect)
	{
		SelectedPart = nullptr;
		SelectedRecipe = nullptr;
	}
	for (UKnightMaintenanceEntryWidget* Entry : Entries)
		SetSelected(Entry, Entry == Reselect);
	if (Txt_Status && Entries.IsEmpty()) Txt_Status->SetText(LOCTEXT("EmptyList", "표시할 장비가 없습니다."));
	RefreshTabButtons();
	RefreshDetails();
	if (FocusedPart)
	{
		for (UKnightMaintenanceEntryWidget* Entry : Entries)
			if (Entry && Entry->GetPart() == FocusedPart) { Entry->SetFocus(); break; }
	}
}

void UKnightMaintenanceWidget::SelectEntry(UKnightMaintenanceEntryWidget* Entry)
{
	SelectedPart = Entry ? Entry->GetPart() : nullptr;
	SelectedRecipe = Entry ? Entry->GetRecipe() : nullptr;
	for (UKnightMaintenanceEntryWidget* Candidate : Entries)
		SetSelected(Candidate, Candidate == Entry);
	if (Txt_Status) Txt_Status->SetText(FText::GetEmpty());
	RefreshDetails();
}

void UKnightMaintenanceWidget::HandleEntryClicked(UKnightMaintenanceEntryWidget* Entry)
{
	SelectEntry(Entry);
}

void UKnightMaintenanceWidget::HandleEntryFocused(UKnightMaintenanceEntryWidget* Entry)
{
	if (Entry && (Entry->GetPart() != SelectedPart || Entry->GetRecipe() != SelectedRecipe)) SelectEntry(Entry);
}

bool UKnightMaintenanceWidget::HandleConfirmKey()
{
	if (Panel_Confirm && Panel_Confirm->IsVisible()) return false;
	for (UKnightMaintenanceEntryWidget* Entry : Entries)
	{
		// 포커스는 버튼 내부 위젯(자손)에 있다.
		if (Entry && (Entry->HasAnyUserFocus() || Entry->HasFocusedDescendants()))
		{
			SelectEntry(Entry);
			RequestExecute();
			return true;
		}
	}
	return false;
}

bool UKnightMaintenanceWidget::HandleBack()
{
	if (!Panel_Confirm || !Panel_Confirm->IsVisible()) return false;
	ShowConfirm(false);
	return true;
}

void UKnightMaintenanceWidget::ShowConfirm(bool bShow)
{
	if (!Panel_Confirm) return;
	const bool bWasShown = Panel_Confirm->IsVisible();
	Panel_Confirm->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bShow)
	{
		if (Btn_ConfirmOk) Btn_ConfirmOk->SetFocus();
		return;
	}
	if (!bWasShown) return;
	// 포커스 복원: 요청한 줄, 없으면 실행 버튼.
	for (UKnightMaintenanceEntryWidget* Entry : Entries)
		if (Entry && Entry->GetPart() == SelectedPart && Entry->GetRecipe() == SelectedRecipe) { Entry->SetFocus(); return; }
	if (Btn_Execute && Btn_Execute->IsVisible()) Btn_Execute->SetFocus();
}

void UKnightMaintenanceWidget::RequestExecute()
{
	if (bBusy || !SelectedPart) return;
	if (Category != 0)
	{
		// Enter 실행은 버튼을 거치지 않으므로 잠금 확인.
		if (!ExecuteLockReason.IsEmpty()) { if (Txt_Status) Txt_Status->SetText(ExecuteLockReason); return; }
		ExecuteSelection();
		return;
	}
	UknightProgressionSubsystem* Progression = GetProgression();
	if (!SelectedRecipe || !Progression || !Progression->CanCraftPart(SelectedRecipe))
	{
		if (Txt_Status) Txt_Status->SetText(ExecuteLockReason.IsEmpty() ? LOCTEXT("CannotCraft", "아직 제작할 수 없습니다.") : ExecuteLockReason);
		return;
	}
	// 제작만 확인 (재료 소모). 장착·해제는 바로 실행.
	TArray<FText> Costs;
	for (const FknightItemCost& Cost : SelectedRecipe->Costs)
		if (Cost.Item) Costs.Add(FText::Format(LOCTEXT("ConfirmCost", "{0} {1}개"), Cost.Item->DisplayName, Cost.Count));
	if (Txt_ConfirmTitle) Txt_ConfirmTitle->SetText(FText::Format(LOCTEXT("ConfirmTitle", "{0} 제작"), PartName(SelectedPart)));
	if (Txt_ConfirmBody)
	{
		Txt_ConfirmBody->SetText(Costs.IsEmpty()
			? LOCTEXT("ConfirmBodyFree", "재료 없이 제작합니다.")
			: FText::Format(LOCTEXT("ConfirmBody", "재료를 사용해 제작합니다.\n{0}"), FText::Join(LOCTEXT("CostSeparator", ", "), Costs)));
	}
	ShowConfirm(true);
}

void UKnightMaintenanceWidget::EnsureStatRows()
{
	if (!Stats_Rows || !WidgetTree) return;
	const int32 RowCount = StatRows().Num();
	if (StatCurrentTexts.Num() == RowCount && Stats_Rows->GetChildrenCount() == RowCount) return;
	Stats_Rows->ClearChildren();
	StatLabelTexts.Reset();
	StatCurrentTexts.Reset();
	StatArrowTexts.Reset();
	StatAfterTexts.Reset();
	StatDeltaTexts.Reset();
	const TSubclassOf<UCommonTextStyle> TextStyle = BodyStyle();
	// 이름 | 현재 | → | 변경 후 | 변화량. 숫자 열 고정 폭.
	for (const FStatRow& Stat : StatRows())
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		UCommonTextBlock* Label = NewRowText(WidgetTree, TextStyle, ETextJustify::Left);
		Label->SetText(Stat.Label);
		UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label);
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		UCommonTextBlock* CurrentText = NewRowText(WidgetTree, TextStyle, ETextJustify::Right);
		UCommonTextBlock* ArrowText = NewRowText(WidgetTree, TextStyle, ETextJustify::Center);
		ArrowText->SetText(LOCTEXT("StatArrow", "→"));
		ArrowText->SetColorAndOpacity(FSlateColor(DimColor));
		UCommonTextBlock* AfterText = NewRowText(WidgetTree, TextStyle, ETextJustify::Right);
		UCommonTextBlock* DeltaText = NewRowText(WidgetTree, TextStyle, ETextJustify::Right);
		AddFixed(Row, WidgetTree, CurrentText, 76.f);
		AddFixed(Row, WidgetTree, ArrowText, 32.f);
		AddFixed(Row, WidgetTree, AfterText, 76.f);
		AddFixed(Row, WidgetTree, DeltaText, 68.f);
		Stats_Rows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 3.f, 12.f, 3.f));
		StatLabelTexts.Add(Label);
		StatCurrentTexts.Add(CurrentText);
		StatArrowTexts.Add(ArrowText);
		StatAfterTexts.Add(AfterText);
		StatDeltaTexts.Add(DeltaText);
	}
}

void UKnightMaintenanceWidget::RefreshMaterials(const UknightCraftRecipeDefinition* Recipe, UknightProgressionSubsystem* Progression)
{
	if (!Box_Materials || !WidgetTree) return;
	Box_Materials->ClearChildren();
	const TSubclassOf<UCommonTextStyle> TextStyle = BodyStyle();
	auto AddLine = [this, &TextStyle](const FText& Left, const FText& Right, const FLinearColor& RightColor)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		UCommonTextBlock* Name = NewRowText(WidgetTree, TextStyle, ETextJustify::Left);
		Name->SetText(Left);
		UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(Name);
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UCommonTextBlock* Count = NewRowText(WidgetTree, TextStyle, ETextJustify::Right);
		Count->SetText(Right);
		Row->AddChildToHorizontalBox(Count);
		Box_Materials->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 4.f));
		// 추가 후에 색 지정 (위젯 생성 시 텍스트 스타일이 색을 덮어씀).
		Count->SetColorAndOpacity(FSlateColor(RightColor));
	};
	if (!Recipe)
	{
		AddLine(LOCTEXT("PickRecipe", "제작할 장비를 선택하세요."), FText::GetEmpty(), White);
	}
	else if (Recipe->Costs.IsEmpty())
	{
		AddLine(LOCTEXT("NoMaterials", "필요 재료 없음"), FText::GetEmpty(), White);
	}
	for (const FknightItemCost& Cost : Recipe ? Recipe->Costs : TArray<FknightItemCost>())
	{
		const int32 Have = Progression && Cost.Item ? Progression->GetItemCount(Cost.Item) : 0;
		AddLine(Cost.Item ? Cost.Item->DisplayName : LOCTEXT("MissingItem", "없는 아이템"),
			FText::Format(LOCTEXT("MaterialCount", "{0} / {1}"), Have, Cost.Count), Have >= Cost.Count ? White : Alert);
	}
	if (Txt_StageReq)
	{
		const bool bHasStage = Recipe && !Recipe->RequiredClearedStageId.IsNone();
		Txt_StageReq->SetVisibility(bHasStage ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bHasStage)
		{
			FText StageName = FText::FromName(Recipe->RequiredClearedStageId);
			FKnightStageDefinition Stage;
			const UKnightGameFlowSubsystem* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>() : nullptr;
			if (Flow && Flow->FindStageDefinition(Recipe->RequiredClearedStageId, Stage) && !Stage.DisplayName.IsEmpty()) StageName = Stage.DisplayName;
			const bool bCleared = Progression && Progression->IsStageCleared(Recipe->RequiredClearedStageId);
			Txt_StageReq->SetText(FText::Format(bCleared ? LOCTEXT("StageCleared", "필요 스테이지 · {0} 클리어 (완료)")
				: LOCTEXT("StageNotCleared", "필요 스테이지 · {0} 클리어"), StageName));
			Txt_StageReq->SetColorAndOpacity(FSlateColor(bCleared ? Mist : Alert));
		}
	}
}

void UKnightMaintenanceWidget::RefreshDetails()
{
	UknightProgressionSubsystem* Progression = GetProgression();
	const UknightMaintenanceCatalog* Catalog = UknightEquipmentSettings::LoadCatalog();
	const knightEquipmentStats::FLoadout Loadout = knightEquipmentStats::ReadLoadout(Progression, Catalog);
	const FknightEquipmentStats Current = knightEquipmentStats::Calculate(Loadout, Catalog, Progression);
	const UknightPartItemDefinition* Equipped = Loadout.FindRef(Slot);
	// 선택 없음: 현재 장착 장비 표시.
	const UknightPartItemDefinition* Shown = SelectedPart ? SelectedPart.Get() : Equipped;
	const bool bShownEquipped = Shown && Shown == Equipped;

	FknightEquipmentStats Predicted;
	bool bHasPrediction = false;
	if (SelectedPart && !bShownEquipped)
	{
		auto Changed = Loadout;
		Changed.Add(SelectedPart->SlotTag, SelectedPart);
		Predicted = knightEquipmentStats::Calculate(Changed, Catalog, Progression);
		bHasPrediction = Current.bComplete && Predicted.bComplete;
	}

	if (Txt_PartName) Txt_PartName->SetText(Shown ? PartName(Shown) : LOCTEXT("NothingEquipped", "장착된 장비 없음"));
	if (Txt_PartState)
	{
		FText State;
		FLinearColor Color = Mist;
		if (bShownEquipped) { State = LOCTEXT("StateEquipped", "장착 중"); Color = Ember; }
		else if (Shown && SelectedRecipe)
		{
			const bool bCan = Progression && Progression->CanCraftPart(SelectedRecipe);
			State = bCan ? LOCTEXT("StateCraftable", "제작 가능") : LOCTEXT("StateMissing", "재료 부족");
			Color = bCan ? Frost : DimColor;
		}
		else if (Shown) State = FText::Format(LOCTEXT("StateOwned", "보유 {0}"), Progression ? Progression->GetItemCount(Shown) : 0);
		Txt_PartState->SetText(State);
		Txt_PartState->SetColorAndOpacity(FSlateColor(Color));
		Txt_PartState->SetVisibility(State.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (Txt_PartDesc) Txt_PartDesc->SetText(Shown ? Shown->Description : LOCTEXT("PickPart", "왼쪽 목록에서 장비를 선택하세요."));
	if (Txt_Bonus)
	{
		const FText Bonus = Shown ? JoinModifiers(Shown->StatModifiers) : FText::GetEmpty();
		Txt_Bonus->SetText(Bonus);
		Txt_Bonus->SetVisibility(Bonus.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (Img_Selected)
	{
		UTexture2D* Icon = Shown ? Shown->Icon.LoadSynchronous() : nullptr;
		if (Icon) Img_Selected->SetBrushFromTexture(Icon);
		Img_Selected->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	// 스펙: 현재값, 다른 장비 선택 시 변경 후 값.
	EnsureStatRows();
	const TArray<FStatRow>& Rows = StatRows();
	for (int32 Index = 0; Index < Rows.Num() && Index < StatCurrentTexts.Num(); ++Index)
	{
		const FStatRow& Stat = Rows[Index];
		if (!Current.bComplete)
		{
			StatCurrentTexts[Index]->SetText(FText::GetEmpty());
			StatArrowTexts[Index]->SetVisibility(ESlateVisibility::Hidden);
			StatAfterTexts[Index]->SetText(FText::GetEmpty());
			StatDeltaTexts[Index]->SetText(Index == 0 ? FText::Format(LOCTEXT("StatsUnavailable", "계산 불가: {0}"), FText::FromString(Current.IncompleteReason)) : FText::GetEmpty());
			continue;
		}
		const int32 Now = DisplayNumber(Current.Values.FindRef(Stat.Attribute), Stat.bPercent);
		const int32 After = bHasPrediction ? DisplayNumber(Predicted.Values.FindRef(Stat.Attribute), Stat.bPercent) : Now;
		const int32 Delta = After - Now;
		StatCurrentTexts[Index]->SetText(FormatNumber(Now, Stat.bPercent, false));
		StatLabelTexts[Index]->SetColorAndOpacity(FSlateColor(Delta != 0 ? White : Mist));
		StatArrowTexts[Index]->SetVisibility(Delta != 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		StatAfterTexts[Index]->SetText(Delta != 0 ? FormatNumber(After, Stat.bPercent, false) : FText::GetEmpty());
		StatDeltaTexts[Index]->SetText(Delta != 0 ? FormatNumber(Delta, Stat.bPercent, true) : FText::GetEmpty());
		StatDeltaTexts[Index]->SetColorAndOpacity(FSlateColor(Delta > 0 ? Increase : Alert));
	}

	if (Txt_SetInfo)
	{
		FText Info = SetInfoText(Shown, Catalog, Current, bHasPrediction ? &Predicted : nullptr);
		if (SelectedPart && !bShownEquipped && !bHasPrediction)
			Info = FText::Format(LOCTEXT("SetPredictionFailed", "{0}\n장착 후 수치를 계산할 수 없습니다."), Info);
		Txt_SetInfo->SetText(Info);
	}
	if (Txt_ActiveSets) Txt_ActiveSets->SetText(ActiveSetsText(Catalog, Current));
	RefreshMaterials(Category == 0 ? SelectedRecipe.Get() : nullptr, Progression);

	// 비활성 대신 잠금: 흐리게 보이고, 누르면 이유 표시.
	if (Btn_Execute)
	{
		ExecuteLockReason = FText::GetEmpty();
		if (!SelectedPart) ExecuteLockReason = LOCTEXT("LockPick", "목록에서 장비를 선택하세요.");
		else if (Category == 0 && SelectedRecipe && !SelectedRecipe->RequiredClearedStageId.IsNone()
			&& !(Progression && Progression->IsStageCleared(SelectedRecipe->RequiredClearedStageId)))
			ExecuteLockReason = LOCTEXT("LockStage", "필요 스테이지를 먼저 클리어해야 합니다.");
		else if (Category == 0 && !(SelectedRecipe && Progression && Progression->CanCraftPart(SelectedRecipe)))
			ExecuteLockReason = LOCTEXT("LockMaterials", "재료가 부족합니다.");
		else if (Category != 0 && bShownEquipped) ExecuteLockReason = LOCTEXT("LockEquipped", "이미 장착 중입니다.");
		else if (Category != 0 && !(Progression && Progression->GetItemCount(SelectedPart) > 0)) ExecuteLockReason = LOCTEXT("LockNotOwned", "보유하지 않은 장비입니다.");
		SetButtonLabel(Btn_Execute, Category == 0 ? LOCTEXT("ActionCraft", "제작") : LOCTEXT("ActionEquip", "장착"));
		Btn_Execute->SetVisibility(Category == 0 && !SelectedPart ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		SetButtonLocked(Btn_Execute, bBusy || !ExecuteLockReason.IsEmpty(), ExecuteLockReason);
	}
	if (Btn_Unequip)
	{
		UnequipLockReason = bShownEquipped ? FText::GetEmpty() : LOCTEXT("LockNotEquipped", "장착 중인 장비가 아닙니다.");
		Btn_Unequip->SetVisibility(Category == 0 ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		SetButtonLocked(Btn_Unequip, bBusy || !UnequipLockReason.IsEmpty(), UnequipLockReason);
	}
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
			// 로비에는 폰이 없음. 저장 전 적재 한도 직접 확인.
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
	RebuildList();
	if (Txt_Status)
	{
		Txt_Status->SetText(!bSuccess ? LOCTEXT("ActionFailed", "요청 실패: 재료·소유 상태·적재 한도를 확인하세요.")
			: Category == 0 ? LOCTEXT("Crafted", "제작했습니다.") : LOCTEXT("Equipped", "장착했습니다."));
	}
}

void UKnightMaintenanceWidget::UnequipSelection()
{
	UknightProgressionSubsystem* Progression = GetProgression();
	const knightEquipmentStats::FLoadout Loadout = knightEquipmentStats::ReadLoadout(Progression, UknightEquipmentSettings::LoadCatalog());
	// 선택 장비, 없으면 현재 장착 장비 대상.
	const UknightPartItemDefinition* Target = SelectedPart ? SelectedPart.Get() : Loadout.FindRef(Slot);
	if (bBusy || !Target || Category == 0 || !Progression || Loadout.FindRef(Target->SlotTag) != Target) return;
	bBusy = true;
	if (UknightPartsComponent* LiveParts = GetOwningPlayerPawn() ? GetOwningPlayerPawn()->FindComponentByClass<UknightPartsComponent>() : nullptr)
		LiveParts->UnequipSlot(Target->SlotTag);
	Progression->ClearEquippedSlot(Target->SlotTag);
	bBusy = false;
	RebuildList();
	if (Txt_Status) Txt_Status->SetText(LOCTEXT("Unequipped", "장착을 해제했습니다."));
}

void UKnightMaintenanceWidget::HandleProgressionChanged()
{
	if (bBusy) return;
	RebuildList();
}

#undef LOCTEXT_NAMESPACE
