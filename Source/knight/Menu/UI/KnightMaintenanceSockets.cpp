#include "Menu/UI/KnightMaintenanceWidget.h"
#include "Menu/UI/KnightMaintenancePresentation.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Item/knightItemDefinitions.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Progression/knightSaveSubsystem.h"

#define LOCTEXT_NAMESPACE "KnightSocketPresentation"

namespace
{
const FLinearColor Primary(FColor(0xED, 0xF3, 0xFA));
const FLinearColor Secondary(FColor(0xA0, 0xB8, 0xD0));

UCommonTextBlock* Text(UWidgetTree* Tree, FName Name, const FText& Value, int32 Pixels = 24,
	bool bCenter = false, FLinearColor Color = Primary)
{
	UCommonTextBlock* Label = Tree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), Name);
	Label->SetStyle(LoadClass<UCommonTextStyle>(nullptr, TEXT("/Game/Knight/UI/Style/TS_Knight_Body.TS_Knight_Body_C")));
	Label->SetStyle(nullptr); // Retain project font while keeping per-label color/size on synchronization.
	FSlateFontInfo Font = Label->GetFont(); Font.Size = FMath::RoundToInt(Pixels * .75f); Label->SetFont(Font);
	Label->SetText(Value); Label->SetAutoWrapText(true);
	Label->SetJustification(bCenter ? ETextJustify::Center : ETextJustify::Left);
	Label->SetColorAndOpacity(FSlateColor(Color)); Label->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Label;
}
UBorder* Frame(UWidgetTree* Tree, FName Name)
{
	UBorder* Result = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
	FSlateBrush Brush; Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.TintColor = FSlateColor(FLinearColor(.007f, .013f, .022f, 1.f));
	Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(0.f), FSlateColor(FLinearColor(.10f, .19f, .29f, 1.f)), 1.f);
	Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	Result->SetBrush(Brush); Result->SetPadding(FMargin(18.f));
	Result->SetVisibility(ESlateVisibility::SelfHitTestInvisible); return Result;
}
UVerticalBoxSlot* Add(UVerticalBox* Box, UWidget* Child, float Bottom = 12.f)
{
	auto* Cell = Box->AddChildToVerticalBox(Child); Cell->SetPadding(FMargin(0.f, 0.f, 0.f, Bottom)); return Cell;
}
void Line(UWidgetTree* Tree, UVerticalBox* Box)
{
	USizeBox* Size = Tree->ConstructWidget<USizeBox>(); Size->SetHeightOverride(1.f);
	UImage* Image = Tree->ConstructWidget<UImage>(); Image->SetColorAndOpacity(FLinearColor(.13f, .23f, .34f, 1.f));
	Image->SetVisibility(ESlateVisibility::HitTestInvisible); Size->AddChild(Image); Add(Box, Size, 14.f);
}
void Label(UCommonButtonBase* Button, const FText& Value)
{
	if (UFunction* Function = Button ? Button->FindFunction(TEXT("SetButtonText")) : nullptr)
	{
		struct { FText InText; } Params{ Value }; Button->ProcessEvent(Function, &Params);
	}
}
USizeBox* Artwork(UWidgetTree* Tree, UTexture2D* Texture, float Side)
{
	USizeBox* Size = Tree->ConstructWidget<USizeBox>(); Size->SetWidthOverride(Side); Size->SetHeightOverride(Side);
	UScaleBox* Fit = Tree->ConstructWidget<UScaleBox>(); Fit->SetStretch(EStretch::ScaleToFit);
	UImage* Image = Tree->ConstructWidget<UImage>(); Image->SetBrushFromTexture(Texture, true);
	Fit->AddChild(Image); Size->AddChild(Fit);
	Size->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); return Size;
}
UOverlay* ButtonContent(UWidgetTree* Tree, UCommonButtonBase* Button, UWidget* Content, const FText& AccessibleLabel)
{
	Label(Button, AccessibleLabel);
	if (UWidget* NativeLabel = Button->GetWidgetFromName(TEXT("Txt_Label"))) NativeLabel->SetVisibility(ESlateVisibility::Collapsed);
	UOverlay* Overlay = Tree->ConstructWidget<UOverlay>();
	UOverlaySlot* ButtonSlot = Overlay->AddChildToOverlay(Button);
	ButtonSlot->SetHorizontalAlignment(HAlign_Fill); ButtonSlot->SetVerticalAlignment(VAlign_Fill);
	Content->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* Cell = Overlay->AddChildToOverlay(Content);
	Cell->SetHorizontalAlignment(HAlign_Fill); Cell->SetVerticalAlignment(VAlign_Center); Cell->SetPadding(FMargin(12.f));
	return Overlay;
}
}

void UKnightMaintenanceWidget::BuildSocketPresentation()
{
	UCanvasPanel* Root = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
	if (!Root || SocketColumns) return;
	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SocketColumns"));
	SocketColumns = Columns;
	UCanvasPanelSlot* Layout = Root->AddChildToCanvas(Columns);
	Layout->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); Layout->SetOffsets(FMargin(48.f, 152.f, 48.f, 100.f)); Layout->SetZOrder(1);
	auto Column = [this, Columns](const TCHAR* Name, float Ratio, float Left, float Right)
	{
		UBorder* Border = Frame(WidgetTree, Name);
		UHorizontalBoxSlot* Cell = Columns->AddChildToHorizontalBox(Border);
		FSlateChildSize Size(ESlateSizeRule::Fill); Size.Value = Ratio; Cell->SetSize(Size); Cell->SetPadding(FMargin(Left, 0.f, Right, 0.f));
		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(); Border->SetContent(Box); return Box;
	};
	UVerticalBox* Left = Column(TEXT("SocketOwnedColumn"), .25f, 0.f, 6.f);
	UVerticalBox* Center = Column(TEXT("SocketAssignmentColumn"), .45f, 6.f, 6.f);
	UVerticalBox* Right = Column(TEXT("SocketEffectsColumn"), .30f, 6.f, 0.f);
	Add(Left, Text(WidgetTree, TEXT("SocketOwnedTitle"), LOCTEXT("Owned", "보유 소켓"), 28)); Line(WidgetTree, Left);
	SocketOwnedList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("SocketOwnedList"));
	Add(Left, SocketOwnedList)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	SocketListStatus = Text(WidgetTree, TEXT("SocketListStatus"), FText::GetEmpty(), 22, false, Secondary); Add(Left, SocketListStatus, 0.f);
	Add(Center, Text(WidgetTree, TEXT("SocketAssignmentTitle"), LOCTEXT("Assignments", "소켓 장착"), 28)); Line(WidgetTree, Center);
	UScrollBox* CenterScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("SocketAssignmentScroll"));
	Add(Center, CenterScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(); CenterScroll->AddChild(Body);
	UScaleBox* Fit = WidgetTree->ConstructWidget<UScaleBox>(); Fit->SetStretch(EStretch::ScaleToFit); Fit->SetStretchDirection(EStretchDirection::DownOnly);
	USizeBox* RowSize = WidgetTree->ConstructWidget<USizeBox>(); RowSize->SetWidthOverride(720.f); RowSize->SetHeightOverride(288.f);
	SocketSlotHost = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SocketSlotRow"));
	RowSize->AddChild(SocketSlotHost); Fit->AddChild(RowSize);
	// One uniformly scaled row retains three equal squares without a WrapBox or per-frame resizing.
	Add(Body, Fit, 28.f)->SetPadding(FMargin(0.f, 32.f, 0.f, 28.f));
	Line(WidgetTree, Body);
	SocketAssignmentSummary = Text(WidgetTree, TEXT("SocketAssignmentSummary"), FText::GetEmpty(), 26, true, Secondary);
	Add(Body, SocketAssignmentSummary, 20.f); Line(WidgetTree, Body);
	SocketCandidateText = Text(WidgetTree, TEXT("SocketCandidateText"), FText::GetEmpty(), 24, true);
	Add(Body, SocketCandidateText, 20.f);
	SocketFeedbackText = Text(WidgetTree, TEXT("SocketFeedback"), FText::GetEmpty(), 22, false, Secondary);
	Add(Body, SocketFeedbackText, 0.f);
	const TSubclassOf<UCommonButtonBase> ButtonClass = LoadClass<UCommonButtonBase>(nullptr, TEXT("/Game/Knight/UI/Common/WBP_KnightButton.WBP_KnightButton_C"));
	UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>();
	auto Action = [this, ButtonClass, Actions](const TCHAR* Name, const FText& Caption, bool bEquip)
	{
		UCommonButtonBase* Button = ButtonClass ? CreateWidget<UCommonButtonBase>(GetOwningPlayer(), ButtonClass, Name) : nullptr;
		if (!Button) return Button;
		Label(Button, Caption); KnightMaintenancePresentation::ConfigureButton(Button, false);
		Button->OnClicked().AddWeakLambda(this, [this, bEquip]() { if (bEquip) RequestSocketEquip(); else RequestSocketUnequip(); });
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(); Size->SetMinDesiredHeight(62.f); Size->AddChild(Button);
		UHorizontalBoxSlot* Cell = Actions->AddChildToHorizontalBox(Size); Cell->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Cell->SetPadding(bEquip ? FMargin(0.f, 0.f, 8.f, 0.f) : FMargin(8.f, 0.f, 0.f, 0.f)); return Button;
	};
	SocketEquipButton = Action(TEXT("SocketEquipAction"), LOCTEXT("Equip", "장착"), true);
	SocketUnequipButton = Action(TEXT("SocketUnequipAction"), LOCTEXT("Unequip", "해제"), false);
	Add(Center, Actions, 0.f);
	SocketDetailTitle = Text(WidgetTree, TEXT("SocketDetailTitle"), FText::GetEmpty(), 28);
	Add(Right, SocketDetailTitle); Line(WidgetTree, Right);
	UScrollBox* Details = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("SocketEffectsScroll"));
	Add(Right, Details)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UVerticalBox* DetailBody = WidgetTree->ConstructWidget<UVerticalBox>(); Details->AddChild(DetailBody);
	SocketDetailDescription = Text(WidgetTree, TEXT("SocketDetailDescription"), FText::GetEmpty(), 22, false, Secondary);
	Add(DetailBody, SocketDetailDescription, 20.f);
	SocketEffectsText = Text(WidgetTree, TEXT("SocketEffectsText"), FText::GetEmpty(), 26, false, Secondary); Add(DetailBody, SocketEffectsText);
	Add(Right, Text(WidgetTree, TEXT("SocketGuideTitle"), LOCTEXT("Guide", "소켓 안내"), 28)); Line(WidgetTree, Right);
	Add(Right, Text(WidgetTree, TEXT("SocketGuide"), LOCTEXT("GuideBody", "소켓 칸을 선택하면 현재 장착 정보를 확인합니다.\n\n같은 칸을 다시 누르면 전체 효과로 돌아갑니다.\n\n장착과 해제는 아이템 수량을 바꾸지 않습니다."), 22, false, Secondary), 0.f);
	UHorizontalBox* Hint = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SocketInputHints")); SocketInputHints = Hint;
	UBorder* Key = Frame(WidgetTree, TEXT("SocketEnterKey")); Key->SetPadding(FMargin(6.f, 2.f));
	UCommonTextBlock* KeyText = Text(WidgetTree, NAME_None, INVTEXT("Enter"), 20, false, Secondary); KeyText->SetAutoWrapText(false); Key->SetContent(KeyText);
	Hint->AddChildToHorizontalBox(Key);
	UCommonTextBlock* HintText = Text(WidgetTree, NAME_None, LOCTEXT("Input", "선택 / 실행"), 20, false, Secondary); HintText->SetAutoWrapText(false);
	UHorizontalBoxSlot* HintCell = Hint->AddChildToHorizontalBox(HintText); HintCell->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f)); HintCell->SetVerticalAlignment(VAlign_Center);
	UCanvasPanelSlot* HintSlot = Root->AddChildToCanvas(Hint); HintSlot->SetAnchors(FAnchors(0.f, 1.f)); HintSlot->SetAlignment(FVector2D(0.f, 1.f));
	HintSlot->SetOffsets(FMargin(48.f, -60.f, 0.f, 0.f)); HintSlot->SetAutoSize(true);
	Hint->SetVisibility(ESlateVisibility::Collapsed); Columns->SetVisibility(ESlateVisibility::Collapsed);
}

void UKnightMaintenanceWidget::RebuildSocketList()
{
	if (!SocketOwnedList || !SocketSlotHost || !WidgetTree) return;
	int32 FocusedSlot = INDEX_NONE; FPrimaryAssetId FocusedCandidate;
	for (int32 Index = 0; Index < SocketSlotButtons.Num(); ++Index)
		if (SocketSlotButtons[Index] && (SocketSlotButtons[Index]->HasAnyUserFocus() || SocketSlotButtons[Index]->HasFocusedDescendants())) FocusedSlot = Index;
	for (int32 Index = 0; Index < SocketCandidateButtons.Num(); ++Index)
		if (SocketCandidateButtons[Index] && (SocketCandidateButtons[Index]->HasAnyUserFocus() || SocketCandidateButtons[Index]->HasFocusedDescendants())) FocusedCandidate = SocketCandidateIds[Index];
	const float Scroll = SocketOwnedList->GetScrollOffset();
	SocketOwnedList->ClearChildren(); SocketSlotHost->ClearChildren();
	Entries.Reset(); // Retire hidden Crafting rows from the shared confirm-key route.
	SocketListButtons.Reset(); SocketSlotButtons.Reset(); SocketCandidateButtons.Reset(); SocketCandidateIds.Reset();
	UknightProgressionSubsystem* Progression = GetProgression();
	const auto* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	const FString Profile = Saves ? Saves->GetActiveSlotId() : FString();
	const int32 Unlocked = Progression ? Progression->GetUnlockedSharedSocketSlots() : 0;
	TArray<FKnightSocketAssignment> Assignments;
	if (Progression) Progression->GetSocketAssignments(Assignments);
	if (SocketSelectionProfileId != Profile)
	{
		SelectedSocketSlot = INDEX_NONE; SelectedSocketCandidateId = FPrimaryAssetId(); SelectedSocketItem = nullptr;
		SocketSelectionProfileId.Reset(); SocketFeedback = FText::GetEmpty();
	}
	if (SelectedSocketSlot >= FMath::Min(Unlocked, 3) || SelectedSocketSlot < INDEX_NONE) SelectedSocketSlot = INDEX_NONE;
	const auto ButtonClass = LoadClass<UCommonButtonBase>(nullptr, TEXT("/Game/Knight/UI/Common/WBP_KnightButton.WBP_KnightButton_C"));
	if (!ButtonClass || !GetOwningPlayer()) return;
	auto Button = [this, ButtonClass](FName Name, bool bSelected)
	{
		auto* Result = CreateWidget<UCommonButtonBase>(GetOwningPlayer(), ButtonClass, Name);
		KnightMaintenancePresentation::ConfigureButton(Result, true);
		if (bSelected) Result->SetIsSelected(true, false); else Result->ClearSelection();
		SocketListButtons.Add(Result); return Result;
	};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const bool bUnlocked = Index < Unlocked;
		const FKnightSocketAssignment* Assignment = Assignments.IsValidIndex(Index) ? &Assignments[Index] : nullptr;
		const auto* Item = Assignment ? Assignment->Item.Get() : nullptr;
		const FText State = !bUnlocked ? LOCTEXT("Locked", "잠김") : Item ? Item->DisplayName
			: Assignment && Assignment->ItemId.IsValid() ? LOCTEXT("Unknown", "장착 정보 확인 불가") : LOCTEXT("Empty", "빈 슬롯");
		UCommonButtonBase* SlotButton = Button(FName(*FString::Printf(TEXT("Phase8_SocketSlot%d"), Index)), SelectedSocketSlot == Index);
		SlotButton->SetIsLocked(!bUnlocked);
		SlotButton->OnClicked().AddWeakLambda(this, [this, Index, Profile]()
		{
			const auto* Current = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
			if (Current && Current->GetActiveSlotId() == Profile) SelectSocketSlot(Index);
		});
		UVerticalBox* Cell = WidgetTree->ConstructWidget<UVerticalBox>();
		Add(Cell, Text(WidgetTree, NAME_None, FText::Format(LOCTEXT("Position", "소켓 {0}"), Index + 1), 24, true, Secondary), 16.f);
		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
		Add(Content, Artwork(WidgetTree, Item ? Item->Icon.LoadSynchronous() : nullptr, 100.f), 8.f)->SetHorizontalAlignment(HAlign_Center);
		Add(Content, Text(WidgetTree, NAME_None, State, 22, true, bUnlocked ? Primary : Secondary), 0.f);
		USizeBox* Square = WidgetTree->ConstructWidget<USizeBox>(); Square->SetWidthOverride(220.f); Square->SetHeightOverride(220.f);
		Square->AddChild(ButtonContent(WidgetTree, SlotButton, Content, State)); Add(Cell, Square, 0.f);
		SocketSlotHost->AddChildToHorizontalBox(Cell)->SetPadding(FMargin(10.f, 0.f)); SocketSlotButtons.Add(SlotButton);
	}
	TArray<FknightItemStack> Owned;
	const bool bOwnedComplete = Progression && Progression->GetOwnedSockets(Owned);
	Owned.Sort([](const FknightItemStack& A, const FknightItemStack& B) { return A.Item && B.Item ? A.Item->DisplayName.ToString() < B.Item->DisplayName.ToString() : A.Item != nullptr; });
	SelectedSocketItem = nullptr;
	for (const FknightItemStack& Stack : Owned)
	{
		const auto* Item = Cast<UknightSocketItemDefinition>(Stack.Item);
		if (!Item || Stack.Count <= 0) continue;
		const FPrimaryAssetId Id = Item->GetPrimaryAssetId();
		if (Id == SelectedSocketCandidateId) SelectedSocketItem = Item;
		UCommonButtonBase* Row = Button(FName(*FString::Printf(TEXT("Phase8_SocketCandidate%d"), SocketCandidateButtons.Num())), Id == SelectedSocketCandidateId);
		Row->OnClicked().AddWeakLambda(this, [this, Item, Profile]()
		{
			const auto* Current = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
			if (Current && Current->GetActiveSlotId() == Profile) SelectSocketCandidate(Item);
		});
		UHorizontalBox* Content = WidgetTree->ConstructWidget<UHorizontalBox>();
		Content->AddChildToHorizontalBox(Artwork(WidgetTree, Item->Icon.LoadSynchronous(), 68.f))->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		UVerticalBox* Labels = WidgetTree->ConstructWidget<UVerticalBox>();
		Add(Labels, Text(WidgetTree, NAME_None, Item->DisplayName, 24), 8.f);
		Add(Labels, Text(WidgetTree, NAME_None, FText::Format(LOCTEXT("Quantity", "보유 {0} · 사용 가능 {1}"), Stack.Count, Progression->GetAvailableSocketItemCount(Item)), 20, false, Secondary), 0.f);
		UHorizontalBoxSlot* LabelsSlot = Content->AddChildToHorizontalBox(Labels); LabelsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); LabelsSlot->SetVerticalAlignment(VAlign_Center);
		USizeBox* RowSize = WidgetTree->ConstructWidget<USizeBox>(); RowSize->SetMinDesiredHeight(104.f);
		RowSize->AddChild(ButtonContent(WidgetTree, Row, Content, Item->DisplayName));
		Cast<UScrollBoxSlot>(SocketOwnedList->AddChild(RowSize))->SetPadding(FMargin(0.f, 0.f, 8.f, 10.f));
		SocketCandidateButtons.Add(Row); SocketCandidateIds.Add(Id);
	}
	if (!SelectedSocketItem) SelectedSocketCandidateId = FPrimaryAssetId();
	SocketListStatus->SetText(!bOwnedComplete ? LOCTEXT("Incomplete", "일부 보유 소켓 정의를 확인할 수 없습니다.")
		: Owned.IsEmpty() ? LOCTEXT("NoOwned", "보유 소켓이 없습니다.") : LOCTEXT("Choose", "장착할 소켓을 선택하세요."));
	SocketOwnedList->SetScrollOffset(Scroll);
	RefreshCurrency(); RefreshSocketDetails();
	if (SocketSlotButtons.IsValidIndex(FocusedSlot)) SocketSlotButtons[FocusedSlot]->SetFocus();
	else for (int32 Index = 0; Index < SocketCandidateIds.Num(); ++Index)
		if (SocketCandidateIds[Index] == FocusedCandidate) { SocketCandidateButtons[Index]->SetFocus(); break; }
}

#undef LOCTEXT_NAMESPACE
