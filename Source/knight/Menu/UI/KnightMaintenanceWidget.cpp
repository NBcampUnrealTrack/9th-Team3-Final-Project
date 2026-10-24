#include "Menu/UI/KnightMaintenanceWidget.h"

#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "GameFlow/KnightGameFlowSubsystem.h"
#include "GameFlow/KnightStageCatalog.h"
#include "Item/knightEquipmentData.h"
#include "Item/knightItemDefinitions.h"
#include "Item/knightEquipmentStats.h"
#include "Item/knightPartsComponent.h"
#include "knightCharacter.h"
#include "Menu/UI/KnightMaintenanceEntryWidget.h"
#include "Menu/UI/KnightInventoryWidget.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Progression/knightSaveSubsystem.h"
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

	const TArray<FGameplayTag>& MainEquipmentSlotTags()
	{
		static const TArray<FGameplayTag> Tags = { knightGameplayTags::Equipment_Slot_Weapon,
			knightGameplayTags::Equipment_Slot_Head, knightGameplayTags::Equipment_Slot_Torso,
			knightGameplayTags::Equipment_Slot_Arms, knightGameplayTags::Equipment_Slot_Legs };
		return Tags;
	}

	FText MainEquipmentSlotLabel(FGameplayTag Tag)
	{
		if (Tag == knightGameplayTags::Equipment_Slot_Weapon) return LOCTEXT("EquipmentWeapon", "무기");
		if (Tag == knightGameplayTags::Equipment_Slot_Head) return LOCTEXT("EquipmentHead", "머리");
		if (Tag == knightGameplayTags::Equipment_Slot_Torso) return LOCTEXT("EquipmentTorso", "몸");
		if (Tag == knightGameplayTags::Equipment_Slot_Arms) return LOCTEXT("EquipmentArms", "팔");
		if (Tag == knightGameplayTags::Equipment_Slot_Legs) return LOCTEXT("EquipmentLegs", "다리");
		return FText::FromString(Tag.ToString());
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
		for (const FknightActiveSetBonus& Bonus : knightEquipmentStats::GetActiveSetBonuses(Current, Catalog))
		{
			const FknightSetTier& Tier = Bonus.bFourPiece ? Bonus.Set->FourPiece : Bonus.Set->TwoPiece;
			Lines.Add(FText::Format(LOCTEXT("ActiveSetLine", "{0} {1}세트 · {2}"),
				SetName(Bonus.Set), Tier.RequiredCount, TierEffectText(Tier)));
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
			if (Tier->StatModifiers.IsEmpty() && Tier->Description.IsEmpty()) continue;
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

namespace
{
	FText SocketEffectName(EKnightSocketEffect Effect)
	{
		switch (Effect)
		{
		case EKnightSocketEffect::RepairKitCapacity: return LOCTEXT("SocketEffectKit", "수리키트 최대량");
		case EKnightSocketEffect::AttackStaminaCost: return LOCTEXT("SocketEffectAttack", "공격 스태미너 소모량");
		case EKnightSocketEffect::SocketGaugeCharge: return LOCTEXT("SocketEffectCharge", "키트 충전에 필요한 퍼펙트 가드");
		case EKnightSocketEffect::LoadCapacity: return LOCTEXT("SocketEffectLoad", "장비 중량 한계");
		case EKnightSocketEffect::StaminaRecovery: return LOCTEXT("SocketEffectRecovery", "스태미너 회복량");
		case EKnightSocketEffect::FullHealthAttack: return LOCTEXT("SocketEffectFullHealth", "최대 체력일 때 공격력");
		default: return LOCTEXT("SocketEffectUnknown", "효과 확인 불가");
		}
	}

	FText SocketItemEffectText(const UknightSocketItemDefinition* Item)
	{
		if (!Item) return FText::GetEmpty();
		if (Item->Effect == EKnightSocketEffect::SocketGaugeCharge)
			return LOCTEXT("SocketGuardOneLess", "키트 충전에 필요한 퍼펙트 가드 -1회");
		if (!FMath::IsFinite(Item->EffectValue) || Item->EffectValue < 0.f)
			return LOCTEXT("SocketInvalidEffectValue", "효과 수치 확인 불가");
		if (Item->Effect == EKnightSocketEffect::RepairKitCapacity)
			return FText::Format(LOCTEXT("SocketFlatEffect", "{0} +{1}"), SocketEffectName(Item->Effect),
				FText::AsNumber(FMath::RoundToInt(Item->EffectValue)));
		return FText::Format(Item->Effect == EKnightSocketEffect::AttackStaminaCost
			? LOCTEXT("SocketReducedEffect", "{0} -{1}") : LOCTEXT("SocketIncreasedEffect", "{0} +{1}"),
			SocketEffectName(Item->Effect), FText::AsPercent(Item->EffectValue));
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
	if (Btn_Weapon) Btn_Weapon->OnClicked().AddWeakLambda(this, [this]() { SetCategory(2, true); });
	if (Btn_Armor) Btn_Armor->OnClicked().AddWeakLambda(this, [this]() { SetCategory(4, true); });
	// The WBP owns the three existing tab buttons. Their labels and routes now belong to Crafting.
	SetButtonLabel(Btn_Weapon, LOCTEXT("EquipmentTab", "장비"));
	SetButtonLabel(Btn_Armor, LOCTEXT("MaterialsTab", "소재"));
	if (UCanvasPanel* const Root = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr)
	{
		CurrencyText = NewRowText(WidgetTree, BodyStyle(), ETextJustify::Right);
		UCanvasPanelSlot* const CurrencySlot = Root->AddChildToCanvas(CurrencyText);
		CurrencySlot->SetAnchors(FAnchors(1.f, 0.f));
		CurrencySlot->SetAlignment(FVector2D(1.f, 0.f));
		CurrencySlot->SetPosition(FVector2D(-96.f, 64.f));
		CurrencySlot->SetAutoSize(true);
		RefreshCurrency();
		if (UWidget* const Confirm = WidgetTree->FindWidget(TEXT("Panel_Confirm")))
			if (UCanvasPanelSlot* const ConfirmSlot = Cast<UCanvasPanelSlot>(Confirm->Slot)) ConfirmSlot->SetZOrder(100);
	}
	if (Btn_Head) Btn_Head->OnClicked().AddWeakLambda(this, [this]() { SetSlot(knightGameplayTags::Equipment_Slot_Head, true); });
	if (Btn_Torso) Btn_Torso->OnClicked().AddWeakLambda(this, [this]() { SetSlot(knightGameplayTags::Equipment_Slot_Torso, true); });
	if (Btn_Legs) Btn_Legs->OnClicked().AddWeakLambda(this, [this]() { SetSlot(knightGameplayTags::Equipment_Slot_Legs, true); });
	if (Btn_Arms) Btn_Arms->OnClicked().AddWeakLambda(this, [this]() { SetSlot(knightGameplayTags::Equipment_Slot_Arms, true); });
	if (Btn_SlotWeapon) Btn_SlotWeapon->OnClicked().AddWeakLambda(this, [this]() { SetSlot(knightGameplayTags::Equipment_Slot_Weapon, true); });
	if (Btn_InfoSpec) Btn_InfoSpec->OnClicked().AddWeakLambda(this, [this]() { SetInfoTab(InfoSpec); });
	if (Btn_InfoSet) Btn_InfoSet->OnClicked().AddWeakLambda(this, [this]() { SetInfoTab(InfoSet); });
	if (Btn_InfoMaterial) Btn_InfoMaterial->OnClicked().AddWeakLambda(this, [this]() { SetInfoTab(InfoMaterial); });
	// Keep the WBP's existing pages/buttons. Reorder their runtime row and add two
	// CommonUI arrows; the original Q/E hints are still handled by the menu controller.
	if (UHorizontalBox* const InfoRow = WidgetTree ? Cast<UHorizontalBox>(WidgetTree->FindWidget(TEXT("Row_InfoTabs"))) : nullptr)
	{
		const TSubclassOf<UCommonButtonBase> ButtonClass = LoadClass<UCommonButtonBase>(nullptr,
			TEXT("/Game/Knight/UI/Common/WBP_KnightButton.WBP_KnightButton_C"));
		if (ButtonClass && GetOwningPlayer() && Btn_InfoMaterial && Btn_InfoSpec && Btn_InfoSet)
		{
			InfoPreviousButton = CreateWidget<UCommonButtonBase>(GetOwningPlayer(), ButtonClass);
			InfoNextButton = CreateWidget<UCommonButtonBase>(GetOwningPlayer(), ButtonClass);
			if (InfoPreviousButton && InfoNextButton)
			{
				InfoRow->ClearChildren();
				SetButtonLabel(InfoPreviousButton, LOCTEXT("InfoPrevious", "←"));
				SetButtonLabel(InfoNextButton, LOCTEXT("InfoNext", "→"));
				InfoPreviousButton->OnClicked().AddWeakLambda(this, [this]() { CycleInfoTab(-1); });
				InfoNextButton->OnClicked().AddWeakLambda(this, [this]() { CycleInfoTab(1); });
				auto AddArrow = [this, InfoRow](UCommonButtonBase* Button)
				{
					USizeBox* const Frame = WidgetTree->ConstructWidget<USizeBox>();
					Frame->SetWidthOverride(56.f);
					Frame->AddChild(Button);
					InfoRow->AddChildToHorizontalBox(Frame)->SetPadding(FMargin(3.f, 0.f));
				};
				AddArrow(InfoPreviousButton);
				for (UCommonButtonBase* Button : { Btn_InfoMaterial.Get(), Btn_InfoSpec.Get(), Btn_InfoSet.Get() })
				{
					UHorizontalBoxSlot* const ButtonSlot = InfoRow->AddChildToHorizontalBox(Button);
					ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
					ButtonSlot->SetVerticalAlignment(VAlign_Center);
					ButtonSlot->SetPadding(FMargin(3.f, 0.f));
				}
				AddArrow(InfoNextButton);
			}
		}
	}
	// The existing WBP owns the Crafting pages. Materials gets its own page so
	// recipe Materials/Specs controls and their contents are never repurposed.
	if (Switcher_Info && WidgetTree && GetOwningPlayer())
	{
		MaterialSalePage = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Phase6_MaterialSalePage"));
		UVerticalBox* const Body = WidgetTree->ConstructWidget<UVerticalBox>();
		MaterialSalePage->AddChild(Body);
		Switcher_Info->AddChild(MaterialSalePage);
		const TSubclassOf<UCommonTextStyle> TextStyle = BodyStyle();
		MaterialOwnedText = NewRowText(WidgetTree, TextStyle, ETextJustify::Left);
		Body->AddChildToVerticalBox(MaterialOwnedText)->SetPadding(FMargin(0.f, 0.f, 12.f, 10.f));
		MaterialUnitPriceText = NewRowText(WidgetTree, TextStyle, ETextJustify::Left);
		Body->AddChildToVerticalBox(MaterialUnitPriceText)->SetPadding(FMargin(0.f, 0.f, 12.f, 14.f));
		UHorizontalBox* const QuantityRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		Body->AddChildToVerticalBox(QuantityRow)->SetPadding(FMargin(0.f, 0.f, 12.f, 12.f));
		UCommonTextBlock* const QuantityLabel = NewRowText(WidgetTree, TextStyle, ETextJustify::Left);
		QuantityLabel->SetText(LOCTEXT("MaterialSaleQuantity", "판매 수량"));
		UHorizontalBoxSlot* const QuantityLabelSlot = QuantityRow->AddChildToHorizontalBox(QuantityLabel);
		QuantityLabelSlot->SetVerticalAlignment(VAlign_Center);
		QuantityLabelSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		const TSubclassOf<UCommonButtonBase> ButtonClass = LoadClass<UCommonButtonBase>(nullptr,
			TEXT("/Game/Knight/UI/Common/WBP_KnightButton.WBP_KnightButton_C"));
		auto AddQuantityButton = [this, QuantityRow, ButtonClass](const FText& Label, TFunction<void()> Action)
		{
			if (!ButtonClass) return;
			UCommonButtonBase* const Button = CreateWidget<UCommonButtonBase>(GetOwningPlayer(), ButtonClass);
			if (!Button) return;
			SetButtonLabel(Button, Label);
			Button->OnClicked().AddWeakLambda(this, [Action = MoveTemp(Action)]() { Action(); });
			USizeBox* const Frame = WidgetTree->ConstructWidget<USizeBox>();
			Frame->SetWidthOverride(64.f);
			Frame->AddChild(Button);
			QuantityRow->AddChildToHorizontalBox(Frame)->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		};
		AddQuantityButton(LOCTEXT("MaterialSaleMinus", "−"), [this]() { ChangeMaterialSaleQuantity(-1); });
		MaterialQuantityInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Phase6_MaterialQuantityInput"));
		MaterialQuantityInput->SetHintText(LOCTEXT("MaterialQuantityHint", "수량"));
		MaterialQuantityInput->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::HandleMaterialQuantityCommitted);
		AddFixed(QuantityRow, WidgetTree, MaterialQuantityInput, 84.f);
		AddQuantityButton(LOCTEXT("MaterialSalePlus", "+"), [this]() { ChangeMaterialSaleQuantity(1); });
		AddQuantityButton(LOCTEXT("MaterialSaleMax", "최대"), [this]()
		{
			const UknightMaterialItemDefinition* const Material = GetSelectedMaterial();
			SetMaterialSaleQuantity(Material && GetProgression() ? GetProgression()->GetItemCount(Material) : 0);
		});
		MaterialTotalText = NewRowText(WidgetTree, TextStyle, ETextJustify::Left);
		Body->AddChildToVerticalBox(MaterialTotalText)->SetPadding(FMargin(0.f, 0.f, 12.f, 10.f));
		MaterialFeedbackText = NewRowText(WidgetTree, TextStyle, ETextJustify::Left);
		MaterialFeedbackText->SetAutoWrapText(true);
		Body->AddChildToVerticalBox(MaterialFeedbackText)->SetPadding(FMargin(0.f, 0.f, 12.f, 8.f));
		EnhancementDetailPage = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Phase7_EnhancementDetailPage"));
		UpgradePanel = WidgetTree->ConstructWidget<UVerticalBox>();
		EnhancementDetailPage->AddChild(UpgradePanel);
		Switcher_Info->AddChild(EnhancementDetailPage);
		auto AddUpgradeText = [this, TextStyle](TObjectPtr<UCommonTextBlock>& Target)
		{
			Target = NewRowText(WidgetTree, TextStyle, ETextJustify::Left);
			Target->SetAutoWrapText(true);
			UpgradePanel->AddChildToVerticalBox(Target)->SetPadding(FMargin(0.f, 0.f, 12.f, 12.f));
		};
		AddUpgradeText(UpgradeHeading);
		AddUpgradeText(UpgradeAttack);
		AddUpgradeText(UpgradeMoney);
		AddUpgradeText(UpgradeGate);
		AddUpgradeText(UpgradeChance);
		AddUpgradeText(UpgradeResult);
		UpgradeButton = ButtonClass ? CreateWidget<UCommonButtonBase>(GetOwningPlayer(), ButtonClass,
			TEXT("Phase7_UpgradeButton")) : nullptr;
		if (UpgradeButton)
		{
			SetButtonLabel(UpgradeButton, LOCTEXT("UpgradeAction", "강화"));
			UpgradeButton->OnClicked().AddWeakLambda(this, [this]() { RequestWeaponUpgrade(); });
			UpgradePanel->AddChildToVerticalBox(UpgradeButton)->SetPadding(FMargin(0.f, 8.f, 12.f, 0.f));
		}
	}
	if (Btn_Execute) Btn_Execute->OnClicked().AddWeakLambda(this, [this]() { RequestExecute(); });
	if (Btn_Unequip) Btn_Unequip->OnClicked().AddWeakLambda(this, [this]() { UnequipSelection(); });
	if (Btn_Execute) Btn_Execute->OnLockClicked().AddWeakLambda(this, [this]() { if (Txt_Status) Txt_Status->SetText(ExecuteLockReason); });
	if (Btn_Unequip) Btn_Unequip->OnLockClicked().AddWeakLambda(this, [this]() { if (Txt_Status) Txt_Status->SetText(UnequipLockReason); });
	if (UVerticalBox* const ConfirmColumn = WidgetTree ? Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("VerticalBox_Confirm"))) : nullptr)
	{
		UWidget* const Buttons = WidgetTree->FindWidget(TEXT("Row_ConfirmButtons"));
		if (Buttons) ConfirmColumn->RemoveChild(Buttons);
		USizeBox* const PickerFrame = WidgetTree->ConstructWidget<USizeBox>();
		PickerFrame->SetHeightOverride(250.f);
		EquipmentPickerList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("RuntimeEquipmentPickerList"));
		PickerFrame->AddChild(EquipmentPickerList);
		ConfirmColumn->AddChildToVerticalBox(PickerFrame)->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
		PickerFrame->SetVisibility(ESlateVisibility::Collapsed);
		if (Buttons)
		{
			UVerticalBoxSlot* const ButtonsSlot = ConfirmColumn->AddChildToVerticalBox(Buttons);
			ButtonsSlot->SetPadding(FMargin(0.f, 28.f, 0.f, 0.f));
			ButtonsSlot->SetHorizontalAlignment(HAlign_Right);
		}
	}
	if (Btn_ConfirmOk) Btn_ConfirmOk->OnClicked().AddWeakLambda(this, [this]()
	{
		if (bEquipmentPickerOpen) ConfirmEquipmentReplacement();
		else if (bEnhancementPickerOpen) ConfirmEnhancementTarget();
		else if (bMaterialSaleConfirmOpen) ConfirmMaterialSale();
		else { ShowConfirm(false); ExecuteSelection(); }
	});
	if (Btn_ConfirmCancel) Btn_ConfirmCancel->OnClicked().AddWeakLambda(this, [this]()
	{
		if (bEquipmentPickerOpen) CloseEquipmentPicker(true);
		else if (bEnhancementPickerOpen) CloseEnhancementPicker(true);
		else if (bMaterialSaleConfirmOpen) CloseMaterialSaleConfirmation(true);
		else ShowConfirm(false);
	});
	ShowConfirm(false);
	if (UknightProgressionSubsystem* Progression = GetProgression())
		Progression->OnProgressionChanged.AddUniqueDynamic(this, &ThisClass::HandleProgressionChanged);
	if (UGameInstance* const GI = GetGameInstance())
		if (UknightSaveSubsystem* const Saves = GI->GetSubsystem<UknightSaveSubsystem>())
		{
			Saves->OnActiveSlotChanged.AddUniqueDynamic(this, &ThisClass::HandleActiveSlotChanged);
			Saves->OnSaveFinished.AddUniqueDynamic(this, &ThisClass::HandleSaveFinished);
		}
}

void UKnightMaintenanceWidget::NativeDestruct()
{
	if (UknightProgressionSubsystem* Progression = GetProgression())
		Progression->OnProgressionChanged.RemoveDynamic(this, &ThisClass::HandleProgressionChanged);
	if (UGameInstance* const GI = GetGameInstance())
		if (UknightSaveSubsystem* const Saves = GI->GetSubsystem<UknightSaveSubsystem>())
		{
			Saves->OnActiveSlotChanged.RemoveDynamic(this, &ThisClass::HandleActiveSlotChanged);
			Saves->OnSaveFinished.RemoveDynamic(this, &ThisClass::HandleSaveFinished);
		}
	Super::NativeDestruct();
}

UknightProgressionSubsystem* UKnightMaintenanceWidget::GetProgression() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>() : nullptr;
}

void UKnightMaintenanceWidget::Open(EKnightMaintenanceDestination InDestination, bool bStartInMaterials)
{
	SelectedSocketSlot = INDEX_NONE;
	SelectedSocketItem = nullptr;
	SelectedSocketCandidateId = FPrimaryAssetId();
	SocketSlotButtons.Reset();
	SocketCandidateButtons.Reset();
	SocketCandidateIds.Reset();
	bAwaitingSocketSave = false;
	bSocketSaveFailed = false;
	PendingSocketProfileId.Reset();
	CloseEnhancementPicker(false);
	EnhancementTargetId = FPrimaryAssetId();
	UpgradeResultMessage = FText::GetEmpty();
	UpgradeSaveStatusMessage = FText::GetEmpty();
	PendingUpgradeWeaponId = FPrimaryAssetId();
	PendingUpgradeProfileId.Reset();
	bAwaitingUpgradeSave = false;
	bUpgradeSaveFailed = false;
	CloseMaterialSaleConfirmation(false);
	CloseEquipmentPicker(false);
	ShowConfirm(false);
	Destination = InDestination;
	InfoTab = InDestination == EKnightMaintenanceDestination::Crafting ? InfoMaterial : InfoSpec;
	if (Txt_Status) Txt_Status->SetText(FText::GetEmpty());
	const bool bCrafting = Destination == EKnightMaintenanceDestination::Crafting;
	if (UTextBlock* const Heading = WidgetTree ? Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Txt_Header"))) : nullptr)
		Heading->SetText(bCrafting ? LOCTEXT("CraftingDestination", "제작")
			: Destination == EKnightMaintenanceDestination::Enhancement ? LOCTEXT("EnhancementDestination", "강화")
			: LOCTEXT("SocketsDestination", "소켓"));
	for (UCommonButtonBase* const Button : { Btn_Craft.Get(), Btn_Weapon.Get(), Btn_Armor.Get() })
		if (Button) Button->SetVisibility(bCrafting ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (CurrencyText) CurrencyText->SetVisibility(bCrafting ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	RefreshCurrency();
	SetCategory(bCrafting ? (bStartInMaterials ? 4 : 0) : Destination == EKnightMaintenanceDestination::Enhancement ? 1 : 3, false);
}

bool UKnightMaintenanceWidget::IsMaterialQuantityInputFocused() const
{
	return IsMaterialsViewOpen() && MaterialQuantityInput
		&& (MaterialQuantityInput->HasAnyUserFocus() || MaterialQuantityInput->HasFocusedDescendants());
}

void UKnightMaintenanceWidget::Close()
{
	SelectedSocketCandidateId = FPrimaryAssetId();
	SocketSlotButtons.Reset();
	SocketCandidateButtons.Reset();
	SocketCandidateIds.Reset();
	bAwaitingSocketSave = false;
	bSocketSaveFailed = false;
	PendingSocketProfileId.Reset();
	CloseEnhancementPicker(false);
	EnhancementTargetId = FPrimaryAssetId();
	bAwaitingUpgradeSave = false;
	bUpgradeSaveFailed = false;
	UpgradeSaveStatusMessage = FText::GetEmpty();
	PendingUpgradeWeaponId = FPrimaryAssetId();
	PendingUpgradeProfileId.Reset();
	CloseMaterialSaleConfirmation(false);
	CloseEquipmentPicker(false);
	ShowConfirm(false);
	SelectedMaterialId = FPrimaryAssetId();
	MaterialEntries.Reset();
	MaterialSaleQuantity = 0;
	MaterialSaleFeedback = FText::GetEmpty();
	bMaterialSaleSaveFailureShown = false;
	bAwaitingMaterialSaleSave = false;
	SelectedEquipmentSlot = FGameplayTag();
	SelectedPart = nullptr;
	SelectedRecipe = nullptr;
	SelectedSocketItem = nullptr;
	SelectedSocketSlot = INDEX_NONE;
	UpgradeResultMessage = FText::GetEmpty();
}

UWidget* UKnightMaintenanceWidget::GetFocusButton() const
{
	if (Destination == EKnightMaintenanceDestination::Crafting)
		return Category == 4 ? Btn_Armor.Get() : Category == 2 ? Btn_Weapon.Get() : Btn_Craft.Get();
	if (Category == 3) return SocketListButtons.IsEmpty() ? static_cast<UWidget*>(Btn_Execute.Get()) : SocketListButtons[0].Get();
	if (Category == 1) return EnhancementTargetButton.Get();
	return Entries.IsEmpty() ? static_cast<UWidget*>(Btn_Execute.Get()) : Entries[0].Get();
}

void UKnightMaintenanceWidget::SetCategory(int32 NewCategory, bool bFromPlayer)
{
	if ((bEquipmentPickerOpen || bEnhancementPickerOpen || bMaterialSaleConfirmOpen) && bFromPlayer) return;
	if (Destination == EKnightMaintenanceDestination::Crafting && NewCategory != 0 && NewCategory != 2 && NewCategory != 4) return;
	if (Destination == EKnightMaintenanceDestination::Enhancement && NewCategory != 1) return;
	if (Destination == EKnightMaintenanceDestination::Sockets && NewCategory != 3) return;
	CloseMaterialSaleConfirmation(false);
	CloseEnhancementPicker(false);
	CloseEquipmentPicker(false);
	if (NewCategory != 4)
	{
		SelectedMaterialId = FPrimaryAssetId();
		MaterialEntries.Reset();
		MaterialSaleQuantity = 0;
		MaterialSaleFeedback = FText::GetEmpty();
	}
	if (NewCategory != 1) EnhancementTargetId = FPrimaryAssetId();
	Category = NewCategory;
	SelectedEquipmentSlot = FGameplayTag();
	EquipmentSlotButtons.Reset();
	// Enhancement is weapon-only. Equipment includes all five slots.
	if (Category == 1) Slot = knightGameplayTags::Equipment_Slot_Weapon;
	else if (!Slot.IsValid()) Slot = knightGameplayTags::Equipment_Slot_Head;
	SelectedPart = nullptr;
	SelectedRecipe = nullptr;
	if (Txt_Status) Txt_Status->SetText(FText::GetEmpty());
	if (Category != 3)
	{
		SelectedSocketItem = nullptr;
		SelectedSocketCandidateId = FPrimaryAssetId();
		SelectedSocketSlot = INDEX_NONE;
		SocketSlotButtons.Reset();
		SocketCandidateButtons.Reset();
		SocketCandidateIds.Reset();
	}
	UpgradeResultMessage = FText::GetEmpty();
	if (UWidget* const Slots = WidgetTree ? WidgetTree->FindWidget(TEXT("Row_Slots")) : nullptr)
		Slots->SetVisibility(Category == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (Btn_SlotWeapon) Btn_SlotWeapon->SetVisibility(Category == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	for (UCommonButtonBase* Button : { Btn_Head.Get(), Btn_Torso.Get(), Btn_Legs.Get(), Btn_Arms.Get() })
		if (Button) Button->SetVisibility(Category == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (UWidget* const Tabs = WidgetTree ? WidgetTree->FindWidget(TEXT("Row_InfoTabs")) : nullptr)
		Tabs->SetVisibility(Category == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (Switcher_Info) Switcher_Info->SetVisibility(ESlateVisibility::Visible);
	if (Btn_InfoSet) Btn_InfoSet->SetVisibility(Category == 0 || Category == 3 ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (Btn_InfoSpec) Btn_InfoSpec->SetVisibility(Category == 3 ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (Btn_InfoMaterial) Btn_InfoMaterial->SetVisibility(Category == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (InfoPreviousButton) InfoPreviousButton->SetVisibility(Category == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (InfoNextButton) InfoNextButton->SetVisibility(Category == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	InfoTab = Category == 0 ? InfoMaterial : InfoSpec;
	if (Switcher_Info)
	{
		if (Category == 4 && MaterialSalePage) Switcher_Info->SetActiveWidget(MaterialSalePage);
		else if (Category == 1 && EnhancementDetailPage) Switcher_Info->SetActiveWidget(EnhancementDetailPage);
		else Switcher_Info->SetActiveWidgetIndex(InfoTab);
	}
	RebuildList(false);
	if (bFromPlayer && Category != 3) OnSlotFocused.ExecuteIfBound(Category == 4 || Category == 2 ? FGameplayTag() : Slot);
}

void UKnightMaintenanceWidget::SetSlot(FGameplayTag NewSlot, bool bFromPlayer)
{
	if (Category != 0) return;
	Slot = NewSlot;
	SelectedPart = nullptr;
	SelectedRecipe = nullptr;
	if (Category == 0) SetInfoTab(InfoMaterial);
	if (Txt_Status) Txt_Status->SetText(FText::GetEmpty());
	UpgradeResultMessage = FText::GetEmpty();
	RebuildList(false);
	if (bFromPlayer) OnSlotFocused.ExecuteIfBound(Slot);
}

void UKnightMaintenanceWidget::SetInfoTab(int32 NewTab)
{
	if (Category == 1) return; // Enhancement owns its separate quote page.
	InfoTab = Category == 0 ? (NewTab == InfoSpec ? InfoSpec : InfoMaterial)
		: Category == 1 && NewTab == InfoSet ? InfoSet : InfoSpec;
	if (Switcher_Info) Switcher_Info->SetActiveWidgetIndex(InfoTab);
	RefreshTabButtons();
}

void UKnightMaintenanceWidget::CycleInfoTab(int32 Direction)
{
	if (Direction == 0 || Category != 0 || bEquipmentPickerOpen || (Panel_Confirm && Panel_Confirm->IsVisible())) return;
	SetInfoTab(InfoTab == InfoMaterial ? InfoSpec : InfoMaterial);
}

void UKnightMaintenanceWidget::RefreshCurrency()
{
	if (!CurrencyText) return;
	const UknightSaveSubsystem* const Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	const UknightProgressionSubsystem* const Progression = GetProgression();
	CurrencyText->SetText(Saves && Saves->HasSelectedSlot() && Progression
		? FText::Format(LOCTEXT("CraftingMoney", "보유 돈  {0}"), FText::AsNumber(Progression->GetMoney()))
		: LOCTEXT("CraftingMoneyNoProfile", "보유 돈  —"));
}

void UKnightMaintenanceWidget::RefreshMaterialContainer()
{
	if (!List_Items || !WidgetTree) return;
	List_Items->ClearChildren();
	Entries.Reset();
	SocketListButtons.Reset();
	MaterialEntries.Reset();
	SelectedPart = nullptr;
	SelectedRecipe = nullptr;
	const UknightSaveSubsystem* const Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	const bool bHasProfile = Saves && Saves->HasSelectedSlot();
	TArray<FknightItemStack> Materials;
	const bool bLoaded = GetProgression() && GetProgression()->GetOwnedMaterials(Materials);
	bool bSelectionStillOwned = false;
	for (const FknightItemStack& Stack : Materials)
	{
		const UknightMaterialItemDefinition* const Material = Cast<UknightMaterialItemDefinition>(Stack.Item);
		if (!Material || Stack.Count <= 0) continue;
		const FPrimaryAssetId ItemId = Material->GetPrimaryAssetId();
		UKnightInventoryEntryWidget* const Row = CreateWidget<UKnightInventoryEntryWidget>(
			GetOwningPlayer(), UKnightInventoryEntryWidget::StaticClass());
		if (!Row) continue;
		const FLinearColor NameColor = Material->bHardModeExclusive ? FLinearColor(FColor(225, 93, 93))
			: Material->Rarity > 1 ? FLinearColor(FColor(97, 166, 239)) : White;
		Row->Configure(Material, Stack.Count, false, NameColor);
		Row->OnSelected.AddUObject(this, &ThisClass::SelectMaterial);
		Row->SetSelected(ItemId == SelectedMaterialId);
		if (UScrollBoxSlot* const SlotRow = Cast<UScrollBoxSlot>(List_Items->AddChild(Row)))
			SlotRow->SetPadding(FMargin(0.f, 0.f, 10.f, 6.f));
		MaterialEntries.Add(Row);
		if (ItemId == SelectedMaterialId) bSelectionStillOwned = true;
	}
	if (MaterialEntries.IsEmpty())
	{
		UCommonTextBlock* const Empty = NewRowText(WidgetTree, BodyStyle(), ETextJustify::Left);
		Empty->SetText(!bHasProfile ? LOCTEXT("MaterialsNoProfile", "저장 슬롯을 선택해 주세요.")
			: bLoaded ? LOCTEXT("NoOwnedMaterials", "보유 소재가 없습니다.")
			: LOCTEXT("MaterialsUnavailable", "소재 정의를 모두 확인할 수 없습니다."));
		List_Items->AddChild(Empty);
	}
	if (!bSelectionStillOwned)
	{
		SelectedMaterialId = FPrimaryAssetId();
		MaterialSaleQuantity = 0;
	}
	else SetMaterialSaleQuantity(MaterialSaleQuantity);
	if (Txt_Status) Txt_Status->SetText(bHasProfile && !bLoaded
		? LOCTEXT("MaterialQueryIncomplete", "일부 보유 아이템 정의를 확인할 수 없습니다. 미확인 기록은 유지됩니다.") : FText::GetEmpty());
	RefreshTabButtons();
	RefreshMaterialDetails();
}

const UknightMaterialItemDefinition* UKnightMaintenanceWidget::GetSelectedMaterial() const
{
	for (const UKnightInventoryEntryWidget* const Row : MaterialEntries)
		if (Row && Row->GetItem() && Row->GetItem()->GetPrimaryAssetId() == SelectedMaterialId)
			return Cast<UknightMaterialItemDefinition>(Row->GetItem());
	return nullptr;
}

void UKnightMaintenanceWidget::SelectMaterial(const UknightItemDefinition* Item)
{
	if (Category != 4 || bMaterialSaleConfirmOpen || bBusy) return;
	const UknightMaterialItemDefinition* const Material = Cast<UknightMaterialItemDefinition>(Item);
	const FPrimaryAssetId NewId = Material ? Material->GetPrimaryAssetId() : FPrimaryAssetId();
	if (NewId != SelectedMaterialId)
	{
		SelectedMaterialId = NewId;
		MaterialSaleQuantity = NewId.IsValid() ? 1 : 0;
		MaterialSaleFeedback = FText::GetEmpty();
	}
	for (UKnightInventoryEntryWidget* const Row : MaterialEntries)
		if (Row && Row->GetItem()) Row->SetSelected(Row->GetItem()->GetPrimaryAssetId() == SelectedMaterialId);
	RefreshMaterialDetails();
}

void UKnightMaintenanceWidget::SetMaterialSaleQuantity(int32 Quantity)
{
	const UknightMaterialItemDefinition* const Material = GetSelectedMaterial();
	const int32 Owned = Material && GetProgression() ? GetProgression()->GetItemCount(Material) : 0;
	MaterialSaleQuantity = Owned > 0 ? FMath::Clamp(Quantity, 1, Owned) : 0;
	if (MaterialQuantityInput) MaterialQuantityInput->SetText(MaterialSaleQuantity > 0
		? FText::AsNumber(MaterialSaleQuantity) : FText::GetEmpty());
	RefreshMaterialDetails();
}

void UKnightMaintenanceWidget::ChangeMaterialSaleQuantity(int32 Delta)
{
	if (Category == 4 && !bMaterialSaleConfirmOpen && !bBusy)
		SetMaterialSaleQuantity(MaterialSaleQuantity + Delta);
}

void UKnightMaintenanceWidget::HandleMaterialQuantityCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (Category != 4 || bMaterialSaleConfirmOpen || bBusy) return;
	const UknightMaterialItemDefinition* const Material = GetSelectedMaterial();
	const int32 Owned = Material && GetProgression() ? GetProgression()->GetItemCount(Material) : 0;
	int32 Parsed = 0;
	if (!LexTryParseString(Parsed, *Text.ToString().TrimStartAndEnd()) || Parsed < 1 || Parsed > Owned)
	{
		MaterialSaleFeedback = LOCTEXT("MaterialInvalidQuantity", "보유 수량 안에서 판매 수량을 입력하세요.");
		SetMaterialSaleQuantity(MaterialSaleQuantity);
		return;
	}
	MaterialSaleFeedback = FText::GetEmpty();
	SetMaterialSaleQuantity(Parsed);
}

void UKnightMaintenanceWidget::RefreshMaterialDetails()
{
	if (Category != 4) return;
	const UknightMaterialItemDefinition* const Material = GetSelectedMaterial();
	UknightProgressionSubsystem* const Progression = GetProgression();
	const int32 Owned = Material && Progression ? Progression->GetItemCount(Material) : 0;
	if (Material && Owned > 0 && (MaterialSaleQuantity < 1 || MaterialSaleQuantity > Owned))
		MaterialSaleQuantity = FMath::Clamp(MaterialSaleQuantity, 1, Owned);
	if (MaterialQuantityInput)
	{
		MaterialQuantityInput->SetIsEnabled(Material && Owned > 0);
		MaterialQuantityInput->SetText(Material && Owned > 0 ? FText::AsNumber(MaterialSaleQuantity) : FText::GetEmpty());
	}
	if (Txt_PartName) Txt_PartName->SetText(!Material ? LOCTEXT("MaterialSelectPrompt", "소재를 선택하세요.")
		: Material->DisplayName.IsEmpty() ? LOCTEXT("MaterialNameMissing", "이름 미설정") : Material->DisplayName);
	if (Txt_PartState)
	{
		Txt_PartState->SetText(Material ? FText::Format(LOCTEXT("MaterialOwned", "보유 {0}개 · 희귀도 {1}"), Owned, Material->Rarity)
			: FText::GetEmpty());
		Txt_PartState->SetVisibility(Material ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Txt_PartDesc)
	{
		Txt_PartDesc->SetText(Material ? (Material->Description.IsEmpty() ? LOCTEXT("MaterialDescriptionMissing", "설명 미설정")
			: Material->Description) : FText::GetEmpty());
		Txt_PartDesc->SetVisibility(Material ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Img_Selected)
	{
		UTexture2D* const Icon = Material ? Material->Icon.LoadSynchronous() : nullptr;
		if (Icon) Img_Selected->SetBrushFromTexture(Icon);
		Img_Selected->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Txt_Bonus) Txt_Bonus->SetVisibility(ESlateVisibility::Collapsed);
	if (Btn_Unequip) Btn_Unequip->SetVisibility(ESlateVisibility::Collapsed);
	if (MaterialOwnedText) MaterialOwnedText->SetText(Material
		? FText::Format(LOCTEXT("MaterialSaleSelected", "선택 소재 · 보유 {0}개"), Owned)
		: LOCTEXT("MaterialSaleChoose", "왼쪽 목록에서 소재를 선택하세요."));
	if (MaterialUnitPriceText) MaterialUnitPriceText->SetText(Material && Material->bSellable && Material->SellPrice > 0
		? FText::Format(LOCTEXT("MaterialSaleUnitPrice", "개당 판매 가격: {0}"), FText::AsNumber(Material->SellPrice))
		: Material ? LOCTEXT("MaterialNotSellable", "판매 불가") : FText::GetEmpty());
	FKnightMaterialSaleReceipt Quote;
	const bool bCanSell = !bBusy && !bMaterialSaleConfirmOpen && Material && Progression
		&& Progression->GetMaterialSaleQuote(SelectedMaterialId, MaterialSaleQuantity, Quote);
	if (MaterialTotalText) MaterialTotalText->SetText(bCanSell
		? FText::Format(LOCTEXT("MaterialSaleTotal", "총 판매 금액: {0}"), FText::AsNumber(Quote.TotalMoney))
		: Material ? LOCTEXT("MaterialSaleTotalUnknown", "총 판매 금액: —") : FText::GetEmpty());
	if (MaterialFeedbackText) MaterialFeedbackText->SetText(MaterialSaleFeedback);
	if (Btn_Execute)
	{
		SetButtonLabel(Btn_Execute, LOCTEXT("MaterialSaleAction", "판매"));
		Btn_Execute->SetVisibility(ESlateVisibility::Visible);
		ExecuteLockReason = !Material ? LOCTEXT("MaterialPickFirst", "판매할 소재를 선택하세요.")
			: !Material->bSellable || Material->SellPrice <= 0 ? LOCTEXT("MaterialCannotSell", "판매 불가")
			: !bCanSell ? LOCTEXT("MaterialSaleBlocked", "수량 또는 저장 상태를 확인하세요.") : FText::GetEmpty();
		SetButtonLocked(Btn_Execute, !ExecuteLockReason.IsEmpty(), ExecuteLockReason);
	}
}

void UKnightMaintenanceWidget::OpenMaterialSaleConfirmation()
{
	if (Category != 4 || bBusy || bMaterialSaleConfirmOpen || !Panel_Confirm) return;
	const UknightMaterialItemDefinition* const Material = GetSelectedMaterial();
	const UknightSaveSubsystem* const Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	int32 Requested = 0;
	const FString Raw = MaterialQuantityInput ? MaterialQuantityInput->GetText().ToString().TrimStartAndEnd() : FString();
	const int32 Owned = Material && GetProgression() ? GetProgression()->GetItemCount(Material) : 0;
	if (!Material || !Saves || !Saves->CanStartAtomicTransaction()
		|| !LexTryParseString(Requested, *Raw) || Requested < 1 || Requested > Owned)
	{
		MaterialSaleFeedback = LOCTEXT("MaterialSaleInvalidRequest", "소재 보유량, 입력 수량 또는 저장 상태를 확인하세요.");
		RefreshMaterialDetails();
		return;
	}
	FKnightMaterialSaleReceipt Quote;
	if (!GetProgression()->GetMaterialSaleQuote(SelectedMaterialId, Requested, Quote))
	{
		MaterialSaleFeedback = LOCTEXT("MaterialSaleQuoteRejected", "판매 불가: 소재 정의와 수량을 확인하세요.");
		RefreshMaterialDetails();
		return;
	}
	MaterialSaleQuantity = Requested;
	PendingMaterialId = SelectedMaterialId;
	PendingMaterialQuantity = Requested;
	PendingMaterialProfileId = Saves->GetActiveSlotId();
	bMaterialSaleConfirmOpen = true;
	if (Txt_ConfirmTitle) Txt_ConfirmTitle->SetText(LOCTEXT("MaterialSaleConfirmTitle", "소재 판매"));
	if (Txt_ConfirmBody) Txt_ConfirmBody->SetText(FText::Format(
		LOCTEXT("MaterialSaleConfirmBody", "{0} {1}개를 {2}에 판매하시겠습니까?"),
		Material->DisplayName.IsEmpty() ? LOCTEXT("MaterialNameMissingConfirm", "이름 미설정") : Material->DisplayName,
		Requested, FText::AsNumber(Quote.TotalMoney)));
	SetButtonLabel(Btn_ConfirmOk, LOCTEXT("MaterialSaleConfirmButton", "판매"));
	ShowConfirm(true);
}

void UKnightMaintenanceWidget::RestoreMaterialFocus()
{
	if (!GetOwningPlayer()) return;
	for (UKnightInventoryEntryWidget* const Row : MaterialEntries)
		if (Row && Row->GetItem() && Row->GetItem()->GetPrimaryAssetId() == SelectedMaterialId && Row->GetButton())
		{
			Row->GetButton()->SetUserFocus(GetOwningPlayer());
			return;
		}
	if (Btn_Armor) Btn_Armor->SetUserFocus(GetOwningPlayer());
}

void UKnightMaintenanceWidget::CloseMaterialSaleConfirmation(bool bRestoreFocus)
{
	if (!bMaterialSaleConfirmOpen) return;
	bMaterialSaleConfirmOpen = false;
	PendingMaterialId = FPrimaryAssetId();
	PendingMaterialQuantity = 0;
	PendingMaterialProfileId.Reset();
	ShowConfirm(false);
	SetButtonLabel(Btn_ConfirmOk, LOCTEXT("ConfirmCraftLabel", "제작"));
	if (bRestoreFocus) RestoreMaterialFocus();
	RefreshMaterialDetails();
}

void UKnightMaintenanceWidget::ConfirmMaterialSale()
{
	if (!bMaterialSaleConfirmOpen || bBusy || Category != 4) return;
	UknightProgressionSubsystem* const Progression = GetProgression();
	const UknightSaveSubsystem* const Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	const UknightMaterialItemDefinition* const Material = GetSelectedMaterial();
	FKnightMaterialSaleReceipt Quote;
	if (!Progression || !Saves || !Saves->CanStartAtomicTransaction()
		|| Saves->GetActiveSlotId() != PendingMaterialProfileId
		|| !Material || SelectedMaterialId != PendingMaterialId
		|| PendingMaterialQuantity < 1 || Progression->GetItemCount(Material) < PendingMaterialQuantity
		|| !Progression->GetMaterialSaleQuote(PendingMaterialId, PendingMaterialQuantity, Quote))
	{
		CloseMaterialSaleConfirmation(true);
		MaterialSaleFeedback = LOCTEXT("MaterialSaleStale", "보유량이나 저장 슬롯이 바뀌었습니다. 다시 확인하세요.");
		RefreshMaterialContainer();
		return;
	}
	bBusy = true; // CommitChange broadcasts synchronously; rebuild exactly once after SellMaterial returns.
	FKnightMaterialSaleReceipt Receipt;
	const bool bSold = Progression->SellMaterial(PendingMaterialId, PendingMaterialQuantity, Receipt);
	bBusy = false;
	CloseMaterialSaleConfirmation(false);
	if (bSold)
	{
		bAwaitingMaterialSaleSave = true;
		bMaterialSaleSaveFailureShown = false;
		MaterialSaleFeedback = FText::Format(LOCTEXT("MaterialSaleSuccess", "{0}개 판매 완료 · +{1}"),
			Receipt.Count, FText::AsNumber(Receipt.TotalMoney));
	}
	else MaterialSaleFeedback = LOCTEXT("MaterialSaleRejected", "판매하지 못했습니다. 소재·수량·저장 상태를 확인하세요.");
	RefreshCurrency();
	RefreshMaterialContainer();
	RestoreMaterialFocus();
}

void UKnightMaintenanceWidget::RefreshTabButtons()
{
	const auto Select = [](UCommonButtonBase* Button, bool bSelected) { SetSelected(Button, bSelected); };

	Select(Btn_Craft, Category == 0);
	Select(Btn_Weapon, Category == 2);
	Select(Btn_Armor, Category == 4);
	Select(Btn_Head, Category == 0 && Slot == knightGameplayTags::Equipment_Slot_Head);
	Select(Btn_Torso, Category == 0 && Slot == knightGameplayTags::Equipment_Slot_Torso);
	Select(Btn_Legs, Category == 0 && Slot == knightGameplayTags::Equipment_Slot_Legs);
	Select(Btn_Arms, Category == 0 && Slot == knightGameplayTags::Equipment_Slot_Arms);
	Select(Btn_SlotWeapon, Category == 0 && Slot == knightGameplayTags::Equipment_Slot_Weapon);
	Select(Btn_InfoSpec, InfoTab == InfoSpec);
	Select(Btn_InfoSet, InfoTab == InfoSet);
	Select(Btn_InfoMaterial, InfoTab == InfoMaterial);
}

void UKnightMaintenanceWidget::RebuildList(bool bRestoreFocus)
{
	if (!List_Items) return;
	if (Category == 2) { RebuildEquipmentSlots(bRestoreFocus); return; }
	if (Category == 1)
	{
		const bool bTargetFocused = bRestoreFocus && EnhancementTargetButton
			&& (EnhancementTargetButton->HasAnyUserFocus() || EnhancementTargetButton->HasFocusedDescendants());
		RebuildEnhancementTarget();
		if (bTargetFocused && EnhancementTargetButton) EnhancementTargetButton->SetFocus();
		return;
	}
	if (Category == 3) { RebuildSocketList(); return; }
	if (Category == 4) { RefreshMaterialContainer(); return; }
	// 줄을 다시 만들면 포커스가 사라지므로 기억했다가 복원.
	const UknightPartItemDefinition* FocusedPart = nullptr;
	if (bRestoreFocus)
		for (const UKnightMaintenanceEntryWidget* Entry : Entries)
			if (Entry && (Entry->HasAnyUserFocus() || Entry->HasFocusedDescendants())) FocusedPart = Entry->GetPart();
	List_Items->ClearChildren();
	Entries.Reset();
	SocketListButtons.Reset();
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
		if (Recipe) State = Owned > 0 ? EKnightMaintenanceEntryState::Owned
			: Progression && Progression->CanCraftPart(Recipe) ? EKnightMaintenanceEntryState::Craftable : EKnightMaintenanceEntryState::MissingMaterials;
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

const UknightWeaponItemDefinition* UKnightMaintenanceWidget::GetEnhancementTarget(bool& bComplete) const
{
	bComplete = false;
	if (!GetProgression()) return nullptr;
	TArray<FknightItemStack> Owned;
	bComplete = GetProgression()->GetOwnedWeapons(Owned);
	for (const FknightItemStack& Stack : Owned)
		if (Stack.Count > 0 && Stack.Item && Stack.Item->GetPrimaryAssetId() == EnhancementTargetId)
			return Cast<UknightWeaponItemDefinition>(Stack.Item);
	return nullptr;
}

void UKnightMaintenanceWidget::RebuildEnhancementTarget()
{
	if (!List_Items || !WidgetTree) return;
	List_Items->ClearChildren();
	Entries.Reset();
	EquipmentSlotButtons.Reset();
	EnhancementTargetButton = nullptr;
	bool bComplete = false;
	const UknightWeaponItemDefinition* const Weapon = GetEnhancementTarget(bComplete);
	if (bComplete && EnhancementTargetId.IsValid() && !Weapon)
	{
		EnhancementTargetId = FPrimaryAssetId();
		UpgradeResultMessage = FText::GetEmpty();
	}
	const TSubclassOf<UCommonButtonBase> ButtonClass = LoadClass<UCommonButtonBase>(nullptr,
		TEXT("/Game/Knight/UI/Common/WBP_KnightButton.WBP_KnightButton_C"));
	if (ButtonClass && GetOwningPlayer())
	{
		EnhancementTargetButton = CreateWidget<UCommonButtonBase>(GetOwningPlayer(), ButtonClass,
			TEXT("Phase7_EnhancementTarget"));
		if (EnhancementTargetButton)
		{
			SetButtonLabel(EnhancementTargetButton, Weapon
				? FText::Format(LOCTEXT("EnhancementTargetFilled", "강화 대상 · {0}"), PartName(Weapon))
				: EnhancementTargetId.IsValid() ? LOCTEXT("EnhancementTargetUnresolved", "강화 대상 확인 불가")
				: LOCTEXT("EnhancementTargetEmpty", "강화 대상 선택 · 빈 슬롯"));
			EnhancementTargetButton->SetIsSelectable(true);
			EnhancementTargetButton->SetIsInteractableWhenSelected(true);
			EnhancementTargetButton->OnClicked().AddWeakLambda(this, [this]() { OpenEnhancementPicker(); });
			if (UScrollBoxSlot* const Row = Cast<UScrollBoxSlot>(List_Items->AddChild(EnhancementTargetButton)))
				Row->SetPadding(FMargin(18.f, 48.f, 18.f, 8.f));
		}
	}
	TArray<FknightItemStack> Owned;
	if (UknightProgressionSubsystem* const Progression = GetProgression()) Progression->GetOwnedWeapons(Owned);
	if (Txt_Status) Txt_Status->SetText(!bComplete
		? LOCTEXT("EnhancementOwnedIncomplete", "보유 무기 정의를 모두 확인할 수 없습니다. 저장 기록은 유지됩니다.")
		: Owned.IsEmpty() ? LOCTEXT("EnhancementNoWeapons", "보유 무기가 없습니다.") : FText::GetEmpty());
	RefreshEnhancementDetails();
	if (bEnhancementPickerOpen) RebuildEnhancementPicker();
}

void UKnightMaintenanceWidget::OpenEnhancementPicker()
{
	if (Category != 1 || bEnhancementPickerOpen || bBusy || !Panel_Confirm || !EquipmentPickerList) return;
	const UknightSaveSubsystem* const Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	if (!Saves || !Saves->HasSelectedSlot()) return;
	bEnhancementPickerOpen = true;
	EnhancementCandidateId = FPrimaryAssetId();
	EnhancementPickerProfileId = Saves->GetActiveSlotId();
	if (UWidget* const Frame = EquipmentPickerList->GetParent()) Frame->SetVisibility(ESlateVisibility::Visible);
	if (Txt_ConfirmTitle) Txt_ConfirmTitle->SetText(LOCTEXT("EnhancementPickerTitle", "강화 대상 무기"));
	SetButtonLabel(Btn_ConfirmOk, LOCTEXT("EnhancementPickerChoose", "대상 선택"));
	RebuildEnhancementPicker();
	ShowConfirm(true);
	if (!EnhancementPickerButtons.IsEmpty()) EnhancementPickerButtons[0]->SetFocus();
	else if (Btn_ConfirmCancel) Btn_ConfirmCancel->SetFocus();
}

void UKnightMaintenanceWidget::RebuildEnhancementPicker()
{
	if (!bEnhancementPickerOpen || !EquipmentPickerList || !WidgetTree) return;
	EquipmentPickerList->ClearChildren();
	EnhancementPickerButtons.Reset();
	EnhancementPickerIds.Reset();
	TArray<FknightItemStack> Owned;
	const bool bComplete = GetProgression() && GetProgression()->GetOwnedWeapons(Owned);
	if (EnhancementCandidateId.IsValid() && !Owned.ContainsByPredicate([this](const FknightItemStack& Stack)
	{
		return Stack.Count > 0 && Stack.Item && Stack.Item->GetPrimaryAssetId() == EnhancementCandidateId;
	})) EnhancementCandidateId = FPrimaryAssetId();
	const TSubclassOf<UCommonButtonBase> ButtonClass = LoadClass<UCommonButtonBase>(nullptr,
		TEXT("/Game/Knight/UI/Common/WBP_KnightButton.WBP_KnightButton_C"));
	if (ButtonClass && GetOwningPlayer())
		for (const FknightItemStack& Stack : Owned)
		{
			const UknightWeaponItemDefinition* const Weapon = Cast<UknightWeaponItemDefinition>(Stack.Item);
			if (!Weapon || Stack.Count <= 0) continue;
			const FPrimaryAssetId Id = Weapon->GetPrimaryAssetId();
			UCommonButtonBase* const Button = CreateWidget<UCommonButtonBase>(GetOwningPlayer(), ButtonClass);
			if (!Button) continue;
			SetButtonLabel(Button, FText::Format(LOCTEXT("EnhancementPickerRow", "{0} · +{1} · 보유 {2}"),
				PartName(Weapon), GetProgression()->GetUpgradeLevel(Weapon), Stack.Count));
			Button->SetIsSelectable(true);
			Button->SetIsInteractableWhenSelected(true);
			Button->OnClicked().AddWeakLambda(this, [this, Id]()
			{
				if (!bEnhancementPickerOpen) return;
				EnhancementCandidateId = Id;
				for (int32 Index = 0; Index < EnhancementPickerButtons.Num() && Index < EnhancementPickerIds.Num(); ++Index)
					SetSelected(EnhancementPickerButtons[Index], EnhancementPickerIds[Index] == Id);
				SetButtonLocked(Btn_ConfirmOk, false, FText::GetEmpty());
			});
			SetSelected(Button, EnhancementCandidateId == Id);
			if (UScrollBoxSlot* const Row = Cast<UScrollBoxSlot>(EquipmentPickerList->AddChild(Button)))
				Row->SetPadding(FMargin(0.f, 0.f, 10.f, 6.f));
			EnhancementPickerButtons.Add(Button);
			EnhancementPickerIds.Add(Id);
		}
	if (!bComplete || Owned.IsEmpty())
	{
		UCommonTextBlock* const Message = NewRowText(WidgetTree, BodyStyle(), ETextJustify::Left);
		Message->SetText(!bComplete ? LOCTEXT("EnhancementPickerIncomplete", "일부 무기 정의를 확인할 수 없습니다. 먼저 에셋을 확인하세요.")
			: LOCTEXT("EnhancementPickerEmpty", "보유 무기가 없습니다."));
		EquipmentPickerList->AddChild(Message);
	}
	if (Txt_ConfirmBody) Txt_ConfirmBody->SetText(LOCTEXT("EnhancementPickerHelp", "보유 무기를 선택하세요. 대상 지정은 장착이나 저장을 변경하지 않습니다."));
	SetButtonLocked(Btn_ConfirmOk, !bComplete || !EnhancementCandidateId.IsValid(),
		LOCTEXT("EnhancementPickerSelectFirst", "보유 무기를 선택해 주세요."));
}

void UKnightMaintenanceWidget::CloseEnhancementPicker(bool bRestoreFocus)
{
	if (!bEnhancementPickerOpen) return;
	bEnhancementPickerOpen = false;
	EnhancementCandidateId = FPrimaryAssetId();
	EnhancementPickerProfileId.Reset();
	EnhancementPickerButtons.Reset();
	EnhancementPickerIds.Reset();
	if (EquipmentPickerList)
	{
		EquipmentPickerList->ClearChildren();
		if (UWidget* const Frame = EquipmentPickerList->GetParent()) Frame->SetVisibility(ESlateVisibility::Collapsed);
	}
	ShowConfirm(false);
	SetButtonLabel(Btn_ConfirmOk, LOCTEXT("ConfirmCraftLabel", "제작"));
	if (bRestoreFocus && EnhancementTargetButton) EnhancementTargetButton->SetFocus();
}

void UKnightMaintenanceWidget::ConfirmEnhancementTarget()
{
	if (!bEnhancementPickerOpen || bBusy || !EnhancementCandidateId.IsValid()) return;
	const UknightSaveSubsystem* const Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	TArray<FknightItemStack> Owned;
	const bool bComplete = GetProgression() && GetProgression()->GetOwnedWeapons(Owned);
	const bool bStillOwned = Owned.ContainsByPredicate([this](const FknightItemStack& Stack)
	{
		return Stack.Count > 0 && Stack.Item && Stack.Item->GetPrimaryAssetId() == EnhancementCandidateId;
	});
	if (!Saves || Saves->GetActiveSlotId() != EnhancementPickerProfileId || !bComplete || !bStillOwned)
	{
		RebuildEnhancementPicker();
		return;
	}
	EnhancementTargetId = EnhancementCandidateId;
	UpgradeResultMessage = FText::GetEmpty();
	UpgradeSaveStatusMessage = FText::GetEmpty();
	CloseEnhancementPicker(false);
	RebuildEnhancementTarget();
	if (EnhancementTargetButton) EnhancementTargetButton->SetFocus();
}

void UKnightMaintenanceWidget::RebuildEquipmentSlots(bool bRestoreFocus)
{
	if (!List_Items || !WidgetTree) return;
	FGameplayTag FocusedSlot;
	if (bRestoreFocus)
		for (int32 Index = 0; Index < EquipmentSlotButtons.Num() && Index < MainEquipmentSlotTags().Num(); ++Index)
			if (EquipmentSlotButtons[Index] && (EquipmentSlotButtons[Index]->HasAnyUserFocus()
				|| EquipmentSlotButtons[Index]->HasFocusedDescendants())) FocusedSlot = MainEquipmentSlotTags()[Index];
	List_Items->ClearChildren();
	EquipmentSlotButtons.Reset();
	Entries.Reset();
	SocketListButtons.Reset();
	TArray<FKnightMainEquipmentSlot> Slots;
	const bool bComplete = GetProgression() && GetProgression()->GetMainEquipmentSlots(Slots);
	const TSubclassOf<UCommonButtonBase> ButtonClass = LoadClass<UCommonButtonBase>(nullptr,
		TEXT("/Game/Knight/UI/Common/WBP_KnightButton.WBP_KnightButton_C"));
	if (!ButtonClass || !GetOwningPlayer())
	{
		if (Txt_Status) Txt_Status->SetText(LOCTEXT("EquipmentButtonMissing", "장비 슬롯 버튼을 불러올 수 없습니다."));
		return;
	}
	for (const FGameplayTag Tag : MainEquipmentSlotTags())
	{
		const FKnightMainEquipmentSlot* Entry = Slots.FindByPredicate([Tag](const FKnightMainEquipmentSlot& Item) { return Item.SlotTag == Tag; });
		const FText PartLabel = Entry && Entry->Part ? PartName(Entry->Part)
			: bComplete ? LOCTEXT("EquipmentEmpty", "빈 슬롯") : LOCTEXT("EquipmentUnresolved", "확인 불가");
		UCommonButtonBase* const Button = CreateWidget<UCommonButtonBase>(GetOwningPlayer(), ButtonClass);
		if (!Button) continue;
		SetButtonLabel(Button, FText::Format(LOCTEXT("EquipmentSlotRow", "{0}  ·  {1}"), MainEquipmentSlotLabel(Tag), PartLabel));
		Button->SetIsSelectable(true);
		Button->SetIsInteractableWhenSelected(true);
		Button->OnClicked().AddWeakLambda(this, [this, Tag]() { SelectEquipmentSlot(Tag); });
		SetSelected(Button, SelectedEquipmentSlot == Tag);
		if (UScrollBoxSlot* const Row = Cast<UScrollBoxSlot>(List_Items->AddChild(Button)))
			Row->SetPadding(FMargin(0.f, 0.f, 10.f, 6.f));
		EquipmentSlotButtons.Add(Button);
	}
	if (Txt_Status && !bComplete) Txt_Status->SetText(LOCTEXT("EquipmentLoadIncomplete", "일부 장비 데이터를 확인할 수 없습니다. 저장 슬롯과 에셋을 확인하세요."));
	RefreshTabButtons();
	RefreshEquipmentDetails();
	if (bEquipmentPickerOpen) RebuildEquipmentPicker();
	if (FocusedSlot.IsValid())
		for (int32 Index = 0; Index < EquipmentSlotButtons.Num() && Index < MainEquipmentSlotTags().Num(); ++Index)
			if (MainEquipmentSlotTags()[Index] == FocusedSlot) { EquipmentSlotButtons[Index]->SetFocus(); break; }
}

void UKnightMaintenanceWidget::SelectEquipmentSlot(FGameplayTag SlotTag)
{
	if (Category != 2 || bEquipmentPickerOpen || !MainEquipmentSlotTags().Contains(SlotTag)) return;
	SelectedEquipmentSlot = SelectedEquipmentSlot == SlotTag ? FGameplayTag() : SlotTag;
	for (int32 Index = 0; Index < EquipmentSlotButtons.Num() && Index < MainEquipmentSlotTags().Num(); ++Index)
		SetSelected(EquipmentSlotButtons[Index], SelectedEquipmentSlot == MainEquipmentSlotTags()[Index]);
	if (Txt_Status) Txt_Status->SetText(FText::GetEmpty());
	RefreshEquipmentDetails();
	OnSlotFocused.ExecuteIfBound(SelectedEquipmentSlot);
}

const UknightPartItemDefinition* UKnightMaintenanceWidget::GetEquippedInSelectedSlot(bool& bComplete) const
{
	TArray<FKnightMainEquipmentSlot> Slots;
	bComplete = GetProgression() && GetProgression()->GetMainEquipmentSlots(Slots);
	const FKnightMainEquipmentSlot* Entry = Slots.FindByPredicate([this](const FKnightMainEquipmentSlot& Item)
	{
		return Item.SlotTag == SelectedEquipmentSlot;
	});
	return Entry ? Entry->Part.Get() : nullptr;
}

bool UKnightMaintenanceWidget::GetCompatibleOwnedEquipment(FGameplayTag SlotTag,
	TArray<const UknightPartItemDefinition*>& OutParts) const
{
	OutParts.Reset();
	UknightProgressionSubsystem* const Progression = GetProgression();
	if (!Progression || !MainEquipmentSlotTags().Contains(SlotTag)) return false;
	TArray<FknightItemStack> Owned;
	const bool bComplete = SlotTag == knightGameplayTags::Equipment_Slot_Weapon
		? Progression->GetOwnedWeapons(Owned) : Progression->GetOwnedArmorForSlot(SlotTag, Owned);
	for (const FknightItemStack& Stack : Owned)
	{
		const UknightPartItemDefinition* Part = Cast<UknightPartItemDefinition>(Stack.Item);
		if (Stack.Count > 0 && Part && Part->SlotTag == SlotTag) OutParts.Add(Part);
	}
	return bComplete;
}

void UKnightMaintenanceWidget::OpenEquipmentPicker()
{
	if (Category != 2 || !SelectedEquipmentSlot.IsValid() || !Panel_Confirm || !EquipmentPickerList || bBusy) return;
	bEquipmentPickerOpen = true;
	EquipmentPickerCandidate = nullptr;
	if (UWidget* const Frame = EquipmentPickerList->GetParent()) Frame->SetVisibility(ESlateVisibility::Visible);
	if (Txt_ConfirmTitle) Txt_ConfirmTitle->SetText(FText::Format(LOCTEXT("EquipmentPickerTitle", "{0} 교체"),
		MainEquipmentSlotLabel(SelectedEquipmentSlot)));
	SetButtonLabel(Btn_ConfirmOk, LOCTEXT("EquipmentPickerEquip", "장착"));
	RebuildEquipmentPicker();
	ShowConfirm(true);
	if (!EquipmentPickerButtons.IsEmpty()) EquipmentPickerButtons[0]->SetFocus();
	else if (Btn_ConfirmCancel) Btn_ConfirmCancel->SetFocus();
}

void UKnightMaintenanceWidget::RebuildEquipmentPicker()
{
	if (!bEquipmentPickerOpen || !EquipmentPickerList || !WidgetTree) return;
	EquipmentPickerList->ClearChildren();
	EquipmentPickerButtons.Reset();
	EquipmentPickerParts.Reset();
	TArray<const UknightPartItemDefinition*> Owned;
	const bool bComplete = GetCompatibleOwnedEquipment(SelectedEquipmentSlot, Owned);
	if (EquipmentPickerCandidate && !Owned.Contains(EquipmentPickerCandidate.Get())) EquipmentPickerCandidate = nullptr;
	const TSubclassOf<UCommonButtonBase> ButtonClass = LoadClass<UCommonButtonBase>(nullptr,
		TEXT("/Game/Knight/UI/Common/WBP_KnightButton.WBP_KnightButton_C"));
	if (ButtonClass && GetOwningPlayer())
		for (const UknightPartItemDefinition* Part : Owned)
		{
			UCommonButtonBase* const Button = CreateWidget<UCommonButtonBase>(GetOwningPlayer(), ButtonClass);
			if (!Button) continue;
			SetButtonLabel(Button, FText::Format(LOCTEXT("EquipmentPickerRow", "{0}  ·  보유 {1}"),
				PartName(Part), GetProgression() ? GetProgression()->GetItemCount(Part) : 0));
			Button->SetIsSelectable(true);
			Button->SetIsInteractableWhenSelected(true);
			TWeakObjectPtr<const UknightPartItemDefinition> WeakPart(Part);
			Button->OnClicked().AddWeakLambda(this, [this, WeakPart]()
			{
				if (!bEquipmentPickerOpen || !WeakPart.IsValid()) return;
				EquipmentPickerCandidate = WeakPart.Get();
				RefreshEquipmentPickerPreview();
			});
			SetSelected(Button, EquipmentPickerCandidate == Part);
			if (UScrollBoxSlot* const Row = Cast<UScrollBoxSlot>(EquipmentPickerList->AddChild(Button)))
				Row->SetPadding(FMargin(0.f, 0.f, 10.f, 6.f));
			EquipmentPickerButtons.Add(Button);
			EquipmentPickerParts.Add(Part);
		}
	if (!bComplete || Owned.IsEmpty())
	{
		UCommonTextBlock* const Message = NewRowText(WidgetTree, BodyStyle(), ETextJustify::Left);
		Message->SetText(!bComplete ? LOCTEXT("EquipmentPickerUnresolved", "보유 장비 정의를 모두 확인할 수 없습니다. 장착을 잠시 사용할 수 없습니다.")
			: LOCTEXT("EquipmentPickerEmpty", "이 슬롯에 장착할 보유 장비가 없습니다."));
		EquipmentPickerList->AddChild(Message);
	}
	RefreshEquipmentPickerPreview();
}

void UKnightMaintenanceWidget::RefreshEquipmentPickerPreview()
{
	if (!bEquipmentPickerOpen) return;
	TArray<const UknightPartItemDefinition*> Owned;
	const bool bComplete = GetCompatibleOwnedEquipment(SelectedEquipmentSlot, Owned);
	if (EquipmentPickerCandidate && !Owned.Contains(EquipmentPickerCandidate.Get())) EquipmentPickerCandidate = nullptr;
	for (int32 Index = 0; Index < EquipmentPickerButtons.Num() && Index < EquipmentPickerParts.Num(); ++Index)
		SetSelected(EquipmentPickerButtons[Index], EquipmentPickerCandidate == EquipmentPickerParts[Index]);
	bool bSlotComplete = false;
	const UknightPartItemDefinition* const Equipped = GetEquippedInSelectedSlot(bSlotComplete);
	const UknightPartItemDefinition* const Candidate = EquipmentPickerCandidate.Get();
	if (Txt_ConfirmBody)
	{
		FText Detail = LOCTEXT("EquipmentPickerPrompt", "보유 장비를 선택한 뒤 장착을 누르세요. 선택만으로 장비와 저장값은 바뀌지 않습니다.");
		if (Candidate)
		{
			const UknightMaintenanceCatalog* const Catalog = UknightEquipmentSettings::LoadCatalog();
			const FknightEquipmentComparison Comparison = knightEquipmentStats::CompareReplacement(
				knightEquipmentStats::ReadLoadout(GetProgression(), Catalog), SelectedEquipmentSlot, Candidate, Catalog, GetProgression());
			TArray<FText> Changes;
			if (Comparison.bExact)
				for (const FStatRow& Row : StatRows())
				{
					const int32 Delta = DisplayNumber(Comparison.Deltas.FindRef(Row.Attribute), Row.bPercent);
					if (Delta != 0) Changes.Add(FText::Format(LOCTEXT("EquipmentPickerDelta", "{0} {1}"),
						Row.Label, FormatNumber(Delta, Row.bPercent, true)));
				}
			Detail = FText::Format(LOCTEXT("EquipmentPickerComparison", "{0}\n교체 후 변화: {1}"), PartName(Candidate),
				Comparison.bExact ? (Changes.IsEmpty() ? LOCTEXT("EquipmentPickerNoChange", "표시 스탯 변화 없음")
					: FText::Join(FText::FromString(TEXT(" · ")), Changes))
					: FText::Format(LOCTEXT("EquipmentPickerUnknown", "정확한 계산 불가: {0}"), FText::FromString(Comparison.IncompleteReason)));
		}
		Txt_ConfirmBody->SetText(Detail);
	}
	const UknightSaveSubsystem* const Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	SetButtonLocked(Btn_ConfirmOk, !bComplete || !bSlotComplete || !Candidate || Candidate == Equipped
		|| !Saves || !Saves->CanStartAtomicTransaction(),
		LOCTEXT("EquipmentPickerBlocked", "장비 선택·보유 상태·저장 상태를 확인하세요."));
	if (Candidate)
		for (int32 Index = 0; Index < EquipmentPickerParts.Num() && Index < EquipmentPickerButtons.Num(); ++Index)
			if (EquipmentPickerParts[Index] == Candidate) { EquipmentPickerButtons[Index]->SetFocus(); break; }
	else if (Panel_Confirm && Panel_Confirm->IsVisible())
	{
		if (!EquipmentPickerButtons.IsEmpty()) EquipmentPickerButtons[0]->SetFocus();
		else if (Btn_ConfirmCancel) Btn_ConfirmCancel->SetFocus();
	}
}

void UKnightMaintenanceWidget::CloseEquipmentPicker(bool bRestoreFocus)
{
	if (!bEquipmentPickerOpen) return;
	bEquipmentPickerOpen = false;
	EquipmentPickerCandidate = nullptr;
	if (EquipmentPickerList)
	{
		EquipmentPickerList->ClearChildren();
		if (UWidget* const Frame = EquipmentPickerList->GetParent()) Frame->SetVisibility(ESlateVisibility::Collapsed);
	}
	EquipmentPickerButtons.Reset();
	EquipmentPickerParts.Reset();
	ShowConfirm(false);
	SetButtonLabel(Btn_ConfirmOk, LOCTEXT("ConfirmCraftLabel", "제작"));
	if (bRestoreFocus)
		for (int32 Index = 0; Index < EquipmentSlotButtons.Num() && Index < MainEquipmentSlotTags().Num(); ++Index)
			if (MainEquipmentSlotTags()[Index] == SelectedEquipmentSlot) { EquipmentSlotButtons[Index]->SetFocus(); break; }
}

void UKnightMaintenanceWidget::ConfirmEquipmentReplacement()
{
	if (!bEquipmentPickerOpen || !EquipmentPickerCandidate || bBusy) return;
	TArray<const UknightPartItemDefinition*> Owned;
	bool bSlotComplete = false;
	const UknightPartItemDefinition* const Current = GetEquippedInSelectedSlot(bSlotComplete);
	const UknightSaveSubsystem* const Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	if (!GetCompatibleOwnedEquipment(SelectedEquipmentSlot, Owned) || !bSlotComplete
		|| !Owned.Contains(EquipmentPickerCandidate.Get()) || EquipmentPickerCandidate == Current
		|| !Saves || !Saves->CanStartAtomicTransaction())
	{
		if (Txt_Status) Txt_Status->SetText(LOCTEXT("EquipmentPickerStale", "보유·장착 또는 저장 상태가 변경되었습니다. 다시 선택하세요."));
		RebuildEquipmentPicker();
		return;
	}
	ExecuteSelection();
}

void UKnightMaintenanceWidget::RebuildSocketList()
{
	if (!List_Items || !WidgetTree) return;
	int32 FocusedSlot = INDEX_NONE;
	FPrimaryAssetId FocusedCandidate;
	for (int32 Index = 0; Index < SocketSlotButtons.Num(); ++Index)
		if (SocketSlotButtons[Index] && (SocketSlotButtons[Index]->HasAnyUserFocus() || SocketSlotButtons[Index]->HasFocusedDescendants()))
			FocusedSlot = Index;
	for (int32 Index = 0; Index < SocketCandidateButtons.Num() && Index < SocketCandidateIds.Num(); ++Index)
		if (SocketCandidateButtons[Index] && (SocketCandidateButtons[Index]->HasAnyUserFocus() || SocketCandidateButtons[Index]->HasFocusedDescendants()))
			FocusedCandidate = SocketCandidateIds[Index];
	List_Items->ClearChildren();
	Entries.Reset();
	SocketListButtons.Reset();
	SocketSlotButtons.Reset();
	SocketCandidateButtons.Reset();
	SocketCandidateIds.Reset();
	UknightProgressionSubsystem* const Progression = GetProgression();
	const int32 Slots = Progression ? Progression->GetUnlockedSharedSocketSlots() : 0;
	TArray<FKnightSocketAssignment> Assignments;
	const bool bAssignmentsComplete = Progression && Progression->GetSocketAssignments(Assignments);
	if (SelectedSocketSlot >= FMath::Min(Slots, 3) || SelectedSocketSlot < INDEX_NONE)
	{
		SelectedSocketSlot = INDEX_NONE;
		SelectedSocketItem = nullptr;
		SelectedSocketCandidateId = FPrimaryAssetId();
	}
	const TSubclassOf<UCommonButtonBase> ButtonClass = LoadClass<UCommonButtonBase>(nullptr,
		TEXT("/Game/Knight/UI/Common/WBP_KnightButton.WBP_KnightButton_C"));
	if (!ButtonClass || !GetOwningPlayer()) return;
	auto AddButton = [this, ButtonClass](FName Name, const FText& Label, TFunction<void()> OnClick,
		bool bSelected, bool bLocked, UTexture2D* Icon) -> UCommonButtonBase*
	{
		UCommonButtonBase* const Button = CreateWidget<UCommonButtonBase>(GetOwningPlayer(), ButtonClass, Name);
		if (!Button) return nullptr;
		SetButtonLabel(Button, Label);
		Button->SetIsSelectable(true);
		Button->SetIsInteractableWhenSelected(true);
		SetSelected(Button, bSelected);
		Button->OnClicked().AddWeakLambda(this, [Click = MoveTemp(OnClick)]() { Click(); });
		if (bLocked) SetButtonLocked(Button, true, LOCTEXT("SocketLockedReason", "이 소켓 칸은 아직 해금되지 않았습니다."));
		UWidget* RowContent = Button;
		if (Icon)
		{
			UHorizontalBox* const Row = WidgetTree->ConstructWidget<UHorizontalBox>();
			USizeBox* const IconFrame = WidgetTree->ConstructWidget<USizeBox>();
			IconFrame->SetWidthOverride(40.f);
			IconFrame->SetHeightOverride(40.f);
			UImage* const Image = WidgetTree->ConstructWidget<UImage>();
			Image->SetBrushFromTexture(Icon);
			IconFrame->AddChild(Image);
			Row->AddChildToHorizontalBox(IconFrame)->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			Row->AddChildToHorizontalBox(Button)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			RowContent = Row;
		}
		if (UScrollBoxSlot* const ScrollSlot = Cast<UScrollBoxSlot>(List_Items->AddChild(RowContent)))
			ScrollSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 6.f));
		SocketListButtons.Add(Button);
		return Button;
	};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const bool bUnlocked = Index < Slots;
		const FKnightSocketAssignment* const Assignment = Assignments.IsValidIndex(Index) ? &Assignments[Index] : nullptr;
		const bool bUnresolved = bUnlocked && Assignment && Assignment->ItemId.IsValid() && !Assignment->Item;
		const UknightSocketItemDefinition* const Equipped = Assignment ? Assignment->Item.Get() : nullptr;
		const FText State = !bUnlocked ? LOCTEXT("SocketLocked", "잠김")
			: bUnresolved ? LOCTEXT("SocketUnresolved", "장착 정보 확인 불가")
			: Equipped ? (Equipped->DisplayName.IsEmpty() ? FText::FromString(Equipped->GetName()) : Equipped->DisplayName)
			: LOCTEXT("SocketEmpty", "빈 칸");
		UCommonButtonBase* const Button = AddButton(FName(*FString::Printf(TEXT("Phase8_SocketSlot%d"), Index)),
			FText::Format(LOCTEXT("SocketSlotRow", "소켓 {0} · {1}"), Index + 1, State),
			[this, Index]() { SelectSocketSlot(Index); }, SelectedSocketSlot == Index, !bUnlocked,
			Equipped ? Equipped->Icon.LoadSynchronous() : nullptr);
		SocketSlotButtons.Add(Button);
	}
	if (SelectedSocketSlot == INDEX_NONE)
	{
		UCommonTextBlock* const Prompt = NewRowText(WidgetTree, BodyStyle(), ETextJustify::Left);
		Prompt->SetText(LOCTEXT("SocketSelectPrompt", "해금된 소켓 칸을 선택하세요. 같은 칸을 다시 누르면 전체 효과로 돌아갑니다."));
		Prompt->SetAutoWrapText(true);
		List_Items->AddChild(Prompt);
	}
	TArray<FknightItemStack> Owned;
	const bool bOwnedComplete = Progression && Progression->GetOwnedSockets(Owned);
	Owned.Sort([](const FknightItemStack& A, const FknightItemStack& B)
	{
		return A.Item && B.Item ? A.Item->DisplayName.ToString() < B.Item->DisplayName.ToString() : A.Item != nullptr;
	});
	SelectedSocketItem = nullptr;
	for (const FknightItemStack& Stack : Owned)
		if (Stack.Count > 0 && Stack.Item && Stack.Item->GetPrimaryAssetId() == SelectedSocketCandidateId)
			SelectedSocketItem = Cast<UknightSocketItemDefinition>(Stack.Item);
	if (!SelectedSocketItem) SelectedSocketCandidateId = FPrimaryAssetId();
	if (SelectedSocketSlot != INDEX_NONE)
	{
		if (!bOwnedComplete)
		{
			UCommonTextBlock* const Warning = NewRowText(WidgetTree, BodyStyle(), ETextJustify::Left);
			Warning->SetText(LOCTEXT("SocketOwnedUnresolved", "보유 소켓 정의 일부를 확인할 수 없습니다. 장착은 보류됩니다."));
			Warning->SetAutoWrapText(true);
			List_Items->AddChild(Warning);
		}
		if (Owned.IsEmpty())
		{
			UCommonTextBlock* const Empty = NewRowText(WidgetTree, BodyStyle(), ETextJustify::Left);
			Empty->SetText(LOCTEXT("SocketNoOwned", "보유 소켓 아이템이 없습니다."));
			List_Items->AddChild(Empty);
		}
		for (const FknightItemStack& Stack : Owned)
		{
			const UknightSocketItemDefinition* const Item = Cast<UknightSocketItemDefinition>(Stack.Item);
			if (!Item || Stack.Count <= 0) continue;
			const int32 Available = Progression->GetAvailableSocketItemCount(Item);
			const FText Name = Item->DisplayName.IsEmpty() ? FText::FromString(Item->GetName()) : Item->DisplayName;
			UCommonButtonBase* const Button = AddButton(FName(*FString::Printf(TEXT("Phase8_SocketCandidate%d"), SocketCandidateButtons.Num())),
				FText::Format(LOCTEXT("SocketOwnedRow", "{0} · 보유 {1} / 사용 가능 {2}\n{3}"),
					Name, Stack.Count, Available, SocketItemEffectText(Item)),
				[this, Item]() { SelectSocketCandidate(Item); }, SelectedSocketCandidateId == Item->GetPrimaryAssetId(),
				false, Item->Icon.LoadSynchronous());
			SocketCandidateButtons.Add(Button);
			SocketCandidateIds.Add(Item->GetPrimaryAssetId());
		}
	}
	if (!bAssignmentsComplete && Txt_Status)
		Txt_Status->SetText(LOCTEXT("SocketAssignmentsUnresolved", "장착 소켓 정보를 모두 확인할 수 없습니다. 변경은 보류됩니다."));
	else if (Slots > 3 && Txt_Status)
		Txt_Status->SetText(LOCTEXT("SocketOverflowReadonly", "저장된 소켓 칸이 3개를 초과합니다. 모든 효과는 유지되며 이 화면에서는 변경할 수 없습니다."));
	RefreshTabButtons();
	RefreshSocketDetails();
	if (FocusedSlot >= 0 && SocketSlotButtons.IsValidIndex(FocusedSlot) && SocketSlotButtons[FocusedSlot])
		SocketSlotButtons[FocusedSlot]->SetFocus();
	else if (FocusedCandidate.IsValid())
		for (int32 Index = 0; Index < SocketCandidateIds.Num(); ++Index)
			if (SocketCandidateIds[Index] == FocusedCandidate && SocketCandidateButtons[Index])
			{
				SocketCandidateButtons[Index]->SetFocus();
				break;
			}
}

bool UKnightMaintenanceWidget::CanEditSocketProfile() const
{
	const UknightProgressionSubsystem* const Progression = GetProgression();
	if (!Progression || Progression->GetUnlockedSharedSocketSlots() > 3) return false;
	TArray<FKnightSocketAssignment> Assignments;
	return Progression->GetSocketAssignments(Assignments);
}

bool UKnightMaintenanceWidget::CanEquipSocketCandidate(int32 SlotIndex, const UknightSocketItemDefinition* Item) const
{
	const UknightProgressionSubsystem* const Progression = GetProgression();
	if (!Item || !Progression || SlotIndex < 0 || SlotIndex >= 3
		|| SlotIndex >= Progression->GetUnlockedSharedSocketSlots()
		|| Progression->GetAvailableSocketItemCount(Item) <= 0) return false;
	TArray<FKnightSocketAssignment> Assignments;
	if (!Progression->GetSocketAssignments(Assignments)) return false;
	const FPrimaryAssetId Id = Item->GetPrimaryAssetId();
	if (Assignments.IsValidIndex(SlotIndex) && Assignments[SlotIndex].ItemId == Id) return false;
	if (!Item->bAllowDuplicateEquip)
		for (const FKnightSocketAssignment& Assignment : Assignments)
			if (Assignment.SlotIndex != SlotIndex && Assignment.ItemId == Id) return false;
	return true;
}

void UKnightMaintenanceWidget::SelectSocketSlot(int32 SlotIndex)
{
	const UknightProgressionSubsystem* const Progression = GetProgression();
	if (!Progression || SlotIndex < 0 || SlotIndex >= 3
		|| SlotIndex >= Progression->GetUnlockedSharedSocketSlots()) return;
	SelectedSocketSlot = SelectedSocketSlot == SlotIndex ? INDEX_NONE : SlotIndex;
	SelectedSocketCandidateId = FPrimaryAssetId();
	SelectedSocketItem = nullptr;
	if (Txt_Status) Txt_Status->SetText(FText::GetEmpty());
	RebuildSocketList();
}

void UKnightMaintenanceWidget::SelectSocketCandidate(const UknightSocketItemDefinition* Item)
{
	const UknightProgressionSubsystem* const Progression = GetProgression();
	if (!Item || !Progression || SelectedSocketSlot < 0 || SelectedSocketSlot >= 3
		|| SelectedSocketSlot >= Progression->GetUnlockedSharedSocketSlots()) return;
	TArray<FknightItemStack> Owned;
	if (!Progression->GetOwnedSockets(Owned) || !Owned.ContainsByPredicate([Item](const FknightItemStack& Stack)
	{
		return Stack.Count > 0 && Stack.Item == Item;
	})) return;
	SelectedSocketCandidateId = Item->GetPrimaryAssetId();
	SelectedSocketItem = Item;
	if (Txt_Status) Txt_Status->SetText(FText::GetEmpty());
	RebuildSocketList();
}

void UKnightMaintenanceWidget::RequestSocketEquip()
{
	if (bBusy || Category != 3 || SelectedSocketSlot < 0 || SelectedSocketSlot >= 3) return;
	UknightProgressionSubsystem* const Progression = GetProgression();
	UknightSaveSubsystem* const Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	const UknightSocketItemDefinition* const Candidate = SelectedSocketItem.Get();
	TArray<FknightItemStack> Owned;
	const bool bOwnedComplete = Progression && Progression->GetOwnedSockets(Owned);
	const bool bStillOwned = Candidate && Owned.ContainsByPredicate([Candidate](const FknightItemStack& Stack)
	{
		return Stack.Count > 0 && Stack.Item == Candidate;
	});
	if (!Saves || !Saves->CanStartAtomicTransaction() || !CanEditSocketProfile() || !bOwnedComplete
		|| !bStillOwned || Candidate->GetPrimaryAssetId() != SelectedSocketCandidateId
		|| !CanEquipSocketCandidate(SelectedSocketSlot, Candidate))
	{
		if (Txt_Status) Txt_Status->SetText(LOCTEXT("SocketEquipRejected", "소켓 장착 불가: 슬롯·보유·중복·저장 상태를 확인하세요."));
		RebuildSocketList();
		return;
	}
	bBusy = true;
	const bool bSuccess = Progression->EquipSocketItem(SelectedSocketSlot, Candidate);
	bBusy = false;
	if (bSuccess)
	{
		SelectedSocketItem = nullptr;
		SelectedSocketCandidateId = FPrimaryAssetId();
		bAwaitingSocketSave = true;
		bSocketSaveFailed = false;
		PendingSocketProfileId = Saves->GetActiveSlotId();
	}
	if (Txt_Status) Txt_Status->SetText(bSuccess ? LOCTEXT("SocketEquipped", "소켓을 장착했습니다.")
		: LOCTEXT("SocketEquipRejected", "소켓 장착 불가: 슬롯·보유·중복·저장 상태를 확인하세요."));
	RebuildSocketList();
}

void UKnightMaintenanceWidget::RequestSocketUnequip()
{
	if (bBusy || Category != 3 || SelectedSocketSlot < 0 || SelectedSocketSlot >= 3) return;
	UknightProgressionSubsystem* const Progression = GetProgression();
	UknightSaveSubsystem* const Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	if (!Saves || !Saves->CanStartAtomicTransaction() || !CanEditSocketProfile() || !Progression
		|| SelectedSocketSlot >= Progression->GetUnlockedSharedSocketSlots()
		|| !Progression->GetEquippedSocketItem(SelectedSocketSlot))
	{
		if (Txt_Status) Txt_Status->SetText(LOCTEXT("SocketUnequipRejected", "소켓 해제 불가: 장착·슬롯·저장 상태를 확인하세요."));
		RebuildSocketList();
		return;
	}
	bBusy = true;
	const bool bSuccess = Progression->UnequipSocketItem(SelectedSocketSlot);
	bBusy = false;
	if (bSuccess)
	{
		SelectedSocketItem = nullptr;
		SelectedSocketCandidateId = FPrimaryAssetId();
		bAwaitingSocketSave = true;
		bSocketSaveFailed = false;
		PendingSocketProfileId = Saves->GetActiveSlotId();
	}
	if (Txt_Status) Txt_Status->SetText(bSuccess ? LOCTEXT("SocketUnequipped", "소켓을 해제했습니다.")
		: LOCTEXT("SocketUnequipRejected", "소켓 해제 불가: 장착·슬롯·저장 상태를 확인하세요."));
	RebuildSocketList();
}

void UKnightMaintenanceWidget::RefreshSocketDetails()
{
	UknightProgressionSubsystem* const Progression = GetProgression();
	const int32 Slots = Progression ? Progression->GetUnlockedSharedSocketSlots() : 0;
	TArray<FKnightSocketAssignment> Assignments;
	const bool bAssignmentsComplete = Progression && Progression->GetSocketAssignments(Assignments);
	const FKnightSocketAssignment* const Assignment = Assignments.IsValidIndex(SelectedSocketSlot)
		? &Assignments[SelectedSocketSlot] : nullptr;
	const UknightSocketItemDefinition* const Equipped = Assignment ? Assignment->Item.Get() : nullptr;
	const bool bUnresolved = Assignment && Assignment->ItemId.IsValid() && !Equipped;
	if (Txt_PartName) Txt_PartName->SetText(SelectedSocketSlot == INDEX_NONE
		? LOCTEXT("SocketCombinedTitle", "현재 활성 소켓 효과")
		: bUnresolved ? LOCTEXT("SocketUnknownTitle", "장착 소켓 확인 불가")
		: Equipped ? (Equipped->DisplayName.IsEmpty() ? FText::FromString(Equipped->GetName()) : Equipped->DisplayName)
		: LOCTEXT("SocketEmptyTitle", "빈 소켓 칸"));
	if (Txt_PartState) Txt_PartState->SetText(Slots > 3
		? FText::Format(LOCTEXT("SocketOverflowState", "해금 {0}칸 · 화면 표시 3칸 · 읽기 전용"), Slots)
		: FText::Format(LOCTEXT("SocketState", "해금 {0} / 3칸 · {1}"), Slots,
			SelectedSocketSlot == INDEX_NONE ? LOCTEXT("SocketNoSelection", "전체 효과")
			: FText::Format(LOCTEXT("SocketSelectedIndex", "소켓 {0} 선택"), SelectedSocketSlot + 1)));
	if (Txt_PartDesc)
	{
		Txt_PartDesc->SetText(SelectedSocketSlot == INDEX_NONE
			? LOCTEXT("SocketCombinedDescription", "실제로 장착된 소켓의 합산 효과입니다. 왼쪽에서 해금된 칸을 선택하세요.")
			: bUnresolved ? FText::Format(LOCTEXT("SocketUnknownId", "저장된 아이템 ID: {0}"),
				FText::FromString(Assignment->ItemId.ToString()))
			: Equipped ? Equipped->Description
			: LOCTEXT("SocketEmptyDescription", "보유 소켓을 선택한 다음 장착을 누르세요."));
		Txt_PartDesc->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (Img_Selected)
	{
		UTexture2D* const Icon = Equipped ? Equipped->Icon.LoadSynchronous() : nullptr;
		if (Icon) Img_Selected->SetBrushFromTexture(Icon);
		Img_Selected->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Stats_Rows) Stats_Rows->SetVisibility(ESlateVisibility::Collapsed);
	if (Txt_ActiveSets) Txt_ActiveSets->SetVisibility(ESlateVisibility::Collapsed);
	if (Txt_SetInfo) Txt_SetInfo->SetVisibility(ESlateVisibility::Collapsed);
	TArray<FText> Lines;
	if (SelectedSocketSlot == INDEX_NONE && Progression)
	{
		TArray<FText> Active;
		if (const int32 Kit = Progression->GetRepairKitCapacityBonus())
			Active.Add(FText::Format(LOCTEXT("SocketActiveKit", "수리키트 최대 +{0}"), Kit));
		if (const float AttackMultiplier = Progression->GetAttackStaminaCostMultiplier(); AttackMultiplier < 1.f)
			Active.Add(FText::Format(LOCTEXT("SocketActiveAttack", "공격 스태미너 비용 -{0}"),
				FText::AsPercent(1.f - AttackMultiplier)));
		if (const int32 Charge = Progression->GetSocketEffectCount(EKnightSocketEffect::SocketGaugeCharge))
		{
			const AknightCharacter* const Character = Cast<AknightCharacter>(GetOwningPlayerPawn());
			const int32 Base = Character ? Character->GetBaseRepairKitGuardsPerKit() : 0;
			Active.Add(Base > 0
				? FText::Format(LOCTEXT("SocketActiveChargeExact", "키트 충전 퍼펙트 가드 {0}회 → {1}회"),
					Base, Progression->GetSocketGaugeGuardsPerKit(Base))
				: FText::Format(LOCTEXT("SocketActiveChargeContext", "충전 보조 {0}개 · 전투 플레이어 기준 횟수 확인 불가"), Charge));
		}
		for (const EKnightSocketEffect Effect : { EKnightSocketEffect::LoadCapacity, EKnightSocketEffect::StaminaRecovery, EKnightSocketEffect::FullHealthAttack })
		{
			if (const float Total = Progression->GetSocketEffectTotal(Effect); Total > 0.f)
				Active.Add(FText::Format(LOCTEXT("SocketActivePercent", "{0} +{1}"),
					SocketEffectName(Effect), FText::AsPercent(Total)));
		}
		Lines.Add(Active.IsEmpty() ? LOCTEXT("SocketActiveNone", "장착된 소켓 효과 없음")
			: FText::Join(FText::FromString(TEXT("\n")), Active));
		if (Slots > 3)
		{
			Lines.Add(FText::Format(LOCTEXT("SocketOverflowCount", "추가 저장 슬롯 {0}개도 위 활성 효과에 포함됩니다. 이 화면은 읽기 전용입니다."), Slots - 3));
			for (int32 Index = 3; Index < Assignments.Num(); ++Index)
				if (Assignments[Index].ItemId.IsValid())
					Lines.Add(FText::Format(LOCTEXT("SocketOverflowItem", "소켓 {0}: {1}"), Index + 1,
						Assignments[Index].Item ? (Assignments[Index].Item->DisplayName.IsEmpty()
							? FText::FromString(Assignments[Index].Item->GetName()) : Assignments[Index].Item->DisplayName)
							: FText::Format(LOCTEXT("SocketOverflowUnknownId", "아이템 확인 불가 ({0})"),
								FText::FromString(Assignments[Index].ItemId.ToString()))));
		}
	}
	else if (bUnresolved)
		Lines.Add(LOCTEXT("SocketUnknownEffect", "저장된 아이템 정의를 확인할 수 없어 이 칸의 효과를 표시할 수 없습니다."));
	else if (Equipped)
	{
		Lines.Add(SocketItemEffectText(Equipped));
		if (Equipped->Effect == EKnightSocketEffect::SocketGaugeCharge && Progression)
		{
			const AknightCharacter* const Character = Cast<AknightCharacter>(GetOwningPlayerPawn());
			const int32 Base = Character ? Character->GetBaseRepairKitGuardsPerKit() : 0;
			Lines.Add(Base > 0
				? FText::Format(LOCTEXT("SocketIndividualChargeExact", "현재 전체 장착 기준: {0}회 → {1}회"),
					Base, Progression->GetSocketGaugeGuardsPerKit(Base))
				: LOCTEXT("SocketIndividualChargeContext", "전투 플레이어의 기준 충전 횟수는 현재 확인할 수 없습니다."));
		}
	}
	else Lines.Add(LOCTEXT("SocketEmptyEffect", "이 칸에는 활성 효과가 없습니다. 보유 아이템을 선택한 뒤 장착하세요."));
	if (!bAssignmentsComplete) Lines.Add(LOCTEXT("SocketIncompleteSummary", "일부 장착 정의가 확인되지 않아 효과 요약이 불완전할 수 있습니다."));
	if (Txt_Bonus)
	{
		Txt_Bonus->SetText(FText::Join(FText::FromString(TEXT("\n")), Lines));
		Txt_Bonus->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	TArray<FknightItemStack> Owned;
	const bool bOwnedComplete = Progression && Progression->GetOwnedSockets(Owned);
	const UknightSocketItemDefinition* const Candidate = SelectedSocketItem.Get();
	const bool bCandidateOwned = Candidate && Owned.ContainsByPredicate([Candidate](const FknightItemStack& Stack)
	{
		return Stack.Count > 0 && Stack.Item == Candidate;
	});
	const UknightSaveSubsystem* const Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	const bool bCanMutate = !bBusy && CanEditSocketProfile() && Saves && Saves->CanStartAtomicTransaction();
	if (Btn_Execute)
	{
		SetButtonLabel(Btn_Execute, Equipped ? LOCTEXT("SocketReplace", "소켓 교체") : LOCTEXT("SocketEquip", "소켓 장착"));
		Btn_Execute->SetVisibility(ESlateVisibility::Visible);
		SetButtonLocked(Btn_Execute, !bCanMutate || !bOwnedComplete || !bCandidateOwned
			|| !CanEquipSocketCandidate(SelectedSocketSlot, Candidate),
			LOCTEXT("SocketEquipUnavailable", "보유 수량과 소켓 칸을 확인하세요."));
	}
	if (Btn_Unequip)
	{
		Btn_Unequip->SetVisibility(ESlateVisibility::Visible);
		SetButtonLocked(Btn_Unequip, !bCanMutate || !Equipped,
			LOCTEXT("SocketSlotEmpty", "선택한 칸이 비어 있거나 변경할 수 없습니다."));
	}
}

void UKnightMaintenanceWidget::SelectEntry(UKnightMaintenanceEntryWidget* Entry)
{
	if (SelectedPart != (Entry ? Entry->GetPart() : nullptr)) UpgradeResultMessage = FText::GetEmpty();
	const bool bDifferentRecipe = Category == 0 && SelectedRecipe != (Entry ? Entry->GetRecipe() : nullptr);
	SelectedPart = Entry ? Entry->GetPart() : nullptr;
	SelectedRecipe = Entry ? Entry->GetRecipe() : nullptr;
	if (bDifferentRecipe) SetInfoTab(InfoMaterial);
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
	if (Category == 2) return false; // Fixed slot selection is never an equip request.
	if (Category == 3)
	{
		for (UCommonButtonBase* Button : SocketListButtons)
			if (Button && (Button->HasAnyUserFocus() || Button->HasFocusedDescendants())) return false;
	}
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
	if (bEnhancementPickerOpen) { CloseEnhancementPicker(true); return true; }
	if (bMaterialSaleConfirmOpen) { CloseMaterialSaleConfirmation(true); return true; }
	if (bEquipmentPickerOpen) { CloseEquipmentPicker(true); return true; }
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
	if (Category == 4)
	{
		OpenMaterialSaleConfirmation();
		return;
	}
	if (Category == 2)
	{
		if (bEquipmentPickerOpen) return;
		if (!bBusy && SelectedEquipmentSlot.IsValid() && ExecuteLockReason.IsEmpty()) OpenEquipmentPicker();
		else if (Txt_Status) Txt_Status->SetText(ExecuteLockReason);
		return;
	}
	if (Category == 3)
	{
		RequestSocketEquip();
		return;
	}
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
	if (StatCurrentTexts.Num() == RowCount && Stats_Rows->GetChildrenCount() == RowCount + 2) return;
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
	PreviewStatusText = NewRowText(WidgetTree, TextStyle, ETextJustify::Left);
	PreviewStatusText->SetAutoWrapText(true);
	PreviewStatusText->SetColorAndOpacity(FSlateColor(Alert));
	Stats_Rows->AddChildToVerticalBox(PreviewStatusText)->SetPadding(FMargin(0.f, 12.f, 12.f, 0.f));
	SelectedSetFooterText = NewRowText(WidgetTree, TextStyle, ETextJustify::Left);
	SelectedSetFooterText->SetAutoWrapText(true);
	Stats_Rows->AddChildToVerticalBox(SelectedSetFooterText)->SetPadding(FMargin(0.f, 18.f, 12.f, 6.f));
}

void UKnightMaintenanceWidget::RefreshEnhancementDetails()
{
	if (Category != 1) return;
	if (Switcher_Info && EnhancementDetailPage) Switcher_Info->SetActiveWidget(EnhancementDetailPage);
	if (Stats_Rows) Stats_Rows->SetVisibility(ESlateVisibility::Collapsed);
	if (Txt_SetInfo) Txt_SetInfo->SetVisibility(ESlateVisibility::Collapsed);
	if (Txt_ActiveSets) Txt_ActiveSets->SetVisibility(ESlateVisibility::Collapsed);
	if (Btn_Execute) Btn_Execute->SetVisibility(ESlateVisibility::Collapsed);
	if (Btn_Unequip) Btn_Unequip->SetVisibility(ESlateVisibility::Collapsed);
	bool bComplete = false;
	const UknightWeaponItemDefinition* const Weapon = GetEnhancementTarget(bComplete);
	FKnightWeaponUpgradeQuote Quote;
	if (Weapon && GetProgression()) GetProgression()->GetWeaponUpgradeQuote(Weapon, Quote);
	const bool bKnown = Weapon && Quote.WeaponId.IsValid()
		&& Quote.Availability != EKnightWeaponUpgradeAvailability::InvalidSettings
		&& Quote.Availability != EKnightWeaponUpgradeAvailability::NoSlot
		&& Quote.Availability != EKnightWeaponUpgradeAvailability::NotOwned;
	if (Txt_PartName) Txt_PartName->SetText(Weapon ? PartName(Weapon)
		: EnhancementTargetId.IsValid() ? LOCTEXT("EnhancementUnresolvedName", "무기 확인 불가")
		: LOCTEXT("EnhancementEmptyName", "강화 대상 없음"));
	if (Txt_PartState)
	{
		Txt_PartState->SetText(bKnown ? FText::Format(LOCTEXT("EnhancementLevel", "현재 +{0} / +15"), Quote.CurrentLevel)
			: FText::GetEmpty());
		Txt_PartState->SetVisibility(bKnown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Txt_PartDesc)
	{
		Txt_PartDesc->SetText(Weapon ? LOCTEXT("EnhancementIndividualNotice", "선택 무기 자체의 능력치입니다. 현재 캐릭터 전체 능력치와 다릅니다.")
			: LOCTEXT("EnhancementPickNotice", "왼쪽 강화 대상 슬롯에서 보유 무기를 선택하세요."));
		Txt_PartDesc->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (Img_Selected)
	{
		UTexture2D* const Icon = Weapon ? Weapon->Icon.LoadSynchronous() : nullptr;
		if (Icon) Img_Selected->SetBrushFromTexture(Icon);
		Img_Selected->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Txt_Bonus)
	{
		TArray<FText> Lines;
		if (bKnown)
		{
			Lines.Add(Quote.Availability == EKnightWeaponUpgradeAvailability::MaxLevel
				? FText::Format(LOCTEXT("EnhancementOwnAttackMax", "공격력 {0} · 최대 강화"), FText::AsNumber(Quote.CurrentWeaponAttack))
				: FText::Format(LOCTEXT("EnhancementOwnAttack", "공격력 {0}  성공 시 +{1}"),
					FText::AsNumber(Quote.CurrentWeaponAttack), FText::AsNumber(Quote.NextWeaponAttackIncrease)));
			for (const FknightPartStatModifier& Mod : Weapon->StatModifiers)
				if (Mod.Attribute != UknightCoreAttributeSet::GetAttackPowerAttribute() || Mod.ModifierOp != EGameplayModOp::AddBase)
					Lines.Add(ModifierText(Mod));
			if (Weapon->AbilitySet)
				Lines.Add(LOCTEXT("EnhancementConditionalEffects", "AbilitySet의 조건부 효과는 이 개별 수치에 포함되지 않습니다."));
		}
		else if (Weapon) Lines.Add(LOCTEXT("EnhancementOwnUnknown", "개별 능력치를 확인할 수 없습니다."));
		const FText Summary = FText::Join(FText::FromString(TEXT("\n")), Lines);
		Txt_Bonus->SetText(Summary);
		Txt_Bonus->SetVisibility(Summary.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	RefreshWeaponUpgradePanel();
}

void UKnightMaintenanceWidget::RefreshWeaponUpgradePanel()
{
	if (!UpgradePanel) return;
	UpgradePanel->SetVisibility(Category == 1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (Category != 1) return;
	bool bComplete = false;
	const UknightWeaponItemDefinition* const Weapon = GetEnhancementTarget(bComplete);
	UknightProgressionSubsystem* Progression = GetProgression();
	FKnightWeaponUpgradeQuote Quote;
	if (Progression && Weapon) Progression->GetWeaponUpgradeQuote(Weapon, Quote);
	const bool bQuoteKnown = Weapon && Quote.WeaponId.IsValid()
		&& Quote.Availability != EKnightWeaponUpgradeAvailability::InvalidSettings
		&& Quote.Availability != EKnightWeaponUpgradeAvailability::NoSlot
		&& Quote.Availability != EKnightWeaponUpgradeAvailability::NotOwned;
	if (UpgradeHeading) UpgradeHeading->SetText(!Weapon ? LOCTEXT("UpgradeChooseTarget", "강화 대상을 선택하세요.")
		: bQuoteKnown ? FText::Format(LOCTEXT("UpgradeHeading", "{0}  +{1} / +15"), PartName(Weapon), Quote.CurrentLevel)
		: LOCTEXT("UpgradeUnknownTarget", "강화 정보를 확인할 수 없습니다."));
	if (UpgradeAttack) UpgradeAttack->SetText(!bQuoteKnown ? FText::GetEmpty()
		: Quote.Availability == EKnightWeaponUpgradeAvailability::MaxLevel
			? FText::Format(LOCTEXT("UpgradeMaxAttack", "현재 무기 공격력 {0} · 최대 강화"), FText::AsNumber(Quote.CurrentWeaponAttack))
			: FText::Format(LOCTEXT("UpgradeAttack", "무기 공격력 {0} → 성공 시 {1} (+{2})"),
				FText::AsNumber(Quote.CurrentWeaponAttack), FText::AsNumber(Quote.NextWeaponAttack),
				FText::AsNumber(Quote.NextWeaponAttackIncrease)));
	if (UpgradeMoney) UpgradeMoney->SetText(!bQuoteKnown ? FText::GetEmpty()
		: Quote.Availability == EKnightWeaponUpgradeAvailability::MaxLevel
			? FText::Format(LOCTEXT("UpgradeOwnedMoney", "보유 돈 {0}"), FText::AsNumber(Progression->GetMoney()))
			: FText::Format(LOCTEXT("UpgradeMoney", "보유 돈 {0} / 필요 돈 {1}"),
				FText::AsNumber(Progression->GetMoney()), FText::AsNumber(Quote.MoneyCost)));
	if (UpgradeGate)
	{
		const bool bHasGate = bQuoteKnown && Quote.GateMaterial != nullptr;
		UpgradeGate->SetVisibility(bHasGate ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bHasGate) UpgradeGate->SetText(FText::Format(LOCTEXT("UpgradeGate", "성공 시에만 소모: {0}  보유 {1} / 필요 {2}"),
			Quote.GateMaterial->DisplayName, Quote.GateMaterialOwned, Quote.GateMaterialNeeded));
	}
	if (UpgradeChance) UpgradeChance->SetText(bQuoteKnown && Quote.TargetLevel > 0
		? FText::Format(LOCTEXT("UpgradeEffectiveChance", "이번 시도 성공률 {0}"), FText::AsPercent(Quote.EffectiveSuccessChance))
		: FText::GetEmpty());
	FText Reason;
	switch (Quote.Availability)
	{
	case EKnightWeaponUpgradeAvailability::Ready: break;
	case EKnightWeaponUpgradeAvailability::NoSlot: Reason = LOCTEXT("UpgradeNoSlot", "저장 슬롯을 선택하세요."); break;
	case EKnightWeaponUpgradeAvailability::NotOwned: Reason = LOCTEXT("UpgradeNotOwned", "보유하지 않은 무기입니다."); break;
	case EKnightWeaponUpgradeAvailability::MaxLevel: Reason = LOCTEXT("UpgradeMax", "최대 강화"); break;
	case EKnightWeaponUpgradeAvailability::InsufficientMoney: Reason = LOCTEXT("UpgradeNoMoney", "돈이 부족합니다."); break;
	case EKnightWeaponUpgradeAvailability::MissingMaterial: Reason = LOCTEXT("UpgradeNoGate", "관문 소재가 부족합니다."); break;
	case EKnightWeaponUpgradeAvailability::SaveBlocked: Reason = LOCTEXT("UpgradeSaveBlocked", "저장 처리 중입니다. 잠시 후 다시 시도하세요."); break;
	default: Reason = !Weapon && !EnhancementTargetId.IsValid()
		? LOCTEXT("UpgradePickFirst", "강화 대상 슬롯에서 무기를 선택하세요.")
		: LOCTEXT("UpgradeInvalid", "무기 정의 또는 강화 설정을 확인할 수 없습니다."); break;
	}
	if (UpgradeResult)
	{
		const FText Message = UpgradeResultMessage.IsEmpty() ? Reason
			: UpgradeSaveStatusMessage.IsEmpty() ? UpgradeResultMessage
			: FText::Format(LOCTEXT("UpgradeOutcomeAndSave", "{0}\n{1}"), UpgradeResultMessage, UpgradeSaveStatusMessage);
		UpgradeResult->SetText(Message);
		UpgradeResult->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	SetButtonLocked(UpgradeButton, bBusy || !bComplete || Quote.Availability != EKnightWeaponUpgradeAvailability::Ready, Reason);
}

void UKnightMaintenanceWidget::RequestWeaponUpgrade()
{
	if (bBusy || bEnhancementPickerOpen || Category != 1) return;
	UknightProgressionSubsystem* Progression = GetProgression();
	const UknightSaveSubsystem* const Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	bool bComplete = false;
	const UknightWeaponItemDefinition* const Weapon = GetEnhancementTarget(bComplete);
	FKnightWeaponUpgradeQuote Quote;
	if (Progression && Weapon) Progression->GetWeaponUpgradeQuote(Weapon, Quote);
	if (!Progression || !Saves || !Saves->CanStartAtomicTransaction() || !bComplete || !Weapon
		|| Weapon->GetPrimaryAssetId() != EnhancementTargetId
		|| Quote.Availability != EKnightWeaponUpgradeAvailability::Ready)
	{
		UpgradeResultMessage = LOCTEXT("UpgradeRejected", "강화 요청 불가: 대상, 보유량 또는 저장 상태를 확인하세요.");
		RefreshEnhancementDetails();
		return;
	}
	bBusy = true;
	FKnightWeaponUpgradeReceipt Receipt;
	const EKnightWeaponUpgradeOutcome Outcome = Progression->TryUpgradeWeapon(Weapon, Receipt);
	bBusy = false;
	if (Outcome != EKnightWeaponUpgradeOutcome::Rejected)
	{
		bAwaitingUpgradeSave = true;
		bUpgradeSaveFailed = false;
		UpgradeSaveStatusMessage = FText::GetEmpty();
		PendingUpgradeWeaponId = EnhancementTargetId;
		PendingUpgradeProfileId = Saves->GetActiveSlotId();
	}
	UpgradeResultMessage = Outcome == EKnightWeaponUpgradeOutcome::Success
		? FText::Format(LOCTEXT("UpgradeSucceeded", "강화 성공: +{0}"), Receipt.NewLevel)
		: Outcome == EKnightWeaponUpgradeOutcome::ChanceFailed
			? LOCTEXT("UpgradeChanceFailed", "확률 실패: 돈만 소모되고 단계와 관문 소재는 유지됩니다.")
			: LOCTEXT("UpgradeRejected", "강화 요청이 거절됐습니다. 조건을 확인하세요.");
	RefreshEnhancementDetails();
}

void UKnightMaintenanceWidget::RefreshMaterials(const UknightCraftRecipeDefinition* Recipe, UknightProgressionSubsystem* Progression)
{
	if (!Box_Materials || !WidgetTree) return;
	Box_Materials->ClearChildren();
	const UknightSaveSubsystem* const Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	const bool bCanReadOwned = Progression && Saves && Saves->HasSelectedSlot();
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
		if (!Cost.Item || !Cost.Item->GetPrimaryAssetId().IsValid() || Cost.Count <= 0)
		{
			AddLine(LOCTEXT("InvalidCost", "재료 정의를 확인할 수 없습니다."),
				LOCTEXT("CostUnavailable", "이용 불가"), Alert);
			continue;
		}
		const FText Name = Cost.Item->DisplayName.IsEmpty() ? FText::FromString(Cost.Item->GetName()) : Cost.Item->DisplayName;
		if (!bCanReadOwned)
		{
			AddLine(Name, FText::Format(LOCTEXT("OwnedUnavailable", "보유 확인 불가 / 필요 {0}"), Cost.Count), Alert);
			continue;
		}
		const int32 Have = Progression->GetItemCount(Cost.Item);
		AddLine(Name, FText::Format(LOCTEXT("MaterialCount", "보유 {0} / 필요 {1}"), Have, Cost.Count),
			Have >= Cost.Count ? White : Alert);
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
	if (Category == 3) { RefreshSocketDetails(); return; }
	if (Category == 2) { RefreshEquipmentDetails(); return; }
	if (Category == 1) { RefreshEnhancementDetails(); return; }
	const bool bCraftingSelectionMissing = Category == 0 && !SelectedRecipe;
	if (Stats_Rows) Stats_Rows->SetVisibility(bCraftingSelectionMissing ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (Txt_SetInfo) Txt_SetInfo->SetVisibility(ESlateVisibility::Visible);
	if (Txt_ActiveSets) Txt_ActiveSets->SetVisibility(ESlateVisibility::Visible);
	UknightProgressionSubsystem* Progression = GetProgression();
	const UknightMaintenanceCatalog* Catalog = UknightEquipmentSettings::LoadCatalog();
	const knightEquipmentStats::FLoadout Loadout = knightEquipmentStats::ReadLoadout(Progression, Catalog);
	const UknightPartItemDefinition* Equipped = Loadout.FindRef(Slot);
	// 제작에는 레시피를 선택하기 전 장착 장비 정보를 섞어 보여주지 않는다.
	const UknightPartItemDefinition* Shown = SelectedPart ? SelectedPart.Get() : Category == 0 ? nullptr : Equipped;
	const bool bShownEquipped = Shown && Shown == Equipped;

	const bool bWantPrediction = SelectedPart && !bShownEquipped;
	const FknightEquipmentComparison Comparison = bWantPrediction
		? knightEquipmentStats::CompareReplacement(Loadout, SelectedPart->SlotTag, SelectedPart, Catalog, Progression)
		: FknightEquipmentComparison();
	const FknightEquipmentStats Current = bWantPrediction ? Comparison.Current
		: knightEquipmentStats::Calculate(Loadout, Catalog, Progression);
	const FknightEquipmentStats& Predicted = Comparison.Replacement;
	const bool bHasPrediction = bWantPrediction && Comparison.bExact;

	if (Txt_PartName) Txt_PartName->SetText(Shown ? PartName(Shown) : Category == 0
		? LOCTEXT("PickCraftRecipe", "제작할 장비를 선택하세요.") : LOCTEXT("NothingEquipped", "장착된 장비 없음"));
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
	if (Txt_PartDesc)
	{
		Txt_PartDesc->SetText(Shown ? FText::GetEmpty() : LOCTEXT("PickPart", "왼쪽 목록에서 장비를 선택하세요."));
		Txt_PartDesc->SetVisibility(Shown ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
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
	if (PreviewStatusText)
	{
		FText Status;
		if (Category == 0 && !bCraftingSelectionMissing && !Current.bComplete)
			Status = FText::Format(LOCTEXT("CurrentCalculationIncomplete", "현재 스탯 계산 불가: {0}"), FText::FromString(Current.IncompleteReason));
		else if (Category == 0 && bWantPrediction && !Comparison.bExact)
			Status = FText::Format(LOCTEXT("ReplacementCalculationIncomplete", "교체 예상치 계산 불가: {0}"), FText::FromString(Comparison.IncompleteReason));
		PreviewStatusText->SetText(Status);
		PreviewStatusText->SetVisibility(Status.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (SelectedSetFooterText)
	{
		const FText SetDetails = Category == 0 && Shown ? SetInfoText(Shown, Catalog, Current,
			bHasPrediction ? &Predicted : nullptr) : FText::GetEmpty();
		SelectedSetFooterText->SetText(SetDetails.IsEmpty() ? FText::GetEmpty()
			: FText::Format(LOCTEXT("SelectedSetFooter", "선택 장비 세트\n{0}"), SetDetails));
		SelectedSetFooterText->SetVisibility(SetDetails.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	const TArray<FStatRow>& Rows = StatRows();
	for (int32 Index = 0; Index < Rows.Num() && Index < StatCurrentTexts.Num(); ++Index)
	{
		const FStatRow& Stat = Rows[Index];
		if (!Current.bComplete)
		{
			StatCurrentTexts[Index]->SetText(Category == 0 ? LOCTEXT("StatUnknown", "—") : FText::GetEmpty());
			StatArrowTexts[Index]->SetVisibility(ESlateVisibility::Hidden);
			StatAfterTexts[Index]->SetText(FText::GetEmpty());
			StatDeltaTexts[Index]->SetText(Category != 0 && Index == 0 ? FText::Format(LOCTEXT("StatsUnavailable", "계산 불가: {0}"), FText::FromString(Current.IncompleteReason)) : FText::GetEmpty());
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
	for (UCommonTextBlock* Text : StatArrowTexts)
		if (Text && Text->GetParent()) Text->GetParent()->SetVisibility(Category == 0 ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	for (UCommonTextBlock* Text : StatAfterTexts)
		if (Text && Text->GetParent()) Text->GetParent()->SetVisibility(Category == 0 ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	for (UCommonTextBlock* Text : StatDeltaTexts)
		if (Text && Text->GetParent()) Text->GetParent()->SetVisibility(ESlateVisibility::Visible);

	if (Txt_SetInfo)
	{
		FText Info = SetInfoText(Shown, Catalog, Current, bHasPrediction ? &Predicted : nullptr);
		if (SelectedPart && !bShownEquipped && !bHasPrediction)
			Info = FText::Format(LOCTEXT("SetPredictionFailed", "{0}\n장착 후 수치를 계산할 수 없습니다."), Info);
		Txt_SetInfo->SetText(Info);
	}
	if (Txt_ActiveSets) Txt_ActiveSets->SetText(ActiveSetsText(Catalog, Current));
	RefreshMaterials(Category == 0 ? SelectedRecipe.Get() : nullptr, Progression);
	RefreshWeaponUpgradePanel();

	// 비활성 대신 잠금: 흐리게 보이고, 누르면 이유 표시.
	if (Btn_Execute)
	{
		ExecuteLockReason = FText::GetEmpty();
		if (!SelectedPart) ExecuteLockReason = LOCTEXT("LockPick", "목록에서 장비를 선택하세요.");
		else if (Category == 0 && Progression && Progression->GetItemCount(SelectedPart) > 0)
			ExecuteLockReason = LOCTEXT("LockAlreadyOwned", "이미 보유한 장비입니다.");
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

void UKnightMaintenanceWidget::RefreshEquipmentDetails()
{
	if (Category != 2) return;
	if (Switcher_Info) Switcher_Info->SetActiveWidgetIndex(InfoSpec);
	if (Stats_Rows) Stats_Rows->SetVisibility(ESlateVisibility::Visible);
	EnsureStatRows();
	UknightProgressionSubsystem* const Progression = GetProgression();
	const UknightSaveSubsystem* const Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	const bool bCanMutate = Saves && Saves->CanStartAtomicTransaction();
	const UknightMaintenanceCatalog* const Catalog = UknightEquipmentSettings::LoadCatalog();
	bool bSlotsComplete = false;
	const UknightPartItemDefinition* const Part = SelectedEquipmentSlot.IsValid()
		? GetEquippedInSelectedSlot(bSlotsComplete) : nullptr;
	if (!SelectedEquipmentSlot.IsValid())
	{
		TArray<FKnightMainEquipmentSlot> Slots;
		bSlotsComplete = Progression && Progression->GetMainEquipmentSlots(Slots);
	}
	const FknightEquipmentStats Overall = knightEquipmentStats::Calculate(
		knightEquipmentStats::ReadLoadout(Progression, Catalog), Catalog, Progression);
	const FknightItemContribution Individual = Part
		? knightEquipmentStats::CalculateItemContribution(Part, Progression) : FknightItemContribution();
	const bool bIndividual = SelectedEquipmentSlot.IsValid();
	const bool bKnown = bIndividual ? (Part && Individual.bComplete) : (bSlotsComplete && Catalog && Overall.bComplete);
	if (Txt_PartName) Txt_PartName->SetText(!bIndividual ? LOCTEXT("EquipmentOverview", "현재 장비")
		: Part ? PartName(Part) : MainEquipmentSlotLabel(SelectedEquipmentSlot));
	if (Txt_PartState)
	{
		Txt_PartState->SetText(!bIndividual ? LOCTEXT("EquipmentFiveSlots", "무기 · 머리 · 몸 · 팔 · 다리")
			: Part ? LOCTEXT("EquipmentSelectedEquipped", "장착 중")
			: bSlotsComplete ? LOCTEXT("EquipmentSelectedEmpty", "빈 슬롯") : LOCTEXT("EquipmentSelectedUnknown", "장착 정보 확인 불가"));
		Txt_PartState->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (Txt_PartDesc)
	{
		Txt_PartDesc->SetText(!bIndividual ? LOCTEXT("EquipmentOverviewHint", "왼쪽 슬롯을 선택하면 해당 장비의 개별 기여를 볼 수 있습니다.")
			: Part ? LOCTEXT("EquipmentIndividualHint", "개별 장비 기여입니다. 세트 보너스는 포함하지 않습니다.")
			: bSlotsComplete ? LOCTEXT("EquipmentEmptyHint", "장착한 장비가 없습니다. 교체에서 보유 장비를 선택하세요.")
			: LOCTEXT("EquipmentUnknownHint", "저장 슬롯 또는 장비 정의를 확인하세요."));
		Txt_PartDesc->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (Img_Selected)
	{
		UTexture2D* const Icon = Part ? Part->Icon.LoadSynchronous() : nullptr;
		if (Icon) Img_Selected->SetBrushFromTexture(Icon);
		Img_Selected->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Txt_Bonus)
	{
		TArray<FText> Extra;
		if (bIndividual && bKnown)
			for (const TPair<FGameplayAttribute, float>& Entry : Individual.Values)
			{
				if (FindStatRow(Entry.Key) || FMath::IsNearlyZero(Entry.Value)) continue;
				const FExtraStat* const Info = FindExtraStat(Entry.Key);
				FText Number;
				if (Info && Info->bDecimal)
				{
					FNumberFormattingOptions Options;
					Options.AlwaysSign = true;
					Options.MaximumFractionalDigits = 1;
					Number = FText::AsNumber(Entry.Value, &Options);
				}
				else Number = FormatNumber(DisplayNumber(Entry.Value, IsPercentStat(Entry.Key)), IsPercentStat(Entry.Key), true);
				Extra.Add(FText::Format(LOCTEXT("EquipmentExtraStat", "{0} {1}"), StatLabel(Entry.Key), Number));
			}
		Txt_Bonus->SetText(FText::Join(FText::FromString(TEXT(" · ")), Extra));
		Txt_Bonus->SetVisibility(Extra.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (PreviewStatusText)
	{
		FText Warning;
		if (!bSlotsComplete) Warning = LOCTEXT("EquipmentIncompleteLoadout", "장착 정보가 불완전합니다. 일부 정의 또는 세이브 슬롯을 확인하세요.");
		else if (bIndividual && Part && !Individual.bComplete)
			Warning = FText::Format(LOCTEXT("EquipmentIncompleteItem", "개별 효과 계산 불가: {0}"), FText::FromString(Individual.IncompleteReason));
		else if (!bIndividual && !Overall.bComplete)
			Warning = FText::Format(LOCTEXT("EquipmentIncompleteOverall", "전체 스탯 계산 불가: {0}"), FText::FromString(Overall.IncompleteReason));
		PreviewStatusText->SetText(Warning);
		PreviewStatusText->SetVisibility(Warning.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	for (int32 Index = 0; Index < StatRows().Num() && Index < StatCurrentTexts.Num(); ++Index)
	{
		const FStatRow& Row = StatRows()[Index];
		const float Value = bIndividual ? Individual.Values.FindRef(Row.Attribute) : Overall.Values.FindRef(Row.Attribute);
		const int32 Rounded = DisplayNumber(Value, Row.bPercent);
		StatCurrentTexts[Index]->SetText(!bKnown || (bIndividual && Rounded == 0) ? LOCTEXT("EquipmentStatDash", "—")
			: FormatNumber(Rounded, Row.bPercent, bIndividual));
		StatLabelTexts[Index]->SetColorAndOpacity(FSlateColor(Mist));
		StatArrowTexts[Index]->SetVisibility(ESlateVisibility::Collapsed);
		StatAfterTexts[Index]->SetText(FText::GetEmpty());
		StatDeltaTexts[Index]->SetText(FText::GetEmpty());
	}
	for (UCommonTextBlock* Text : StatArrowTexts) if (Text && Text->GetParent()) Text->GetParent()->SetVisibility(ESlateVisibility::Collapsed);
	for (UCommonTextBlock* Text : StatAfterTexts) if (Text && Text->GetParent()) Text->GetParent()->SetVisibility(ESlateVisibility::Collapsed);
	for (UCommonTextBlock* Text : StatDeltaTexts) if (Text && Text->GetParent()) Text->GetParent()->SetVisibility(ESlateVisibility::Collapsed);
	if (UpgradePanel) UpgradePanel->SetVisibility(ESlateVisibility::Collapsed);
	if (SelectedSetFooterText)
	{
		const FText Sets = bSlotsComplete && Catalog && Overall.bComplete ? ActiveSetsText(Catalog, Overall) : FText::GetEmpty();
		SelectedSetFooterText->SetText(Sets.IsEmpty() ? FText::GetEmpty()
			: FText::Format(LOCTEXT("EquipmentActiveSets", "현재 활성 세트\n{0}"), Sets));
		SelectedSetFooterText->SetVisibility(Sets.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (Txt_ActiveSets) Txt_ActiveSets->SetText(bSlotsComplete && Catalog && Overall.bComplete ? ActiveSetsText(Catalog, Overall) : FText::GetEmpty());
	if (Btn_Execute)
	{
		SetButtonLabel(Btn_Execute, LOCTEXT("EquipmentReplace", "교체"));
		Btn_Execute->SetVisibility(bIndividual ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		TArray<const UknightPartItemDefinition*> Owned;
		const bool bOwnedComplete = bIndividual && GetCompatibleOwnedEquipment(SelectedEquipmentSlot, Owned);
		ExecuteLockReason = !bSlotsComplete || !bOwnedComplete ? LOCTEXT("EquipmentUnavailable", "장착 또는 보유 장비 정보가 불완전합니다.")
			: !bCanMutate ? LOCTEXT("EquipmentSaveBusy", "저장 처리 중입니다. 잠시 후 다시 시도하세요.")
			: Owned.IsEmpty() ? LOCTEXT("EquipmentNoOwned", "이 슬롯에 장착할 보유 장비가 없습니다.") : FText::GetEmpty();
		SetButtonLocked(Btn_Execute, bBusy || !ExecuteLockReason.IsEmpty(), ExecuteLockReason);
	}
	if (Btn_Unequip)
	{
		Btn_Unequip->SetVisibility(bIndividual ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		UnequipLockReason = !bSlotsComplete ? LOCTEXT("EquipmentUnequipUnknown", "장착 정보를 확인할 수 없습니다.")
			: !bCanMutate ? LOCTEXT("EquipmentUnequipSaveBusy", "저장 처리 중입니다. 잠시 후 다시 시도하세요.")
			: !Part ? LOCTEXT("EquipmentNothingToUnequip", "선택한 슬롯이 비어 있습니다.") : FText::GetEmpty();
		SetButtonLocked(Btn_Unequip, bBusy || !UnequipLockReason.IsEmpty(), UnequipLockReason);
	}
}

void UKnightMaintenanceWidget::ExecuteSelection()
{
	if (Category == 3)
	{
		RequestSocketEquip();
		return;
	}
	const UknightPartItemDefinition* const ActionPart = Category == 2 ? EquipmentPickerCandidate.Get() : SelectedPart.Get();
	if (bBusy || !ActionPart || (Category == 2 && (!bEquipmentPickerOpen || ActionPart->SlotTag != SelectedEquipmentSlot))) return;
	UknightProgressionSubsystem* Progression = GetProgression();
	if (!Progression) return;
	bBusy = true;
	bool bSuccess = false;
	if (Category == 0) bSuccess = SelectedRecipe && Progression->CraftPart(SelectedRecipe);
	else if (Progression->GetItemCount(ActionPart) > 0)
	{
		UknightPartsComponent* LiveParts = GetOwningPlayerPawn() ? GetOwningPlayerPawn()->FindComponentByClass<UknightPartsComponent>() : nullptr;
		bool bCanEquip = LiveParts && LiveParts->CanEquipPart(ActionPart);
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
				const UknightPartItemDefinition* Replaced = Loadout.FindRef(ActionPart->SlotTag);
				const float Capacity = Stats.Values.FindRef(UknightAttributeSet::GetLoadCapacityAttribute());
				bCanEquip = Stats.bComplete && knightPartsLoad::FitsLoadCapacity(
					TotalWeight, Replaced ? Replaced->Weight : 0.f, ActionPart->Weight, Capacity);
			}
		}
		if (bCanEquip)
		{
			const UknightPartItemDefinition* PreviousLivePart = LiveParts ? LiveParts->GetEquippedPart(ActionPart->SlotTag) : nullptr;
			bSuccess = !LiveParts || LiveParts->EquipPart(ActionPart, Progression->GetUpgradeLevel(ActionPart));
			if (bSuccess && !Progression->SetEquippedPart(ActionPart))
			{
				if (LiveParts)
				{
					if (PreviousLivePart) LiveParts->EquipPart(PreviousLivePart, Progression->GetUpgradeLevel(PreviousLivePart));
					else LiveParts->UnequipSlot(ActionPart->SlotTag);
				}
				bSuccess = false;
			}
		}
	}
	bBusy = false;
	if (Category == 2 && bSuccess) CloseEquipmentPicker(true);
	RebuildList();
	if (Txt_Status)
	{
		Txt_Status->SetText(!bSuccess ? LOCTEXT("ActionFailed", "요청 실패: 재료·소유 상태·적재 한도를 확인하세요.")
			: Category == 0 ? LOCTEXT("Crafted", "제작했습니다.") : LOCTEXT("Equipped", "장착했습니다."));
	}
}

void UKnightMaintenanceWidget::UnequipSelection()
{
	if (bEquipmentPickerOpen) return;
	if (Category == 3)
	{
		RequestSocketUnequip();
		return;
	}
	UknightProgressionSubsystem* Progression = GetProgression();
	const knightEquipmentStats::FLoadout Loadout = knightEquipmentStats::ReadLoadout(Progression, UknightEquipmentSettings::LoadCatalog());
	bool bSlotComplete = false;
	const UknightPartItemDefinition* Target = Category == 2 ? GetEquippedInSelectedSlot(bSlotComplete)
		: SelectedPart ? SelectedPart.Get() : Loadout.FindRef(Slot);
	const UknightSaveSubsystem* const Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	if (Category == 2 && (!SelectedEquipmentSlot.IsValid() || !bSlotComplete || !Saves || !Saves->CanStartAtomicTransaction())) return;
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
	RefreshCurrency();
	if (bBusy) return;
	RebuildList();
}

void UKnightMaintenanceWidget::HandleActiveSlotChanged(const FString& SlotId)
{
	SelectedSocketCandidateId = FPrimaryAssetId();
	bAwaitingSocketSave = false;
	bSocketSaveFailed = false;
	PendingSocketProfileId.Reset();
	CloseEnhancementPicker(false);
	EnhancementTargetId = FPrimaryAssetId();
	UpgradeResultMessage = FText::GetEmpty();
	UpgradeSaveStatusMessage = FText::GetEmpty();
	PendingUpgradeWeaponId = FPrimaryAssetId();
	PendingUpgradeProfileId.Reset();
	bAwaitingUpgradeSave = false;
	bUpgradeSaveFailed = false;
	CloseMaterialSaleConfirmation(false);
	SelectedMaterialId = FPrimaryAssetId();
	MaterialSaleQuantity = 0;
	MaterialSaleFeedback = FText::GetEmpty();
	bMaterialSaleSaveFailureShown = false;
	bAwaitingMaterialSaleSave = false;
	CloseEquipmentPicker(false);
	SelectedEquipmentSlot = FGameplayTag();
	RefreshCurrency();
	SelectedPart = nullptr;
	SelectedRecipe = nullptr;
	if (Category == 0) SetInfoTab(InfoMaterial);
	SelectedSocketItem = nullptr;
	SelectedSocketSlot = INDEX_NONE;
	UpgradeResultMessage = FText::GetEmpty();
	if (Txt_Status) Txt_Status->SetText(FText::GetEmpty());
	RebuildList(false);
	if (Category == 0 && Btn_Craft) Btn_Craft->SetFocus();
	else if (Category == 1 && EnhancementTargetButton) EnhancementTargetButton->SetFocus();
	else if (Category == 2 && Btn_Weapon) Btn_Weapon->SetFocus();
}

void UKnightMaintenanceWidget::HandleSaveFinished(bool bSuccess)
{
	if (bAwaitingSocketSave)
	{
		const UknightSaveSubsystem* const Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
		if (Saves && Saves->GetActiveSlotId() == PendingSocketProfileId && Txt_Status)
		{
			if (!bSuccess)
			{
				Txt_Status->SetText(LOCTEXT("SocketSaveFailed", "소켓 변경은 메모리에 반영됐지만 저장에 실패했습니다. 같은 상태의 저장을 재시도해야 합니다."));
				bSocketSaveFailed = true;
			}
			else if (bSocketSaveFailed) Txt_Status->SetText(LOCTEXT("SocketSaveRecovered", "소켓 변경 저장 완료"));
		}
		if (bSuccess)
		{
			bAwaitingSocketSave = false;
			bSocketSaveFailed = false;
			PendingSocketProfileId.Reset();
		}
	}
	if (bAwaitingUpgradeSave)
	{
		const UknightSaveSubsystem* const Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
		const bool bSameTargetAndProfile = Saves && Saves->GetActiveSlotId() == PendingUpgradeProfileId
			&& EnhancementTargetId == PendingUpgradeWeaponId;
		if (bSameTargetAndProfile)
		{
			if (bSuccess)
			{
				if (bUpgradeSaveFailed) UpgradeSaveStatusMessage = LOCTEXT("UpgradeSaveRecovered", "강화 결과 저장 완료");
				bUpgradeSaveFailed = false;
			}
			else
			{
				UpgradeSaveStatusMessage = LOCTEXT("UpgradeSaveFailed", "강화 결과가 메모리에 반영됐지만 저장에 실패했습니다. 같은 상태의 저장을 재시도해야 합니다.");
				bUpgradeSaveFailed = true;
			}
		}
		if (bSuccess)
		{
			bAwaitingUpgradeSave = false;
			PendingUpgradeWeaponId = FPrimaryAssetId();
			PendingUpgradeProfileId.Reset();
		}
	}
	if (bAwaitingMaterialSaleSave)
	{
		if (bSuccess)
		{
			if (bMaterialSaleSaveFailureShown) MaterialSaleFeedback = LOCTEXT("MaterialSaleSaveRecovered", "판매 저장 완료");
			bMaterialSaleSaveFailureShown = false;
			bAwaitingMaterialSaleSave = false;
		}
		else
		{
			MaterialSaleFeedback = LOCTEXT("MaterialSaleSaveFailed", "판매가 메모리에 반영됐지만 저장에 실패했습니다. 저장 재시도가 필요합니다.");
			bMaterialSaleSaveFailureShown = true;
		}
	}
	if (bBusy) return;
	if (Category == 3) { RefreshSocketDetails(); return; }
	if (Category == 1) { RefreshEnhancementDetails(); return; }
	if (Category == 4) { RefreshMaterialDetails(); return; }
	if (Category == 2)
	{
		RefreshEquipmentDetails();
		if (bEquipmentPickerOpen) RefreshEquipmentPickerPreview();
	}
}

#undef LOCTEXT_NAMESPACE
