#include "KnightCodexEditorLibrary.h"

#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "WidgetBlueprint.h"

namespace
{
	const FLinearColor Navy(FColor(0x10, 0x1A, 0x2A));
	const FLinearColor Blue(FColor(0x1C, 0x30, 0x47));
	const FLinearColor Frost(FColor(0xA8, 0xC8, 0xF0));
	const FLinearColor CodexFrame(FColor(0x0D, 0x15, 0x1D));
	const FLinearColor CodexSidebar(FColor(0x21, 0x2D, 0x36));
	const FLinearColor CodexReadingPanel(FColor(0x18, 0x25, 0x30));
	bool Begin(UWidgetBlueprint* BP)
	{
		if (!BP || !BP->WidgetTree || BP->WidgetTree->RootWidget) return false;
		BP->Modify(); BP->WidgetTree->Modify(); return true;
	}
	void End(UWidgetBlueprint* BP) { FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); }
	template<class T> T* New(UWidgetBlueprint* BP, const TCHAR* Name, bool Variable = false)
	{
		T* W = BP->WidgetTree->ConstructWidget<T>(T::StaticClass(), Name);
		W->bIsVariable = Variable;
		BP->OnVariableAdded(Name);
		return W;
	}
	TSubclassOf<UCommonTextStyle> Style(const TCHAR* Name)
	{
		return LoadClass<UCommonTextStyle>(nullptr, *FString::Printf(TEXT("/Game/Knight/UI/Style/%s.%s_C"), Name, Name));
	}
	UCommonTextBlock* Text(UWidgetBlueprint* BP, const TCHAR* Name, const FText& Value, const TCHAR* StyleName, bool Variable = false)
	{
		UCommonTextBlock* W = New<UCommonTextBlock>(BP, Name, Variable);
		W->SetText(Value);
		W->SetAutoWrapText(true);
		if (TSubclassOf<UCommonTextStyle> S = Style(StyleName)) W->SetStyle(S);
		return W;
	}
	UButton* Button(UWidgetBlueprint* BP, const TCHAR* Name, const FText& Label)
	{
		UButton* W = New<UButton>(BP, Name, true);
		W->SetBackgroundColor(Blue);
		W->AddChild(Text(BP, *FString::Printf(TEXT("Label_%s"), Name), Label, TEXT("TS_Knight_Button")));
		return W;
	}
	void Row(UHorizontalBox* Parent, UWidget* Child, bool Fill, const FMargin& Padding = FMargin())
	{
		UHorizontalBoxSlot* Slot = Parent->AddChildToHorizontalBox(Child);
		Slot->SetSize(FSlateChildSize(Fill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
		Slot->SetVerticalAlignment(VAlign_Fill);
		Slot->SetPadding(Padding);
	}
	void Col(UVerticalBox* Parent, UWidget* Child, bool Fill, const FMargin& Padding = FMargin())
	{
		UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Child);
		Slot->SetSize(FSlateChildSize(Fill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
		Slot->SetHorizontalAlignment(HAlign_Fill);
		Slot->SetPadding(Padding);
	}
	USizeBox* Sized(UWidgetBlueprint* BP, const TCHAR* Name, float W, float H, UWidget* Child)
	{
		USizeBox* Box = New<USizeBox>(BP, Name);
		if (W > 0) Box->SetWidthOverride(W);
		if (H > 0) Box->SetHeightOverride(H);
		if (Child) Box->AddChild(Child);
		return Box;
	}
	UScrollBox* Scroll(UWidgetBlueprint* BP, const TCHAR* Name, UWidget* Child)
	{
		UScrollBox* Box = New<UScrollBox>(BP, Name);
		Box->AddChild(Child);
		return Box;
	}
	UVerticalBox* TextPage(UWidgetBlueprint* BP, const TCHAR* Prefix, const FText& Title)
	{
		UVerticalBox* Column = New<UVerticalBox>(BP, *FString::Printf(TEXT("Column_%s"), Prefix));
		Col(Column, Text(BP, *FString::Printf(TEXT("Text_%sTitle"), Prefix), Title, TEXT("TS_Knight_Title"), true), false, FMargin(0,0,0,24));
		Col(Column, Text(BP, *FString::Printf(TEXT("Text_%sBody"), Prefix), INVTEXT("설명 준비 중"), TEXT("TS_Knight_Body"), true), false);
		return Column;
	}
	void StyleCodexButton(UButton* Widget, const FLinearColor& Color)
	{
		if (!Widget) return;
		Widget->Modify();
		Widget->SetBackgroundColor(Color);
		FButtonStyle ButtonStyle = Widget->GetStyle();
		ButtonStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.83f, 1.05f, 1.20f, 1.f));
		ButtonStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.58f, 0.85f, 1.18f, 1.f));
		Widget->SetStyle(ButtonStyle);
	}
}

bool UKnightCodexEditorLibrary::BuildCodex(UWidgetBlueprint* BP)
{
	if (!Begin(BP)) return false;
	UCanvasPanel* Root = New<UCanvasPanel>(BP, TEXT("Canvas_Root"));
	BP->WidgetTree->RootWidget = Root;
	UBorder* Frame = New<UBorder>(BP, TEXT("Border_Frame"));
	Frame->SetBrushColor(Navy);
	UCanvasPanelSlot* FrameSlot = Root->AddChildToCanvas(Frame);
	FrameSlot->SetAnchors(FAnchors(0,0,1,1));
	FrameSlot->SetOffsets(FMargin(24,24,-24,-24));
	UHorizontalBox* Main = New<UHorizontalBox>(BP, TEXT("Row_Main"));
	Frame->AddChild(Main);
	UVerticalBox* Sidebar = New<UVerticalBox>(BP, TEXT("Column_Categories"));
	Row(Main, Sized(BP, TEXT("Width_Categories"), 180, 0, Sidebar), false, FMargin(20,20,16,20));
	Col(Sidebar, Text(BP, TEXT("Label_Codex"), INVTEXT("도감"), TEXT("TS_Knight_Subhead")), false, FMargin(0,0,0,20));
	Col(Sidebar, Button(BP, TEXT("Button_World"), INVTEXT("세계관")), false, FMargin(0,0,0,12));
	Col(Sidebar, Button(BP, TEXT("Button_Mech"), INVTEXT("기체 설명")), false, FMargin(0,0,0,12));
	Col(Sidebar, Button(BP, TEXT("Button_Monster"), INVTEXT("몬스터")), false, FMargin(0,0,0,12));
	UButton* Close = Button(BP, TEXT("Button_Close"), INVTEXT("닫기"));
	UWidgetSwitcher* Switcher = New<UWidgetSwitcher>(BP, TEXT("Switcher_Content"), true);
	UVerticalBox* Content = New<UVerticalBox>(BP, TEXT("Column_Content"));
	Row(Main, Content, true, FMargin(0,20,20,20));
	UHorizontalBox* Header = New<UHorizontalBox>(BP, TEXT("Row_ContentHeader"));
	Col(Content, Header, false, FMargin(0,0,0,12));
	Row(Header, New<USpacer>(BP, TEXT("Spacer_ContentHeader")), true);
	Row(Header, Close, false);
	Col(Content, Switcher, true);
	Switcher->AddChild(Scroll(BP, TEXT("Scroll_World"), TextPage(BP, TEXT("World"), INVTEXT("세계관"))));
	Switcher->AddChild(Scroll(BP, TEXT("Scroll_Mech"), TextPage(BP, TEXT("Mech"), INVTEXT("기체 설명"))));
	UHorizontalBox* Monster = New<UHorizontalBox>(BP, TEXT("Row_Monster"));
	Switcher->AddChild(Monster);
	UVerticalBox* List = New<UVerticalBox>(BP, TEXT("Column_BossList"));
	Col(List, Text(BP, TEXT("Label_BossList"), INVTEXT("보스 목록"), TEXT("TS_Knight_Subhead")), false, FMargin(0,0,0,12));
	Col(List, Text(BP, TEXT("Text_NoBosses"), INVTEXT("등록된 보스가 없습니다"), TEXT("TS_Knight_BodyDim"), true), false);
	Col(List, New<UVerticalBox>(BP, TEXT("Box_Bosses"), true), false);
	Row(Monster, Sized(BP, TEXT("Width_BossList"), 180, 0, Scroll(BP, TEXT("Scroll_BossList"), List)), false, FMargin(0,0,18,0));
	UVerticalBox* Detail = New<UVerticalBox>(BP, TEXT("Column_BossDetail"));
	Col(Detail, Text(BP, TEXT("Text_BossName"), INVTEXT("보스를 선택해 주세요"), TEXT("TS_Knight_Title"), true), false, FMargin(0,0,0,16));
	Col(Detail, Sized(BP, TEXT("Size_Portrait"), 0, 260, New<UImage>(BP, TEXT("Image_Portrait"), true)), false, FMargin(0,0,0,12));
	Col(Detail, Text(BP, TEXT("Text_NoPortrait"), INVTEXT("이미지 준비 중"), TEXT("TS_Knight_BodyDim"), true), false, FMargin(0,0,0,12));
	Col(Detail, Text(BP, TEXT("Text_BossDescription"), INVTEXT("설명 준비 중"), TEXT("TS_Knight_Body"), true), false, FMargin(0,0,0,16));
	Col(Detail, Text(BP, TEXT("Text_BossExtra"), FText::GetEmpty(), TEXT("TS_Knight_Body"), true), false, FMargin(0,0,0,16));
	Col(Detail, Text(BP, TEXT("Label_Drops"), INVTEXT("드랍 아이템"), TEXT("TS_Knight_Subhead")), false, FMargin(0,0,0,12));
	Col(Detail, New<UVerticalBox>(BP, TEXT("Box_Drops"), true), false);
	Col(Detail, Text(BP, TEXT("Text_NoDrops"), INVTEXT("등록된 드랍 정보가 없습니다"), TEXT("TS_Knight_BodyDim"), true), false);
	Row(Monster, Scroll(BP, TEXT("Scroll_BossDetail"), Detail), true);
	End(BP); return true;
}

bool UKnightCodexEditorLibrary::BuildBossEntry(UWidgetBlueprint* BP)
{
	if (!Begin(BP)) return false;
	UButton* Root = New<UButton>(BP, TEXT("Button_Select"), true);
	Root->SetBackgroundColor(Blue);
	BP->WidgetTree->RootWidget = Root;
	UVerticalBox* Column = New<UVerticalBox>(BP, TEXT("Column_Entry"));
	Root->AddChild(Column);
	Col(Column, Sized(BP, TEXT("Size_Icon"), 80, 80, New<UImage>(BP, TEXT("Image_Icon"), true)), false);
	Col(Column, Text(BP, TEXT("Text_Name"), INVTEXT("보스"), TEXT("TS_Knight_Button"), true), false, FMargin(4,4,4,8));
	End(BP); return true;
}

bool UKnightCodexEditorLibrary::BuildDropEntry(UWidgetBlueprint* BP)
{
	if (!Begin(BP)) return false;
	UHorizontalBox* Root = New<UHorizontalBox>(BP, TEXT("Row_Drop"));
	BP->WidgetTree->RootWidget = Root;
	Row(Root, Sized(BP, TEXT("Size_ItemIcon"), 48, 48, New<UImage>(BP, TEXT("Image_Icon"), true)), false, FMargin(0,0,12,8));
	UVerticalBox* Labels = New<UVerticalBox>(BP, TEXT("Column_DropLabels"));
	Row(Root, Labels, true);
	Col(Labels, Text(BP, TEXT("Text_Name"), INVTEXT("아이템"), TEXT("TS_Knight_Body"), true), false);
	Col(Labels, Text(BP, TEXT("Text_Details"), FText::GetEmpty(), TEXT("TS_Knight_Caption"), true), false);
	End(BP); return true;
}

bool UKnightCodexEditorLibrary::MoveCodexCloseButton(UWidgetBlueprint* BP)
{
	if (!BP || !BP->WidgetTree) return false;
	if (BP->WidgetTree->FindWidget(TEXT("Column_Content"))) return true;
	UHorizontalBox* Main = Cast<UHorizontalBox>(BP->WidgetTree->FindWidget(TEXT("Row_Main")));
	UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(BP->WidgetTree->FindWidget(TEXT("Switcher_Content")));
	UButton* Close = Cast<UButton>(BP->WidgetTree->FindWidget(TEXT("Button_Close")));
	if (!Main || !Switcher || !Close) return false;
	BP->Modify(); BP->WidgetTree->Modify();
	Switcher->RemoveFromParent();
	Close->RemoveFromParent();
	UVerticalBox* Content = New<UVerticalBox>(BP, TEXT("Column_Content"));
	Row(Main, Content, true, FMargin(0,20,20,20));
	UHorizontalBox* Header = New<UHorizontalBox>(BP, TEXT("Row_ContentHeader"));
	Col(Content, Header, false, FMargin(0,0,0,12));
	Row(Header, New<USpacer>(BP, TEXT("Spacer_ContentHeader")), true);
	Row(Header, Close, false);
	Col(Content, Switcher, true);
	End(BP);
	return true;
}

FName UKnightCodexEditorLibrary::GetWidgetParentName(UWidgetBlueprint* BP, FName WidgetName)
{
	const UWidget* Widget = BP && BP->WidgetTree ? BP->WidgetTree->FindWidget(WidgetName) : nullptr;
	return Widget && Widget->GetParent() ? Widget->GetParent()->GetFName() : NAME_None;
}

FString UKnightCodexEditorLibrary::DescribeTree(UWidgetBlueprint* BP)
{
	if (!BP || !BP->WidgetTree) return TEXT("No WidgetTree");
	TArray<UWidget*> Widgets;
	BP->WidgetTree->GetAllWidgets(Widgets);
	FString Result;
	for (const UWidget* Widget : Widgets)
	{
		if (!Widget) continue;
		Result += FString::Printf(TEXT("%s : %s <- %s\n"), *Widget->GetName(),
			*Widget->GetClass()->GetName(), Widget->GetParent() ? *Widget->GetParent()->GetName() : TEXT("ROOT"));
	}
	return Result;
}

bool UKnightCodexEditorLibrary::StyleCodex(UWidgetBlueprint* BP)
{
	if (!BP || !BP->WidgetTree) return false;
	UBorder* Frame = Cast<UBorder>(BP->WidgetTree->FindWidget(TEXT("Border_Frame")));
	UHorizontalBox* Main = Cast<UHorizontalBox>(BP->WidgetTree->FindWidget(TEXT("Row_Main")));
	USizeBox* SidebarWidth = Cast<USizeBox>(BP->WidgetTree->FindWidget(TEXT("Width_Categories")));
	UVerticalBox* Sidebar = Cast<UVerticalBox>(BP->WidgetTree->FindWidget(TEXT("Column_Categories")));
	UVerticalBox* Content = Cast<UVerticalBox>(BP->WidgetTree->FindWidget(TEXT("Column_Content")));
	if (!Frame || !Main || !SidebarWidth || !Sidebar || !Content) return false;
	BP->Modify(); BP->WidgetTree->Modify();
	Frame->Modify(); Frame->SetBrushColor(CodexFrame);
	SidebarWidth->Modify(); SidebarWidth->SetWidthOverride(194.f);
	UBorder* SidePanel = Cast<UBorder>(BP->WidgetTree->FindWidget(TEXT("Border_Categories")));
	if (!SidePanel)
	{
		Sidebar->RemoveFromParent();
		SidePanel = New<UBorder>(BP, TEXT("Border_Categories"));
		SidebarWidth->AddChild(SidePanel);
		SidePanel->AddChild(Sidebar);
	}
	SidePanel->Modify(); SidePanel->SetBrushColor(CodexSidebar); SidePanel->SetPadding(FMargin(14.f, 18.f));
	UBorder* ReadingPanel = Cast<UBorder>(BP->WidgetTree->FindWidget(TEXT("Border_ReadingPanel")));
	if (!ReadingPanel)
	{
		Content->RemoveFromParent();
		ReadingPanel = New<UBorder>(BP, TEXT("Border_ReadingPanel"));
		Row(Main, ReadingPanel, true, FMargin(0.f, 20.f, 20.f, 20.f));
		ReadingPanel->AddChild(Content);
	}
	ReadingPanel->Modify(); ReadingPanel->SetBrushColor(CodexReadingPanel); ReadingPanel->SetPadding(FMargin(20.f, 12.f));
	if (UCommonTextBlock* Label = Cast<UCommonTextBlock>(BP->WidgetTree->FindWidget(TEXT("Label_Codex"))))
	{
		Label->Modify(); Label->SetText(INVTEXT("작전 기록 / 도감"));
	}
	for (const TCHAR* Name : {TEXT("Button_World"), TEXT("Button_Mech"), TEXT("Button_Monster"), TEXT("Button_Close")})
		StyleCodexButton(Cast<UButton>(BP->WidgetTree->FindWidget(Name)), FLinearColor(FColor(0x32, 0x48, 0x5C)));
	for (const TCHAR* Name : {TEXT("Scroll_World"), TEXT("Scroll_Mech"), TEXT("Scroll_BossList"), TEXT("Scroll_BossDetail")})
	{
		if (UScrollBox* ScrollBox = Cast<UScrollBox>(BP->WidgetTree->FindWidget(Name)))
		{
			ScrollBox->Modify();
			ScrollBox->SetScrollbarThickness(FVector2D(7.f, 7.f));
			ScrollBox->SetScrollbarPadding(FMargin(5.f, 0.f, 0.f, 0.f));
		}
	}
	if (USizeBox* BossList = Cast<USizeBox>(BP->WidgetTree->FindWidget(TEXT("Width_BossList"))))
	{
		BossList->Modify(); BossList->SetWidthOverride(190.f);
	}
	if (USizeBox* Portrait = Cast<USizeBox>(BP->WidgetTree->FindWidget(TEXT("Size_Portrait"))))
	{
		Portrait->Modify(); Portrait->SetHeightOverride(220.f);
	}
	End(BP);
	return true;
}

bool UKnightCodexEditorLibrary::StyleBossEntry(UWidgetBlueprint* BP)
{
	if (!BP || !BP->WidgetTree) return false;
	UButton* Button = Cast<UButton>(BP->WidgetTree->FindWidget(TEXT("Button_Select")));
	UVerticalBox* Column = Cast<UVerticalBox>(BP->WidgetTree->FindWidget(TEXT("Column_Entry")));
	if (!Button || !Column) return false;
	BP->Modify(); BP->WidgetTree->Modify();
	StyleCodexButton(Button, FLinearColor(FColor(0x32, 0x48, 0x5C)));
	if (!BP->WidgetTree->FindWidget(TEXT("Row_LockState")))
	{
		UCommonTextBlock* LockState = Cast<UCommonTextBlock>(BP->WidgetTree->FindWidget(TEXT("Text_LockState")));
		if (!LockState) LockState = Text(BP, TEXT("Text_LockState"), INVTEXT("미해금"), TEXT("TS_Knight_Caption"), true);
		LockState->RemoveFromParent();
		LockState->Modify(); LockState->SetText(INVTEXT("미해금"));
		UHorizontalBox* LockRow = New<UHorizontalBox>(BP, TEXT("Row_LockState"), true);
		Col(Column, LockRow, false, FMargin(4.f, 0.f, 4.f, 8.f));
		UVerticalBox* Glyph = New<UVerticalBox>(BP, TEXT("Column_LockGlyph"));
		Row(LockRow, Sized(BP, TEXT("Size_LockGlyph"), 22.f, 25.f, Glyph), false, FMargin(0.f, 0.f, 6.f, 0.f));
		UBorder* Shackle = New<UBorder>(BP, TEXT("Border_LockShackle"));
		Shackle->SetBrushColor(Frost); Shackle->SetPadding(FMargin(3.f, 3.f, 3.f, 0.f));
		UBorder* ShackleHole = New<UBorder>(BP, TEXT("Border_LockShackleHole"));
		ShackleHole->SetBrushColor(CodexReadingPanel);
		Shackle->AddChild(ShackleHole);
		Col(Glyph, Sized(BP, TEXT("Size_LockShackle"), 14.f, 11.f, Shackle), false, FMargin(4.f, 0.f, 0.f, 0.f));
		UBorder* LockBody = New<UBorder>(BP, TEXT("Border_LockBody"));
		LockBody->SetBrushColor(Frost);
		Col(Glyph, Sized(BP, TEXT("Size_LockBody"), 22.f, 14.f, LockBody), false);
		Row(LockRow, LockState, true);
	}
	for (const TPair<const TCHAR*, FVector2D>& Size : {
		TPair<const TCHAR*, FVector2D>(TEXT("Size_LockGlyph"), FVector2D(22.f, 25.f)),
		TPair<const TCHAR*, FVector2D>(TEXT("Size_LockShackle"), FVector2D(14.f, 11.f)),
		TPair<const TCHAR*, FVector2D>(TEXT("Size_LockBody"), FVector2D(22.f, 14.f)) })
	{
		if (USizeBox* Box = Cast<USizeBox>(BP->WidgetTree->FindWidget(Size.Key)))
		{
			Box->Modify(); Box->SetWidthOverride(Size.Value.X); Box->SetHeightOverride(Size.Value.Y);
		}
	}
	if (UBorder* Shackle = Cast<UBorder>(BP->WidgetTree->FindWidget(TEXT("Border_LockShackle"))))
	{
		Shackle->Modify(); Shackle->SetPadding(FMargin(3.f, 3.f, 3.f, 0.f));
	}
	if (USizeBox* Icon = Cast<USizeBox>(BP->WidgetTree->FindWidget(TEXT("Size_Icon"))))
	{
		Icon->Modify(); Icon->SetWidthOverride(72.f); Icon->SetHeightOverride(72.f);
	}
	End(BP);
	return true;
}

bool UKnightCodexEditorLibrary::StyleDropEntry(UWidgetBlueprint* BP)
{
	if (!BP || !BP->WidgetTree) return false;
	USizeBox* Icon = Cast<USizeBox>(BP->WidgetTree->FindWidget(TEXT("Size_ItemIcon")));
	if (!Icon) return false;
	BP->Modify(); BP->WidgetTree->Modify();
	Icon->Modify(); Icon->SetWidthOverride(44.f); Icon->SetHeightOverride(44.f);
	End(BP);
	return true;
}

UScrollBox* UKnightCodexEditorLibrary::FindScrollBox(UUserWidget* Widget, FName WidgetName)
{
	return Widget ? Cast<UScrollBox>(Widget->GetWidgetFromName(WidgetName)) : nullptr;
}

bool UKnightCodexEditorLibrary::RedesignCodex(UWidgetBlueprint* BP)
{
	if (!BP || !BP->WidgetTree) return false;
	// These presentation-only widgets were replaced. Remove their editor variable GUIDs as well,
	// otherwise WidgetBlueprintCompiler reports missing-variable ensures on later compiles.
	auto RemoveOldGuids = [BP]()
	{
		for (const TCHAR* Name : {TEXT("Spacer_Sidebar"), TEXT("Row_ContentHeader"),
			TEXT("Spacer_ContentHeader"), TEXT("Row_Monster"), TEXT("Width_BossList")})
			BP->OnVariableRemoved(Name);
	};
	if (BP->WidgetTree->FindWidget(TEXT("Switcher_Entries")))
	{
		for (const TCHAR* Name : {TEXT("Button_World"), TEXT("Button_Mech"), TEXT("Button_Monster"), TEXT("Button_Close")})
			StyleCodexButton(Cast<UButton>(BP->WidgetTree->FindWidget(Name)), FLinearColor(0.10f, 0.17f, 0.25f, 1.f));
		RemoveOldGuids();
		End(BP);
		return true;
	}
	auto Find = [BP](const TCHAR* Name) { return BP->WidgetTree->FindWidget(Name); };
	UBorder* Frame = Cast<UBorder>(Find(TEXT("Border_Frame")));
	UHorizontalBox* Main = Cast<UHorizontalBox>(Find(TEXT("Row_Main")));
	USizeBox* CategoriesWidth = Cast<USizeBox>(Find(TEXT("Width_Categories")));
	UBorder* CategoriesPanel = Cast<UBorder>(Find(TEXT("Border_Categories")));
	UVerticalBox* Categories = Cast<UVerticalBox>(Find(TEXT("Column_Categories")));
	UBorder* DetailPanel = Cast<UBorder>(Find(TEXT("Border_ReadingPanel")));
	UVerticalBox* Detail = Cast<UVerticalBox>(Find(TEXT("Column_Content")));
	UWidgetSwitcher* Content = Cast<UWidgetSwitcher>(Find(TEXT("Switcher_Content")));
	UHorizontalBox* OldMonster = Cast<UHorizontalBox>(Find(TEXT("Row_Monster")));
	UScrollBox* BossList = Cast<UScrollBox>(Find(TEXT("Scroll_BossList")));
	UScrollBox* BossDetail = Cast<UScrollBox>(Find(TEXT("Scroll_BossDetail")));
	UCommonTextBlock* Title = Cast<UCommonTextBlock>(Find(TEXT("Label_Codex")));
	UButton* Close = Cast<UButton>(Find(TEXT("Button_Close")));
	UButton* World = Cast<UButton>(Find(TEXT("Button_World")));
	UButton* Mech = Cast<UButton>(Find(TEXT("Button_Mech")));
	UButton* Monster = Cast<UButton>(Find(TEXT("Button_Monster")));
	if (!Frame || !Main || !CategoriesWidth || !CategoriesPanel || !Categories || !DetailPanel || !Detail ||
		!Content || !OldMonster || !BossList || !BossDetail || !Title || !Close || !World || !Mech || !Monster) return false;
	BP->Modify(); BP->WidgetTree->Modify();
	TArray<UWidget*> WidgetsToDetach = {Title, Close, World, Mech, Monster, CategoriesWidth,
		DetailPanel, Content, BossList, BossDetail, Main};
	for (UWidget* Widget : WidgetsToDetach) Widget->RemoveFromParent();
	Content->RemoveChild(OldMonster);
	Content->AddChild(BossDetail); // World, Mech, Monster remain indices 0, 1, 2.
	Categories->ClearChildren();
	Detail->ClearChildren();
	Main->ClearChildren();
	Frame->ClearChildren();

	// The same full-viewport anchor and the project's DPI curve drive the shell at every resolution.
	if (UCanvasPanelSlot* FrameSlot = Cast<UCanvasPanelSlot>(Frame->Slot))
	{
		FrameSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		FrameSlot->SetOffsets(FMargin(28.f, 28.f, -28.f, -28.f));
	}
	Frame->SetBrushColor(FLinearColor(0.15f, 0.49f, 0.68f, 0.85f));
	Frame->SetPadding(FMargin(1.f));
	UBorder* Surface = New<UBorder>(BP, TEXT("Border_CodexSurface"));
	Surface->SetBrushColor(FLinearColor(0.025f, 0.055f, 0.095f, 0.89f));
	Surface->SetPadding(FMargin(16.f, 12.f));
	Frame->AddChild(Surface);
	UVerticalBox* Shell = New<UVerticalBox>(BP, TEXT("Column_CodexShell"));
	Surface->AddChild(Shell);
	UHorizontalBox* Header = New<UHorizontalBox>(BP, TEXT("Row_CodexHeader"));
	Col(Shell, Header, false, FMargin(6.f, 0.f, 6.f, 10.f));
	Title->SetText(INVTEXT("도감"));
	if (TSubclassOf<UCommonTextStyle> S = Style(TEXT("TS_Knight_Title"))) Title->SetStyle(S);
	Row(Header, Title, true);
	StyleCodexButton(Close, FLinearColor(0.10f, 0.18f, 0.28f, 1.f));
	Row(Header, Sized(BP, TEXT("Width_CodexClose"), 96.f, 0.f, Close), false);
	UBorder* HeaderRule = New<UBorder>(BP, TEXT("Rule_CodexHeader"));
	HeaderRule->SetBrushColor(FLinearColor(0.20f, 0.54f, 0.73f, 0.65f));
	HeaderRule->SetVisibility(ESlateVisibility::HitTestInvisible);
	Col(Shell, Sized(BP, TEXT("Height_CodexHeaderRule"), 0.f, 1.f, HeaderRule), false, FMargin(0.f, 0.f, 0.f, 12.f));
	Col(Shell, Main, true);

	CategoriesWidth->SetWidthOverride(174.f);
	CategoriesPanel->SetBrushColor(FLinearColor(0.14f, 0.39f, 0.55f, 0.80f));
	CategoriesPanel->SetPadding(FMargin(1.f));
	Categories->RemoveFromParent();
	UBorder* CategoriesSurface = New<UBorder>(BP, TEXT("Border_CategorySurface"));
	CategoriesSurface->SetBrushColor(FLinearColor(0.035f, 0.08f, 0.13f, 0.73f));
	CategoriesSurface->SetPadding(FMargin(10.f, 14.f));
	CategoriesPanel->AddChild(CategoriesSurface);
	CategoriesSurface->AddChild(Categories);
	Col(Categories, Text(BP, TEXT("Label_CodexCategories"), INVTEXT("분류"), TEXT("TS_Knight_Caption")), false, FMargin(4.f, 0.f, 0.f, 14.f));
	for (UButton* ButtonWidget : {World, Mech, Monster})
	{
		StyleCodexButton(ButtonWidget, FLinearColor(0.10f, 0.17f, 0.25f, 1.f));
		if (UWidget* Label = ButtonWidget->GetContent())
			if (UButtonSlot* Slot = Cast<UButtonSlot>(Label->Slot)) Slot->SetPadding(FMargin(10.f, 11.f));
		Col(Categories, ButtonWidget, false, FMargin(0.f, 0.f, 0.f, 8.f));
	}
	Row(Main, CategoriesWidth, false, FMargin(0.f, 0.f, 12.f, 0.f));

	USizeBox* EntriesWidth = New<USizeBox>(BP, TEXT("Width_CodexEntries"));
	EntriesWidth->SetWidthOverride(318.f);
	Row(Main, EntriesWidth, false, FMargin(0.f, 0.f, 12.f, 0.f));
	UBorder* EntriesBorder = New<UBorder>(BP, TEXT("Border_CodexEntries"));
	EntriesBorder->SetBrushColor(FLinearColor(0.14f, 0.39f, 0.55f, 0.80f));
	EntriesBorder->SetPadding(FMargin(1.f));
	EntriesWidth->AddChild(EntriesBorder);
	UBorder* EntriesSurface = New<UBorder>(BP, TEXT("Border_EntriesSurface"));
	EntriesSurface->SetBrushColor(FLinearColor(0.025f, 0.065f, 0.11f, 0.77f));
	EntriesSurface->SetPadding(FMargin(12.f, 12.f, 8.f, 12.f));
	EntriesBorder->AddChild(EntriesSurface);
	UVerticalBox* EntriesColumn = New<UVerticalBox>(BP, TEXT("Column_CodexEntries"));
	EntriesSurface->AddChild(EntriesColumn);
	Col(EntriesColumn, Text(BP, TEXT("Label_CodexEntries"), INVTEXT("항목"), TEXT("TS_Knight_Subhead")), false, FMargin(3.f, 0.f, 0.f, 10.f));
	UWidgetSwitcher* EntrySwitcher = New<UWidgetSwitcher>(BP, TEXT("Switcher_Entries"), true);
	Col(EntriesColumn, EntrySwitcher, true);
	for (const TCHAR* Name : {TEXT("World"), TEXT("Mech")})
	{
		UVerticalBox* List = New<UVerticalBox>(BP, *FString::Printf(TEXT("Box_%sEntries"), Name));
		EntrySwitcher->AddChild(Scroll(BP, *FString::Printf(TEXT("Scroll_%sEntries"), Name), List));
	}
	EntrySwitcher->AddChild(BossList);
	if (UWidget* BossListLabel = Find(TEXT("Label_BossList"))) BossListLabel->SetVisibility(ESlateVisibility::Collapsed);

	DetailPanel->SetBrushColor(FLinearColor(0.14f, 0.39f, 0.55f, 0.80f));
	DetailPanel->SetPadding(FMargin(1.f));
	Detail->RemoveFromParent();
	UBorder* DetailSurface = New<UBorder>(BP, TEXT("Border_DetailSurface"));
	DetailSurface->SetBrushColor(FLinearColor(0.025f, 0.06f, 0.10f, 0.70f));
	DetailSurface->SetPadding(FMargin(20.f, 16.f));
	DetailPanel->AddChild(DetailSurface);
	DetailSurface->AddChild(Detail);
	Col(Detail, Content, true);
	Row(Main, DetailPanel, true);

	UBorder* FooterRule = New<UBorder>(BP, TEXT("Rule_CodexFooter"));
	FooterRule->SetBrushColor(FLinearColor(0.20f, 0.54f, 0.73f, 0.55f));
	FooterRule->SetVisibility(ESlateVisibility::HitTestInvisible);
	Col(Shell, Sized(BP, TEXT("Height_CodexFooterRule"), 0.f, 1.f, FooterRule), false, FMargin(0.f, 12.f, 0.f, 7.f));
	Col(Shell, Text(BP, TEXT("Label_CodexHints"), INVTEXT("항목 선택  ·  마우스 휠로 읽기  ·  Esc 뒤로"), TEXT("TS_Knight_Caption")), false, FMargin(6.f, 0.f, 0.f, 0.f));
	for (const TCHAR* Name : {TEXT("Scroll_World"), TEXT("Scroll_Mech"), TEXT("Scroll_BossList"), TEXT("Scroll_BossDetail"), TEXT("Scroll_WorldEntries"), TEXT("Scroll_MechEntries")})
		if (UScrollBox* Box = Cast<UScrollBox>(Find(Name)))
		{
			Box->SetScrollbarThickness(FVector2D(6.f, 6.f));
			Box->SetScrollbarPadding(FMargin(4.f, 0.f, 0.f, 0.f));
		}
	RemoveOldGuids();
	End(BP);
	return true;
}

bool UKnightCodexEditorLibrary::RedesignBossEntry(UWidgetBlueprint* BP)
{
	if (!BP || !BP->WidgetTree) return false;
	auto EnsureRowHeight = [BP]()
	{
		if (BP->WidgetTree->FindWidget(TEXT("Size_BossRecord"))) return;
		UButton* ButtonRoot = Cast<UButton>(BP->WidgetTree->RootWidget);
		if (!ButtonRoot) return;
		USizeBox* Root = New<USizeBox>(BP, TEXT("Size_BossRecord"));
		Root->SetHeightOverride(72.f);
		Root->AddChild(ButtonRoot);
		BP->WidgetTree->RootWidget = Root;
	};
	if (BP->WidgetTree->FindWidget(TEXT("Row_BossRecord")))
	{
		BP->Modify(); BP->WidgetTree->Modify();
		StyleCodexButton(Cast<UButton>(BP->WidgetTree->FindWidget(TEXT("Button_Select"))), FLinearColor(0.10f, 0.17f, 0.25f, 1.f));
		EnsureRowHeight();
		End(BP);
		return true;
	}
	UButton* ButtonWidget = Cast<UButton>(BP->WidgetTree->FindWidget(TEXT("Button_Select")));
	UVerticalBox* OldColumn = Cast<UVerticalBox>(BP->WidgetTree->FindWidget(TEXT("Column_Entry")));
	USizeBox* Icon = Cast<USizeBox>(BP->WidgetTree->FindWidget(TEXT("Size_Icon")));
	UCommonTextBlock* Name = Cast<UCommonTextBlock>(BP->WidgetTree->FindWidget(TEXT("Text_Name")));
	UWidget* Lock = BP->WidgetTree->FindWidget(TEXT("Row_LockState"));
	if (!ButtonWidget || !OldColumn || !Icon || !Name || !Lock) return false;
	BP->Modify(); BP->WidgetTree->Modify();
	Icon->RemoveFromParent(); Name->RemoveFromParent(); Lock->RemoveFromParent();
	Icon->SetWidthOverride(44.f); Icon->SetHeightOverride(44.f);
	UHorizontalBox* RowWidget = New<UHorizontalBox>(BP, TEXT("Row_BossRecord"));
	Col(OldColumn, RowWidget, false);
	Row(RowWidget, Icon, false, FMargin(0.f, 0.f, 10.f, 0.f));
	UVerticalBox* Labels = New<UVerticalBox>(BP, TEXT("Column_BossRecordLabels"));
	Row(RowWidget, Labels, true);
	Col(Labels, Name, false);
	Col(Labels, Lock, false, FMargin(0.f, 3.f, 0.f, 0.f));
	if (UWidget* Label = ButtonWidget->GetContent())
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Label->Slot)) Slot->SetPadding(FMargin(10.f, 9.f));
	StyleCodexButton(ButtonWidget, FLinearColor(0.10f, 0.17f, 0.25f, 1.f));
	EnsureRowHeight();
	End(BP);
	return true;
}

bool UKnightCodexEditorLibrary::RedesignDropEntry(UWidgetBlueprint* BP)
{
	if (!BP || !BP->WidgetTree) return false;
	if (BP->WidgetTree->FindWidget(TEXT("Border_DropRow"))) return true;
	UHorizontalBox* Root = Cast<UHorizontalBox>(BP->WidgetTree->FindWidget(TEXT("Row_Drop")));
	USizeBox* Icon = Cast<USizeBox>(BP->WidgetTree->FindWidget(TEXT("Size_ItemIcon")));
	UVerticalBox* Labels = Cast<UVerticalBox>(BP->WidgetTree->FindWidget(TEXT("Column_DropLabels")));
	if (!Root || !Icon || !Labels) return false;
	BP->Modify(); BP->WidgetTree->Modify();
	Icon->RemoveFromParent(); Labels->RemoveFromParent();
	UBorder* Border = New<UBorder>(BP, TEXT("Border_DropRow"));
	Border->SetBrushColor(FLinearColor(0.08f, 0.16f, 0.24f, 0.62f));
	Border->SetPadding(FMargin(10.f, 7.f));
	Row(Root, Border, true, FMargin(0.f, 0.f, 0.f, 5.f));
	UHorizontalBox* Content = New<UHorizontalBox>(BP, TEXT("Row_DropContent"));
	Border->AddChild(Content);
	Icon->SetWidthOverride(40.f); Icon->SetHeightOverride(40.f);
	Row(Content, Icon, false, FMargin(0.f, 0.f, 10.f, 0.f));
	Row(Content, Labels, true);
	End(BP);
	return true;
}
