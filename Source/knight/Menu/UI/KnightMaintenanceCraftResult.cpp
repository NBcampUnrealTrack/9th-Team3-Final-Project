#include "Menu/UI/KnightMaintenanceWidget.h"
#include "Menu/UI/KnightMaintenancePresentation.h"
#include "Menu/UI/KnightMaintenanceEntryWidget.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Item/knightEquipmentData.h"
#include "Item/knightEquipmentStats.h"
#include "Item/knightPartsComponent.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Progression/knightSaveSubsystem.h"
#include "System/knightGameplayTags.h"

#define LOCTEXT_NAMESPACE "KnightCraftResult"

void UKnightMaintenanceWidget::BuildCraftResultDialog()
{
	UCanvasPanel* Root = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
	if (!Root || CraftResultPanel) return;
	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("CraftResultPanel"));
	CraftResultPanel = Overlay;
	UCanvasPanelSlot* OverlaySlot = Root->AddChildToCanvas(Overlay);
	OverlaySlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	OverlaySlot->SetOffsets(FMargin(0.f));
	OverlaySlot->SetZOrder(1000);
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>();
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, .68f));
	Overlay->AddChildToOverlay(Dim);
	USizeBox* Frame = WidgetTree->ConstructWidget<USizeBox>();
	Frame->SetWidthOverride(460.f);
	UOverlaySlot* FrameSlot = Overlay->AddChildToOverlay(Frame);
	FrameSlot->SetHorizontalAlignment(HAlign_Center); FrameSlot->SetVerticalAlignment(VAlign_Center);
	UBorder* Surface = WidgetTree->ConstructWidget<UBorder>();
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Box;
	Brush.SetResourceObject(CraftingButtonTexture);
	Brush.ImageSize = FVector2D(512.f, 170.f);
	Brush.Margin = KnightMaintenancePresentation::ControlSlice;
	KnightMaintenancePresentation::SetControlUV(Brush);
	Brush.TintColor = FSlateColor(KnightMaintenancePresentation::ReadoutTint);
	Surface->SetBrush(Brush); Surface->SetPadding(FMargin(28.f)); Frame->AddChild(Surface);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(); Surface->SetContent(Column);
	auto AddText = [this, Column](FName Name, int32 Size, FLinearColor Color)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Txt_PartName ? Txt_PartName->GetFont() : Text->GetFont(); Font.Size = Size;
		Text->SetFont(Font); Text->SetColorAndOpacity(Color); Text->SetAutoWrapText(true);
		Text->SetJustification(ETextJustify::Center);
		Column->AddChildToVerticalBox(Text)->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
		return Text;
	};
	AddText(TEXT("CraftResultTitle"), 20, KnightMaintenancePresentation::WarmHighlight)->SetText(LOCTEXT("Title", "제작 완료"));
	USizeBox* IconFrame = WidgetTree->ConstructWidget<USizeBox>();
	IconFrame->SetWidthOverride(88.f); IconFrame->SetHeightOverride(88.f);
	Column->AddChildToVerticalBox(IconFrame)->SetHorizontalAlignment(HAlign_Center);
	UScaleBox* Fit = WidgetTree->ConstructWidget<UScaleBox>(); Fit->SetStretch(EStretch::ScaleToFit); IconFrame->AddChild(Fit);
	CraftResultIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CraftResultIcon")); Fit->AddChild(CraftResultIcon);
	CraftResultName = AddText(TEXT("CraftResultName"), 22, KnightMaintenancePresentation::Primary);
	CraftResultError = AddText(TEXT("CraftResultError"), 16, KnightMaintenancePresentation::WarmHighlight);
	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(Buttons)->SetHorizontalAlignment(HAlign_Center);
	const TSubclassOf<UCommonButtonBase> ButtonClass = Btn_ConfirmOk ? Btn_ConfirmOk->GetClass() : nullptr;
	auto AddButton = [this, Buttons, ButtonClass](FName Name, const FText& Label)
	{
		UCommonButtonBase* Button = GetOwningPlayer() && ButtonClass
			? CreateWidget<UCommonButtonBase>(GetOwningPlayer(), ButtonClass, Name) : nullptr;
		if (!Button) return Button;
		if (UFunction* Function = Button->FindFunction(TEXT("SetButtonText")))
		{
			struct { FText Text; } Args{ Label }; Button->ProcessEvent(Function, &Args);
		}
		KnightMaintenancePresentation::ConfigureButton(Button, false);
		if (CraftingControlStyle) Button->SetStyle(CraftingControlStyle);
		USizeBox* ButtonFrame = WidgetTree->ConstructWidget<USizeBox>();
		ButtonFrame->SetWidthOverride(174.f); ButtonFrame->SetHeightOverride(46.f); ButtonFrame->AddChild(Button);
		Buttons->AddChildToHorizontalBox(ButtonFrame)->SetPadding(FMargin(4.f, 4.f));
		return Button;
	};
	CraftResultEquip = AddButton(TEXT("CraftResultEquip"), LOCTEXT("Equip", "장착"));
	CraftResultBack = AddButton(TEXT("CraftResultBack"), LOCTEXT("Back", "뒤로가기"));
	if (CraftResultEquip && CraftResultBack)
	{
		CraftResultEquip->OnClicked().AddUObject(this, &ThisClass::EquipCraftResult);
		CraftResultBack->OnClicked().AddWeakLambda(this, [this]() { if (!bBusy) CloseCraftResult(true); });
		for (UCommonButtonBase* Button : { CraftResultEquip.Get(), CraftResultBack.Get() })
		{
			UCommonButtonBase* Other = Button == CraftResultEquip ? CraftResultBack.Get() : CraftResultEquip.Get();
			for (EUINavigation Direction : { EUINavigation::Left, EUINavigation::Right, EUINavigation::Next, EUINavigation::Previous })
				Button->SetNavigationRuleExplicit(Direction, Other);
			Button->SetNavigationRuleBase(EUINavigation::Up, EUINavigationRule::Stop);
			Button->SetNavigationRuleBase(EUINavigation::Down, EUINavigationRule::Stop);
		}
	}
	Overlay->SetVisibility(ESlateVisibility::Collapsed);
}

void UKnightMaintenanceWidget::OpenCraftResult(const UknightPartItemDefinition* Part)
{
	const auto* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	if (!Part || !Saves || !Saves->HasSelectedSlot() || IsCraftResultOpen() || !CraftResultPanel
		|| !CraftResultEquip || !CraftResultBack) return;
	const FGameplayTag PartSlot = Part->SlotTag;
	if (PartSlot != knightGameplayTags::Equipment_Slot_Weapon && PartSlot != knightGameplayTags::Equipment_Slot_Head
		&& PartSlot != knightGameplayTags::Equipment_Slot_Torso && PartSlot != knightGameplayTags::Equipment_Slot_Arms
		&& PartSlot != knightGameplayTags::Equipment_Slot_Legs) return;
	// Equipment is unique by primary asset ID in the current save model, not an instance/GUID inventory.
	CraftedPart = Part; CraftedProfileId = Saves->GetActiveSlotId();
	CraftResultName->SetText(Part->DisplayName);
	CraftResultIcon->SetBrushFromTexture(Part->Icon.LoadSynchronous(), true);
	CraftResultError->SetText(FText::GetEmpty()); CraftResultError->SetVisibility(ESlateVisibility::Collapsed);
	if (UPanelWidget* Root = Cast<UPanelWidget>(WidgetTree->RootWidget))
		for (UWidget* Child : Root->GetAllChildren())
			if (Child != CraftResultPanel)
			{
				CraftResultBlockedWidgets.Add(Child, Child->GetIsEnabled()); Child->SetIsEnabled(false);
			}
	CraftResultPanel->SetVisibility(ESlateVisibility::Visible);
	// Repeated confirmation input must not accidentally equip the newly crafted item.
	CraftResultBack->SetFocus();
}

void UKnightMaintenanceWidget::CloseCraftResult(bool bRestoreFocus)
{
	const bool bWasOpen = IsCraftResultOpen();
	CraftedPart = nullptr; CraftedProfileId.Reset();
	if (CraftResultPanel) CraftResultPanel->SetVisibility(ESlateVisibility::Collapsed);
	for (const auto& Entry : CraftResultBlockedWidgets)
		if (UWidget* Widget = Entry.Key.Get()) Widget->SetIsEnabled(Entry.Value);
	CraftResultBlockedWidgets.Reset();
	if (!bWasOpen || !bRestoreFocus) return;
	for (UKnightMaintenanceEntryWidget* Entry : Entries)
		if (Entry && Entry->GetRecipe() == SelectedRecipe && Entry->GetPart() == SelectedPart) { Entry->SetFocus(); return; }
	if (Btn_Craft) Btn_Craft->SetFocus();
}

FReply UKnightMaintenanceWidget::HandleCraftResultKey(const FKeyEvent& Event)
{
	if (!IsCraftResultOpen()) return FReply::Unhandled();
	if (Event.IsRepeat() || bBusy) return FReply::Handled();
	const FKey Key = Event.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right) CloseCraftResult(true);
	else if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		if (CraftResultEquip && (CraftResultEquip->HasAnyUserFocus() || CraftResultEquip->HasFocusedDescendants())) EquipCraftResult();
		else CloseCraftResult(true);
	}
	else if (Key == EKeys::Tab || Key == EKeys::Left || Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_DPad_Right)
	{
		if (CraftResultEquip && CraftResultBack)
			(CraftResultEquip->HasAnyUserFocus() || CraftResultEquip->HasFocusedDescendants() ? CraftResultBack : CraftResultEquip)->SetFocus();
	}
	return FReply::Handled(); // Modal owns all keys; no crafting, tab or inventory shortcuts underneath.
}

FReply UKnightMaintenanceWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	return IsCraftResultOpen() ? HandleCraftResultKey(KeyEvent) : Super::NativeOnPreviewKeyDown(Geometry, KeyEvent);
}

void UKnightMaintenanceWidget::RefreshCraftingAfterTransaction()
{
	if (Category != 0) return;
	UknightProgressionSubsystem* Progression = GetProgression();
	if (!Progression) return;
	const auto Loadout = knightEquipmentStats::ReadLoadout(Progression, UknightEquipmentSettings::LoadCatalog());
	// Update existing rows in place: preserve selected recipe, focus, scroll and list geometry.
	for (UKnightMaintenanceEntryWidget* Entry : Entries)
		if (Entry && Entry->GetPart())
		{
			const auto* Part = Entry->GetPart(); const auto* Recipe = Entry->GetRecipe();
			const int32 Owned = Progression->GetItemCount(Part);
			const auto State = Loadout.FindRef(Part->SlotTag) == Part ? EKnightMaintenanceEntryState::Equipped
				: Owned > 0 ? EKnightMaintenanceEntryState::Owned
				: Progression->CanCraftPart(Recipe) ? EKnightMaintenanceEntryState::Craftable : EKnightMaintenanceEntryState::MissingMaterials;
			Entry->SetEntry(Part, Recipe, State, Owned);
			StyleCraftingEntry(Entry);
		}
	RefreshCurrency(); RefreshDetails();
}

bool UKnightMaintenanceWidget::TryEquipOwnedPart(const UknightPartItemDefinition* Part, FText& FailureReason)
{
	UknightProgressionSubsystem* Progression = GetProgression();
	if (!Progression || !Part || !Part->SlotTag.IsValid() || Progression->GetItemCount(Part) <= 0)
	{
		FailureReason = LOCTEXT("NotOwned", "장착할 아이템의 보유 정보를 확인할 수 없습니다."); return false;
	}
	UknightPartsComponent* LiveParts = GetOwningPlayerPawn() ? GetOwningPlayerPawn()->FindComponentByClass<UknightPartsComponent>() : nullptr;
	bool bCanEquip = LiveParts && LiveParts->CanEquipPart(Part);
	if (!LiveParts)
	{
		const auto* Catalog = UknightEquipmentSettings::LoadCatalog();
		const auto Loadout = knightEquipmentStats::ReadLoadout(Progression, Catalog);
		const auto Stats = knightEquipmentStats::Calculate(Loadout, Catalog, Progression);
		if (!Catalog || !Stats.bComplete)
		{
			FailureReason = LOCTEXT("Incomplete", "장비 또는 적재 한도 정보를 확인할 수 없습니다."); return false;
		}
		float TotalWeight = 0.f;
		for (const auto& Entry : Loadout) if (Entry.Value) TotalWeight += Entry.Value->Weight;
		const auto* Replaced = Loadout.FindRef(Part->SlotTag);
		bCanEquip = knightPartsLoad::FitsLoadCapacity(TotalWeight, Replaced ? Replaced->Weight : 0.f,
			Part->Weight, Stats.Values.FindRef(UknightAttributeSet::GetLoadCapacityAttribute()));
	}
	if (!bCanEquip)
	{
		FailureReason = LOCTEXT("Capacity", "적재 한도를 초과하여 장착할 수 없습니다."); return false;
	}
	const auto* Previous = LiveParts ? LiveParts->GetEquippedPart(Part->SlotTag) : nullptr;
	if (LiveParts && !LiveParts->EquipPart(Part, Progression->GetUpgradeLevel(Part)))
	{
		FailureReason = LOCTEXT("LiveRejected", "현재 캐릭터에 장착할 수 없습니다."); return false;
	}
	if (!Progression->SetEquippedPart(Part))
	{
		if (LiveParts)
		{
			if (Previous) LiveParts->EquipPart(Previous, Progression->GetUpgradeLevel(Previous));
			else LiveParts->UnequipSlot(Part->SlotTag);
		}
		FailureReason = LOCTEXT("EquipRejected", "장착 변경을 적용하지 못했습니다. 아이템은 보관함에 유지됩니다."); return false;
	}
	return true;
}

void UKnightMaintenanceWidget::EquipCraftResult()
{
	if (!IsCraftResultOpen() || bBusy || Category != 0) return;
	const auto* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	FText Error;
	if (!Saves || !Saves->HasSelectedSlot() || Saves->GetActiveSlotId() != CraftedProfileId)
		Error = LOCTEXT("ProfileChanged", "제작한 저장 슬롯이 아닙니다. 장착할 수 없습니다.");
	else if (!Saves->CanStartAtomicTransaction())
		Error = LOCTEXT("SaveBusy", "저장 완료 후 다시 장착해 주세요.");
	else
	{
		bBusy = true;
		const bool bEquipped = TryEquipOwnedPart(CraftedPart, Error);
		bBusy = false;
		if (bEquipped)
		{
			RefreshCraftingAfterTransaction(); CloseCraftResult(true);
			if (Txt_Status) Txt_Status->SetText(LOCTEXT("Equipped", "장착했습니다."));
			return;
		}
	}
	CraftResultError->SetText(Error); CraftResultError->SetVisibility(ESlateVisibility::HitTestInvisible);
}

#undef LOCTEXT_NAMESPACE
