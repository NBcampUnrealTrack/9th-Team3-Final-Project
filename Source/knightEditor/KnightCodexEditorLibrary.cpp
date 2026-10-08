#include "KnightCodexEditorLibrary.h"

#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
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

UScrollBox* UKnightCodexEditorLibrary::FindScrollBox(UUserWidget* Widget, FName WidgetName)
{
	return Widget ? Cast<UScrollBox>(Widget->GetWidgetFromName(WidgetName)) : nullptr;
}
