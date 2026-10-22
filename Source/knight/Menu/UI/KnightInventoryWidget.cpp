#include "Menu/UI/KnightInventoryWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Item/knightEquipmentData.h"
#include "Item/knightEquipmentStats.h"
#include "Item/knightItemDefinitions.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Progression/knightSaveSubsystem.h"
#include "System/knightGameplayTags.h"
#include "knightInventoryTypes.h"

#define LOCTEXT_NAMESPACE "KnightInventory"

namespace
{
	const FLinearColor PanelColor(FColor(15, 25, 35, 242));
	const FLinearColor RowColor(FColor(35, 52, 67, 230));
	const FLinearColor InventorySelectedColor(FColor(176, 123, 55, 240));
	const FLinearColor InventoryWhite(FColor(242, 244, 246));

	UCommonTextBlock* MakeText(UWidgetTree* Tree, const FText& Value, const TCHAR* Style)
	{
		UCommonTextBlock* Text = Tree->ConstructWidget<UCommonTextBlock>();
		Text->SetText(Value);
		if (const TSubclassOf<UCommonTextStyle> StyleClass = LoadClass<UCommonTextStyle>(nullptr,
			*FString::Printf(TEXT("/Game/Knight/UI/Style/%s.%s_C"), Style, Style)))
		{
			Text->SetStyle(StyleClass);
		}
		return Text;
	}

	UButton* MakeTab(UWidgetTree* Tree, UHorizontalBox* Parent, const FText& Label)
	{
		UButton* Button = Tree->ConstructWidget<UButton>();
		Button->SetBackgroundColor(RowColor);
		Button->SetContent(MakeText(Tree, Label, TEXT("TS_Knight_Button")));
		UHorizontalBoxSlot* Slot = Parent->AddChildToHorizontalBox(Button);
		Slot->SetPadding(FMargin(0, 0, 10, 0));
		Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		return Button;
	}

	FText ItemName(const UknightItemDefinition* Item)
	{
		return Item && !Item->DisplayName.IsEmpty() ? Item->DisplayName
			: Item ? FText::FromString(Item->GetName()) : FText::GetEmpty();
	}
}

TSharedRef<SWidget> UKnightInventoryEntryWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ItemButton"));
		Button->SetBackgroundColor(RowColor);
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		NameText = MakeText(WidgetTree, FText::GetEmpty(), TEXT("TS_Knight_Body"));
		CountText = MakeText(WidgetTree, FText::GetEmpty(), TEXT("TS_Knight_Caption"));
		EquippedText = MakeText(WidgetTree, FText::GetEmpty(), TEXT("TS_Knight_Caption"));
		Row->AddChildToHorizontalBox(NameText)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Row->AddChildToHorizontalBox(CountText)->SetPadding(FMargin(10, 0, 10, 0));
		Row->AddChildToHorizontalBox(EquippedText);
		Button->SetContent(Row);
		WidgetTree->RootWidget = Button;
	}
	return Super::RebuildWidget();
}

void UKnightInventoryEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (Button) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
	ApplyConfiguration();
}

void UKnightInventoryEntryWidget::Configure(const UknightItemDefinition* InItem, int32 Count, bool bEquipped,
	const FLinearColor& NameColor)
{
	Item = InItem;
	DisplayCount = Count;
	bDisplayEquipped = bEquipped;
	DisplayNameColor = NameColor;
	ApplyConfiguration();
}

void UKnightInventoryEntryWidget::ApplyConfiguration()
{
	if (NameText)
	{
		NameText->SetText(ItemName(Item));
		NameText->SetColorAndOpacity(FSlateColor(DisplayNameColor));
	}
	if (CountText) CountText->SetText(FText::Format(LOCTEXT("Count", "×{0}"), FText::AsNumber(DisplayCount)));
	if (EquippedText) EquippedText->SetText(bDisplayEquipped ? LOCTEXT("Equipped", "장착 중") : FText::GetEmpty());
	if (Button) Button->SetBackgroundColor(bSelected ? InventorySelectedColor : RowColor);
}

void UKnightInventoryEntryWidget::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	if (Button) Button->SetBackgroundColor(bInSelected ? InventorySelectedColor : RowColor);
}

void UKnightInventoryEntryWidget::HandleClicked()
{
	if (Item) OnSelected.Broadcast(Item);
}

TSharedRef<SWidget> UKnightInventoryWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("InventoryRoot"));
		WidgetTree->RootWidget = Root;
		UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
		Backdrop->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.04f, 0.88f));
		UCanvasPanelSlot* BackdropSlot = Root->AddChildToCanvas(Backdrop);
		BackdropSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		BackdropSlot->SetOffsets(FMargin(0));

		UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
		Card->SetBrushColor(PanelColor);
		Card->SetPadding(FMargin(26, 22));
		UCanvasPanelSlot* CardSlot = Root->AddChildToCanvas(Card);
		CardSlot->SetAnchors(FAnchors(0.055f, 0.065f, 0.945f, 0.935f));
		CardSlot->SetOffsets(FMargin(0));

		UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
		Card->SetContent(Layout);
		UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
		Layout->AddChildToVerticalBox(Header)->SetPadding(FMargin(0, 0, 0, 18));
		Header->AddChildToHorizontalBox(MakeText(WidgetTree, LOCTEXT("Title", "인벤토리"), TEXT("TS_Knight_Subhead")))
			->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ProfileText = MakeText(WidgetTree, FText::GetEmpty(), TEXT("TS_Knight_Caption"));
		Header->AddChildToHorizontalBox(ProfileText);
		MoneyText = MakeText(WidgetTree, FText::GetEmpty(), TEXT("TS_Knight_Caption"));
		Header->AddChildToHorizontalBox(MoneyText)->SetPadding(FMargin(18, 0, 0, 0));

		UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>();
		Layout->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(0, 0, 0, 16));
		EquipmentButton = MakeTab(WidgetTree, Tabs, LOCTEXT("Equipment", "장비"));
		MaterialButton = MakeTab(WidgetTree, Tabs, LOCTEXT("Materials", "소재"));
		SocketButton = MakeTab(WidgetTree, Tabs, LOCTEXT("Sockets", "소켓 아이템"));

		UHorizontalBox* Content = WidgetTree->ConstructWidget<UHorizontalBox>();
		Layout->AddChildToVerticalBox(Content)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		USizeBox* ListWidth = WidgetTree->ConstructWidget<USizeBox>();
		ListWidth->SetWidthOverride(390.f);
		ItemList = WidgetTree->ConstructWidget<UScrollBox>();
		ListWidth->SetContent(ItemList);
		Content->AddChildToHorizontalBox(ListWidth)->SetPadding(FMargin(0, 0, 20, 0));

		UBorder* Details = WidgetTree->ConstructWidget<UBorder>();
		Details->SetBrushColor(FLinearColor(0.07f, 0.11f, 0.15f, 0.9f));
		Details->SetPadding(FMargin(22));
		Content->AddChildToHorizontalBox(Details)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UScrollBox* DetailScroll = WidgetTree->ConstructWidget<UScrollBox>();
		Details->SetContent(DetailScroll);
		UVerticalBox* DetailBody = WidgetTree->ConstructWidget<UVerticalBox>();
		DetailScroll->AddChild(DetailBody);
		DetailName = MakeText(WidgetTree, FText::GetEmpty(), TEXT("TS_Knight_Subhead"));
		DetailBody->AddChildToVerticalBox(DetailName)->SetPadding(FMargin(0, 0, 0, 18));
		DetailDescription = MakeText(WidgetTree, FText::GetEmpty(), TEXT("TS_Knight_Body"));
		DetailDescription->SetAutoWrapText(true);
		DetailBody->AddChildToVerticalBox(DetailDescription);

		SalePanel = WidgetTree->ConstructWidget<UVerticalBox>();
		DetailBody->AddChildToVerticalBox(SalePanel)->SetPadding(FMargin(0, 22, 0, 0));
		SaleSelectionText = MakeText(WidgetTree, FText::GetEmpty(), TEXT("TS_Knight_Body"));
		SalePanel->AddChildToVerticalBox(SaleSelectionText)->SetPadding(FMargin(0, 0, 0, 8));
		UnitPriceText = MakeText(WidgetTree, FText::GetEmpty(), TEXT("TS_Knight_Body"));
		SalePanel->AddChildToVerticalBox(UnitPriceText)->SetPadding(FMargin(0, 0, 0, 8));
		UHorizontalBox* QuantityRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		SalePanel->AddChildToVerticalBox(QuantityRow)->SetPadding(FMargin(0, 0, 0, 8));
		QuantityRow->AddChildToHorizontalBox(MakeText(WidgetTree,
			LOCTEXT("SaleQuantity", "판매 수량"), TEXT("TS_Knight_Body")))->SetPadding(FMargin(0, 4, 12, 0));
		auto MakeQuantityButton = [this, QuantityRow](const FText& Label)
		{
			UButton* Button = WidgetTree->ConstructWidget<UButton>();
			Button->SetBackgroundColor(RowColor);
			Button->SetContent(MakeText(WidgetTree, Label, TEXT("TS_Knight_Button")));
			QuantityRow->AddChildToHorizontalBox(Button)->SetPadding(FMargin(0, 0, 8, 0));
			return Button;
		};
		DecreaseButton = MakeQuantityButton(LOCTEXT("SaleMinus", "−"));
		USizeBox* QuantityWidth = WidgetTree->ConstructWidget<USizeBox>();
		QuantityWidth->SetWidthOverride(80.f);
		SaleQuantityInput = WidgetTree->ConstructWidget<UEditableTextBox>();
		SaleQuantityInput->SetText(FText::AsNumber(1));
		SaleQuantityInput->SetHintText(LOCTEXT("QuantityHint", "수량"));
		QuantityWidth->SetContent(SaleQuantityInput);
		QuantityRow->AddChildToHorizontalBox(QuantityWidth)->SetPadding(FMargin(0, 0, 8, 0));
		IncreaseButton = MakeQuantityButton(LOCTEXT("SalePlus", "+"));
		MaxButton = MakeQuantityButton(LOCTEXT("SaleMax", "최대"));
		TotalPriceText = MakeText(WidgetTree, FText::GetEmpty(), TEXT("TS_Knight_Body"));
		SalePanel->AddChildToVerticalBox(TotalPriceText)->SetPadding(FMargin(0, 0, 0, 12));
		SellButton = WidgetTree->ConstructWidget<UButton>();
		SellButton->SetBackgroundColor(InventorySelectedColor);
		SellButton->SetContent(MakeText(WidgetTree, LOCTEXT("Sell", "판매"), TEXT("TS_Knight_Button")));
		SalePanel->AddChildToVerticalBox(SellButton)->SetPadding(FMargin(0, 0, 0, 8));
		SaleFeedbackText = MakeText(WidgetTree, FText::GetEmpty(), TEXT("TS_Knight_Caption"));
		SalePanel->AddChildToVerticalBox(SaleFeedbackText);
		SalePanel->SetVisibility(ESlateVisibility::Collapsed);

		Layout->AddChildToVerticalBox(MakeText(WidgetTree,
			LOCTEXT("Hint", "I: 열기/닫기   ·   Esc: 닫기"), TEXT("TS_Knight_Caption")))
			->SetPadding(FMargin(0, 16, 0, 0));
	}
	return Super::RebuildWidget();
}

void UKnightInventoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UknightProgressionSubsystem* Progression = GI->GetSubsystem<UknightProgressionSubsystem>())
			Progression->OnProgressionChanged.AddUniqueDynamic(this, &ThisClass::HandleProgressionChanged);
		if (UknightSaveSubsystem* Saves = GI->GetSubsystem<UknightSaveSubsystem>())
		{
			Saves->OnActiveSlotChanged.AddUniqueDynamic(this, &ThisClass::HandleActiveSlotChanged);
			Saves->OnSaveFinished.AddUniqueDynamic(this, &ThisClass::HandleSaveFinished);
		}
	}
}

void UKnightInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (EquipmentButton) EquipmentButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowEquipment);
	if (MaterialButton) MaterialButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowMaterials);
	if (SocketButton) SocketButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowSockets);
	if (DecreaseButton) DecreaseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::DecreaseSaleQuantity);
	if (IncreaseButton) IncreaseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::IncreaseSaleQuantity);
	if (MaxButton) MaxButton->OnClicked.AddUniqueDynamic(this, &ThisClass::MaxSaleQuantity);
	if (SellButton) SellButton->OnClicked.AddUniqueDynamic(this, &ThisClass::SellSelectedMaterial);
	if (SaleQuantityInput) SaleQuantityInput->OnTextChanged.AddUniqueDynamic(this, &ThisClass::HandleQuantityChanged);
}

void UKnightInventoryWidget::NativeDestruct()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UknightProgressionSubsystem* Progression = GI->GetSubsystem<UknightProgressionSubsystem>())
			Progression->OnProgressionChanged.RemoveDynamic(this, &ThisClass::HandleProgressionChanged);
		if (UknightSaveSubsystem* Saves = GI->GetSubsystem<UknightSaveSubsystem>())
		{
			Saves->OnActiveSlotChanged.RemoveDynamic(this, &ThisClass::HandleActiveSlotChanged);
			Saves->OnSaveFinished.RemoveDynamic(this, &ThisClass::HandleSaveFinished);
		}
	}
	Entries.Reset();
	Super::NativeDestruct();
}

void UKnightInventoryWidget::Open() { SetCategory(0); }
void UKnightInventoryWidget::Close() { SelectedItemId = FPrimaryAssetId(); Entries.Reset(); SaleFeedback = FText::GetEmpty(); bSaleSaveFailureShown = false; if (ItemList) ItemList->ClearChildren(); }
UButton* UKnightInventoryWidget::GetFocusButton() const { return EquipmentButton; }
void UKnightInventoryWidget::ShowEquipment() { SetCategory(0); }
void UKnightInventoryWidget::ShowMaterials() { SetCategory(1); }
void UKnightInventoryWidget::ShowSockets() { SetCategory(2); }
void UKnightInventoryWidget::SetCategory(int32 NewCategory)
{
	Category = NewCategory;
	SelectedItemId = FPrimaryAssetId();
	SaleFeedback = FText::GetEmpty();
	bSaleSaveFailureShown = false;
	SetSaleQuantity(1);
	Refresh();
	if (EquipmentButton) EquipmentButton->SetBackgroundColor(Category == 0 ? InventorySelectedColor : RowColor);
	if (MaterialButton) MaterialButton->SetBackgroundColor(Category == 1 ? InventorySelectedColor : RowColor);
	if (SocketButton) SocketButton->SetBackgroundColor(Category == 2 ? InventorySelectedColor : RowColor);
}

UknightProgressionSubsystem* UKnightInventoryWidget::GetProgression() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>() : nullptr;
}

void UKnightInventoryWidget::HandleProgressionChanged() { Refresh(); }
void UKnightInventoryWidget::HandleSaveFinished(bool bSuccess)
{
	if (!bSuccess)
	{
		SaleFeedback = LOCTEXT("SaleSaveFailed", "저장 실패: 저장을 다시 시도해야 합니다.");
		bSaleSaveFailureShown = true;
	}
	else if (bSaleSaveFailureShown)
	{
		SaleFeedback = LOCTEXT("SaleSaveRecovered", "저장 완료");
		bSaleSaveFailureShown = false;
	}
	RefreshSale();
}
void UKnightInventoryWidget::HandleActiveSlotChanged(const FString& SlotId)
{
	SelectedItemId = FPrimaryAssetId();
	SaleFeedback = FText::GetEmpty();
	bSaleSaveFailureShown = false;
	SetSaleQuantity(1);
	Refresh();
}

void UKnightInventoryWidget::Refresh()
{
	if (!ItemList) return;
	ItemList->ClearChildren();
	Entries.Reset();
	UGameInstance* GI = GetGameInstance();
	const UknightSaveSubsystem* Saves = GI ? GI->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	UknightProgressionSubsystem* Progression = GetProgression();
	const bool bHasSlot = Saves && Saves->HasSelectedSlot();
	if (ProfileText)
	{
		const int32 FixedSlotNumber = bHasSlot ? Saves->GetActiveFixedSlotNumber() : 0;
		ProfileText->SetText(FixedSlotNumber > 0 ? FText::Format(LOCTEXT("Slot", "저장 {0}"), FText::AsNumber(FixedSlotNumber))
			: bHasSlot ? LOCTEXT("TestSlot", "테스트 저장") : LOCTEXT("NoSlot", "선택한 저장 없음"));
	}
	if (MoneyText) MoneyText->SetText(bHasSlot && Progression
		? FText::Format(LOCTEXT("Money", "보유 돈: {0}"), FText::AsNumber(Progression->GetMoney()))
		: FText::GetEmpty());
	TArray<FknightItemStack> Owned;
	if (bHasSlot && Progression) Progression->GetOwnedItems(Owned);
	TMap<FPrimaryAssetId, const UknightPartItemDefinition*> Equipped;
	if (bHasSlot && Progression)
	{
		const knightEquipmentStats::FLoadout Loadout = knightEquipmentStats::ReadLoadout(Progression, UknightEquipmentSettings::LoadCatalog());
		for (const auto& Pair : Loadout)
		{
			if (Pair.Value) Equipped.Add(Pair.Value->GetPrimaryAssetId(), Pair.Value);
		}
		if (Category == 0)
		{
			for (const auto& Pair : Equipped)
			{
				bool bInOwned = false;
				for (const FknightItemStack& Stack : Owned) if (Stack.Item == Pair.Value && Stack.Count > 0) { bInOwned = true; break; }
				if (!bInOwned) Owned.Emplace(Pair.Value, 1); // Implicit starter gear, exactly as Maintenance shows it.
			}
		}
	}
	Owned.RemoveAll([](const FknightItemStack& Stack) { return !Stack.Item || Stack.Count <= 0; });
	Owned.Sort([](const FknightItemStack& A, const FknightItemStack& B)
	{
		return ItemName(A.Item).ToString() < ItemName(B.Item).ToString();
	});
	for (const FknightItemStack& Stack : Owned)
	{
		const UknightItemDefinition* Item = Stack.Item;
		const bool bEquipment = Item->IsA<UknightPartItemDefinition>();
		const bool bMaterial = Item->IsA<UknightMaterialItemDefinition>() || Item->ItemType == knightGameplayTags::Item_Type_Material;
		const bool bSocket = Item->IsA<UknightSocketItemDefinition>() || Item->ItemType == knightGameplayTags::Item_Type_Socket;
		if (!(Category == 0 && bEquipment || Category == 1 && bMaterial || Category == 2 && bSocket)) continue;
		FLinearColor NameColor = InventoryWhite;
		if (Category == 1)
		{
			const UknightMaterialItemDefinition* Material = Cast<UknightMaterialItemDefinition>(Item);
			NameColor = Material && Material->bHardModeExclusive ? FLinearColor(FColor(225, 93, 93))
				: Item->Rarity > 1 ? FLinearColor(FColor(97, 166, 239)) : InventoryWhite;
		}
		UKnightInventoryEntryWidget* Entry = CreateWidget<UKnightInventoryEntryWidget>(GetOwningPlayer(), UKnightInventoryEntryWidget::StaticClass());
		if (!Entry) continue;
		bool bEquipped = Equipped.Contains(Item->GetPrimaryAssetId());
		if (Category == 2 && Progression)
		{
			for (int32 SocketIndex = 0; SocketIndex < Progression->GetUnlockedSharedSocketSlots(); ++SocketIndex)
			{
				const UknightSocketItemDefinition* const Socket = Progression->GetEquippedSocketItem(SocketIndex);
				if (Socket && Socket->GetPrimaryAssetId() == Item->GetPrimaryAssetId()) { bEquipped = true; break; }
			}
		}
		Entry->Configure(Item, Stack.Count, bEquipped, NameColor);
		Entry->OnSelected.AddUObject(this, &ThisClass::SelectItem);
		ItemList->AddChild(Entry);
		Entries.Add(Entry);
		Entry->SetSelected(Item->GetPrimaryAssetId() == SelectedItemId);
	}
	if (Entries.IsEmpty())
	{
		SelectedItemId = FPrimaryAssetId();
		ItemList->AddChild(MakeText(WidgetTree, bHasSlot ? LOCTEXT("Empty", "보유한 항목이 없습니다.")
			: LOCTEXT("ChooseSlot", "저장 슬롯을 선택해 주세요."), TEXT("TS_Knight_BodyDim")));
	}
	else if (SelectedItemId.IsValid())
	{
		bool bStillOwned = false;
		for (const UKnightInventoryEntryWidget* Entry : Entries)
			if (Entry && Entry->GetItem() && Entry->GetItem()->GetPrimaryAssetId() == SelectedItemId) { bStillOwned = true; break; }
		if (!bStillOwned)
		{
			SelectedItemId = FPrimaryAssetId();
			if (Category == 1 && MaterialButton && GetOwningPlayer()) MaterialButton->SetUserFocus(GetOwningPlayer());
		}
	}
	RefreshDetail();
}

void UKnightInventoryWidget::SelectItem(const UknightItemDefinition* Item)
{
	SelectedItemId = Item ? Item->GetPrimaryAssetId() : FPrimaryAssetId();
	SaleFeedback = FText::GetEmpty();
	SetSaleQuantity(1);
	for (UKnightInventoryEntryWidget* Entry : Entries)
		if (Entry) Entry->SetSelected(Entry->GetItem() == Item);
	RefreshDetail();
}

void UKnightInventoryWidget::RefreshDetail()
{
	const UknightItemDefinition* Item = nullptr;
	for (const UKnightInventoryEntryWidget* Entry : Entries)
		if (Entry && Entry->GetItem() && Entry->GetItem()->GetPrimaryAssetId() == SelectedItemId)
		{ Item = Entry->GetItem(); break; }
	if (DetailName) DetailName->SetText(Item ? ItemName(Item) : LOCTEXT("Select", "항목 선택"));
	if (DetailDescription)
	{
		FText Description = Category != 0 && Item && !Item->Description.IsEmpty() ? Item->Description
			: Item ? LOCTEXT("NoDescription", "설명이 없습니다.") : FText::GetEmpty();
		if (Category == 2)
		{
			const UknightSocketItemDefinition* const Socket = Cast<UknightSocketItemDefinition>(Item);
			if (Socket)
			{
				const UknightProgressionSubsystem* const Progression = GetProgression();
				const int32 Available = Progression ? Progression->GetAvailableSocketItemCount(Socket) : 0;
				Description = FText::Format(LOCTEXT("SocketInventoryDetail", "{0}\n장착 가능한 남은 수량: {1}"), Description, Available);
			}
		}
		DetailDescription->SetText(Category == 0 ? FText::GetEmpty() : Description);
		DetailDescription->SetVisibility(Category == 0 ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	RefreshSale();
}

void UKnightInventoryWidget::SetSaleQuantity(int32 Quantity)
{
	SaleQuantity = Quantity;
	if (SaleQuantityInput) SaleQuantityInput->SetText(FText::AsNumber(Quantity));
	RefreshSale();
}

void UKnightInventoryWidget::HandleQuantityChanged(const FText& Text)
{
	const FString Raw = Text.ToString().TrimStartAndEnd();
	int32 Parsed = 0;
	SaleQuantity = LexTryParseString(Parsed, *Raw) ? Parsed : 0;
	RefreshSale();
}

void UKnightInventoryWidget::DecreaseSaleQuantity()
{
	SetSaleQuantity(FMath::Max(1, SaleQuantity - 1));
}

void UKnightInventoryWidget::IncreaseSaleQuantity()
{
	const UknightProgressionSubsystem* const Progression = GetProgression();
	const UknightMaterialItemDefinition* Material = nullptr;
	for (const UKnightInventoryEntryWidget* Entry : Entries)
		if (Entry && Entry->GetItem() && Entry->GetItem()->GetPrimaryAssetId() == SelectedItemId)
		{ Material = Cast<UknightMaterialItemDefinition>(Entry->GetItem()); break; }
	const int32 Owned = Progression && Material ? Progression->GetItemCount(Material) : 0;
	if (Owned > 0) SetSaleQuantity(SaleQuantity < 1 ? 1 : SaleQuantity >= Owned ? Owned : SaleQuantity + 1);
}

void UKnightInventoryWidget::MaxSaleQuantity()
{
	const UknightProgressionSubsystem* const Progression = GetProgression();
	for (const UKnightInventoryEntryWidget* Entry : Entries)
	{
		if (Entry && Entry->GetItem() && Entry->GetItem()->GetPrimaryAssetId() == SelectedItemId)
		{
			if (const UknightMaterialItemDefinition* const Material = Cast<UknightMaterialItemDefinition>(Entry->GetItem()))
				SetSaleQuantity(Progression ? Progression->GetItemCount(Material) : 0);
			return;
		}
	}
}

void UKnightInventoryWidget::RefreshSale()
{
	if (!SalePanel) return;
	SalePanel->SetVisibility(Category == 1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (Category != 1) return;
	const UknightProgressionSubsystem* const Progression = GetProgression();
	const UknightMaterialItemDefinition* Material = nullptr;
	for (const UKnightInventoryEntryWidget* Entry : Entries)
		if (Entry && Entry->GetItem() && Entry->GetItem()->GetPrimaryAssetId() == SelectedItemId)
		{ Material = Cast<UknightMaterialItemDefinition>(Entry->GetItem()); break; }
	const int32 Owned = Progression && Material ? Progression->GetItemCount(Material) : 0;
	if (SaleSelectionText) SaleSelectionText->SetText(Material
		? FText::Format(LOCTEXT("SaleSelected", "{0}  ·  보유 {1}개"), ItemName(Material), FText::AsNumber(Owned))
		: LOCTEXT("SaleChoose", "판매할 소재를 선택하세요."));
	if (UnitPriceText) UnitPriceText->SetText(Material && Material->bSellable && Material->SellPrice > 0
		? FText::Format(LOCTEXT("SaleUnitPrice", "개당 판매 가격: {0}"), FText::AsNumber(Material->SellPrice))
		: LOCTEXT("SaleUnavailable", "판매 불가"));
	FKnightMaterialSaleReceipt Quote;
	const bool bValid = !bSaleRequestInProgress && Progression && Material
		&& Progression->GetMaterialSaleQuote(SelectedItemId, SaleQuantity, Quote);
	if (TotalPriceText) TotalPriceText->SetText(bValid
		? FText::Format(LOCTEXT("SaleTotal", "총 판매 금액: {0}"), FText::AsNumber(Quote.TotalMoney))
		: LOCTEXT("SaleTotalInvalid", "총 판매 금액: —"));
	if (SellButton) SellButton->SetIsEnabled(bValid);
	if (DecreaseButton) DecreaseButton->SetIsEnabled(Owned > 0);
	if (IncreaseButton) IncreaseButton->SetIsEnabled(Owned > 0);
	if (MaxButton) MaxButton->SetIsEnabled(Owned > 0);
	if (SaleFeedbackText) SaleFeedbackText->SetText(SaleFeedback);
}

void UKnightInventoryWidget::SellSelectedMaterial()
{
	if (bSaleRequestInProgress || Category != 1 || !SelectedItemId.IsValid()) return;
	UknightProgressionSubsystem* const Progression = GetProgression();
	if (!Progression) return;
	bSaleRequestInProgress = true;
	FKnightMaterialSaleReceipt Receipt;
	const bool bSold = Progression->SellMaterial(SelectedItemId, SaleQuantity, Receipt);
	bSaleRequestInProgress = false;
	SaleFeedback = bSold
		? FText::Format(LOCTEXT("SaleSuccess", "{0}개 판매 완료 · +{1}"),
			FText::AsNumber(Receipt.Count), FText::AsNumber(Receipt.TotalMoney))
		: LOCTEXT("SaleRejected", "판매할 수 없습니다. 수량과 저장 상태를 확인하세요.");
	if (bSold) bSaleSaveFailureShown = false;
	if (bSold) SetSaleQuantity(1);
	RefreshDetail();
}

#undef LOCTEXT_NAMESPACE
