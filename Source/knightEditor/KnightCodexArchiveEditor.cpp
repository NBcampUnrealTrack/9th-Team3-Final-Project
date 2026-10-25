#include "KnightCodexEditorLibrary.h"

#include "Codex/KnightCodexPresentation.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "WidgetBlueprint.h"

namespace CodexArchiveDesigner
{
	using namespace KnightCodexPresentation;
	template<class T> T* Make(UWidgetBlueprint* BP, const TCHAR* Name, bool bVariable = false)
	{
		T* Widget = BP->WidgetTree->ConstructWidget<T>(T::StaticClass(), Name);
		Widget->bIsVariable = bVariable;
		BP->OnVariableAdded(Name);
		return Widget;
	}
	UTextBlock* Text(UWidgetBlueprint* BP, const TCHAR* Name, const FText& Value, int32 Size, bool bBold=false, bool bMuted=false, bool bVariable=false)
	{
		UTextBlock* Widget = Make<UTextBlock>(BP, Name, bVariable);
		Widget->SetText(Value); TextStyle(Widget, Size, bBold, bMuted);
		return Widget;
	}
	void Col(UVerticalBox* Parent, UWidget* Child, bool Fill=false, FMargin Padding=FMargin())
	{
		UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Child);
		Slot->SetSize(FSlateChildSize(Fill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
		Slot->SetHorizontalAlignment(HAlign_Fill); Slot->SetPadding(Padding);
	}
	void Row(UHorizontalBox* Parent, UWidget* Child, float Weight=0.f, FMargin Padding=FMargin())
	{
		UHorizontalBoxSlot* Slot = Parent->AddChildToHorizontalBox(Child);
		FSlateChildSize Size(Weight > 0.f ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic);
		Size.Value = Weight; Slot->SetSize(Size); Slot->SetPadding(Padding); Slot->SetVerticalAlignment(VAlign_Fill);
	}
	void Overlay(UOverlay* Parent, UWidget* Child)
	{
		UOverlaySlot* Slot = Parent->AddChildToOverlay(Child);
		Slot->SetHorizontalAlignment(HAlign_Fill); Slot->SetVerticalAlignment(VAlign_Fill);
	}
	USizeBox* Size(UWidgetBlueprint* BP, const TCHAR* Name, UWidget* Child, float Width=0.f, float Height=0.f)
	{
		USizeBox* Box=Make<USizeBox>(BP,Name);
		if (Width>0) Box->SetWidthOverride(Width);
		if (Height>0) Box->SetHeightOverride(Height);
		if (Child) Box->AddChild(Child);
		return Box;
	}
	USizeBox* OptionalImage(UWidgetBlueprint* BP, const TCHAR* ContainerName, const TCHAR* ImageName, float Width, float Height)
	{
		UImage* Image=Make<UImage>(BP,ImageName,true);
		Image->SetVisibility(ESlateVisibility::Collapsed);
		UScaleBox* Fit=Make<UScaleBox>(BP,*FString::Printf(TEXT("%s_Fit"),ImageName));
		Fit->SetStretch(EStretch::ScaleToFit); Fit->AddChild(Image);
		USizeBox* Box=Size(BP,ContainerName,Fit,Width,Height);
		Box->SetVisibility(ESlateVisibility::Collapsed);
		return Box;
	}
	UKnightCodexSymbolWidget* Symbol(UWidgetBlueprint* BP, const TCHAR* Name, EKnightCodexSymbol Kind, float Extent=40.f)
	{
		UKnightCodexSymbolWidget* W=Make<UKnightCodexSymbolWidget>(BP,Name);
		W->Symbol=Kind; W->SymbolSize=FVector2D(Extent); W->Ink=Color(TEXT("85B5CF"));
		W->SetVisibility(ESlateVisibility::HitTestInvisible);
		return W;
	}
	UWidget* Rule(UWidgetBlueprint* BP,const TCHAR* Name)
	{
		UBorder* B=Make<UBorder>(BP,*FString::Printf(TEXT("%s_Ink"),Name));
		B->SetBrushColor(Color(TEXT("547B93"),.38f)); B->SetPadding(FMargin());
		B->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Size(BP,Name,B,0.f,1.f);
	}
	UOverlay* Panel(UWidgetBlueprint* BP,const TCHAR* Name,UWidget* Content,float Alpha,FMargin Padding,bool Grid=false)
	{
		UOverlay* P=Make<UOverlay>(BP,Name);
		UBorder* Surface=Make<UBorder>(BP,*FString::Printf(TEXT("%s_Surface"),Name));
		Surface->SetBrushColor(Color(TEXT("071018"),Alpha)); Surface->SetPadding(Padding);
		Surface->SetVisibility(ESlateVisibility::SelfHitTestInvisible); Surface->AddChild(Content); Overlay(P,Surface);
		UKnightCodexSymbolWidget* Frame=Symbol(BP,*FString::Printf(TEXT("%s_Frame"),Name),EKnightCodexSymbol::Frame);
		Frame->Ink=Color(TEXT("5B88A3"),.58f); Frame->bGrid=Grid; Overlay(P,Frame);
		return P;
	}
	UScrollBox* Scroll(UWidgetBlueprint* BP,const TCHAR* Name,UWidget* Child)
	{
		UScrollBox* W=Make<UScrollBox>(BP,Name);
		W->AddChild(Child); W->SetScrollbarThickness(FVector2D(4.f)); W->SetScrollbarPadding(FMargin(12.f,0.f,0.f,0.f));
		FScrollBoxStyle Style=W->GetWidgetStyle();
		Style.TopShadowBrush=FSlateNoResource(); Style.BottomShadowBrush=FSlateNoResource();
		Style.LeftShadowBrush=FSlateNoResource(); Style.RightShadowBrush=FSlateNoResource(); W->SetWidgetStyle(Style);
		FScrollBarStyle Bar=W->GetWidgetBarStyle();
		Bar.NormalThumbImage=FSlateColorBrush(Color(TEXT("628CA4"),.80f));
		Bar.HoveredThumbImage=FSlateColorBrush(Color(TEXT("80CEE8")));
		Bar.DraggedThumbImage=FSlateColorBrush(Color(TEXT("A8E8F8")));
		Bar.VerticalBackgroundImage=FSlateColorBrush(Color(TEXT("35556A"),.18f)); W->SetWidgetBarStyle(Bar);
		W->SetAnimateWheelScrolling(true); W->SetWheelScrollMultiplier(1.3f);
		return W;
	}
	UKnightCodexArchiveButton* Button(UWidgetBlueprint* BP,const TCHAR* Name,UWidget* Child,FMargin Padding)
	{
		UKnightCodexArchiveButton* W=Make<UKnightCodexArchiveButton>(BP,Name,true);
		W->AddChild(Child);
		UButtonSlot* ButtonSlot=CastChecked<UButtonSlot>(Child->Slot);
		ButtonSlot->SetPadding(Padding); ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
		return W;
	}
	UWidget* TextPage(UWidgetBlueprint* BP,const TCHAR* Prefix,const FText& Title)
	{
		UVerticalBox* Page=Make<UVerticalBox>(BP,*FString::Printf(TEXT("Column_%s"),Prefix));
		Col(Page,Text(BP,*FString::Printf(TEXT("Text_%sTitle"),Prefix),Title,23,true,false,true),false,FMargin(0,0,0,18));
		Col(Page,Rule(BP,*FString::Printf(TEXT("Rule_%sTitle"),Prefix)),false,FMargin(0,0,0,24));
		UVerticalBox* Body=Make<UVerticalBox>(BP,*FString::Printf(TEXT("Body_%s"),Prefix));
		Col(Body,Text(BP,*FString::Printf(TEXT("Text_%sBody"),Prefix),INVTEXT("설명 준비 중"),18,false,false,true),false,FMargin(0,0,20,20));
		Col(Page,Scroll(BP,*FString::Printf(TEXT("Scroll_%s"),Prefix),Body),true);
		return Page;
	}
	void BuildMain(UWidgetBlueprint* BP)
	{
		UCanvasPanel* Root=Make<UCanvasPanel>(BP,TEXT("Canvas_Root")); BP->WidgetTree->RootWidget=Root;
		UBorder* Scrim=Make<UBorder>(BP,TEXT("Border_CodexScrim")); Scrim->SetBrushColor(Color(TEXT("02070C"),.48f));
		Scrim->SetVisibility(ESlateVisibility::HitTestInvisible);
		UCanvasPanelSlot* ScrimSlot=Root->AddChildToCanvas(Scrim); ScrimSlot->SetAnchors(FAnchors(0,0,1,1)); ScrimSlot->SetOffsets(FMargin());
		UVerticalBox* Shell=Make<UVerticalBox>(BP,TEXT("Column_CodexShell"));
		UCanvasPanelSlot* ShellSlot=Root->AddChildToCanvas(Shell); ShellSlot->SetAnchors(FAnchors(0,0,1,1));
		// Stretched canvas offsets are positive distances from the far edges, not negative positions.
		ShellSlot->SetOffsets(FMargin(64.f,48.f,64.f,36.f));
		UHorizontalBox* Header=Make<UHorizontalBox>(BP,TEXT("Row_CodexHeader")); Col(Shell,Header,false,FMargin(4,0,4,18));
		UVerticalBox* Heading=Make<UVerticalBox>(BP,TEXT("Column_ArchiveHeading")); Row(Header,Heading,1.f);
		Col(Heading,Text(BP,TEXT("Label_Codex"),INVTEXT("도감"),28,true));
		Col(Heading,Text(BP,TEXT("Label_ArchiveSubtitle"),INVTEXT("기체 내 기록 보관소"),13,false,true),false,FMargin(2,4,0,0));
		UHorizontalBox* CloseContent=Make<UHorizontalBox>(BP,TEXT("Row_CloseContent"));
		UTextBlock* CloseLabel=Text(BP,TEXT("Label_Button_Close"),INVTEXT("닫기"),16);
		CloseLabel->SetJustification(ETextJustify::Center); Row(CloseContent,CloseLabel,1.f);
		UKnightCodexArchiveButton* Close=Button(BP,TEXT("Button_Close"),CloseContent,FMargin(18,12));
		USizeBox* CloseSize=Size(BP,TEXT("Size_Close"),Close,112,50); Row(Header,CloseSize);
		CastChecked<UHorizontalBoxSlot>(CloseSize->Slot)->SetVerticalAlignment(VAlign_Center);
		Col(Shell,Rule(BP,TEXT("Rule_Header")),false,FMargin(0,0,0,26));
		UHorizontalBox* Body=Make<UHorizontalBox>(BP,TEXT("Row_Main")); Col(Shell,Body,true);
		UVerticalBox* Rail=Make<UVerticalBox>(BP,TEXT("Column_Categories"));
		const TCHAR* Names[]={TEXT("World"),TEXT("Mech"),TEXT("Monster")};
		const FText Labels[]={INVTEXT("세계관"),INVTEXT("기체 설명"),INVTEXT("몬스터")};
		for(int32 Index=0;Index<3;++Index)
		{
			UVerticalBox* Tile=Make<UVerticalBox>(BP,*FString::Printf(TEXT("Tile_%s"),Names[Index]));
			Col(Tile,OptionalImage(BP,*FString::Printf(TEXT("Size_CategoryIcon_%s"),Names[Index]),
				*FString::Printf(TEXT("Image_Category_%s"),Names[Index]),0,64),false,FMargin(0,0,0,10));
			UTextBlock* Label=Text(BP,*FString::Printf(TEXT("Label_Button_%s"),Names[Index]),Labels[Index],15,true);
			Label->SetJustification(ETextJustify::Center); Label->SetAutoWrapText(false); Col(Tile,Label);
			UKnightCodexArchiveButton* B=Button(BP,*FString::Printf(TEXT("Button_%s"),Names[Index]),Tile,FMargin(14,18));
			B->bCategoryTile=true;
			USizeBox* TileSize=Size(BP,*FString::Printf(TEXT("Size_Category_%s"),Names[Index]),B);
			TileSize->SetMinDesiredHeight(72.f); Col(Rail,TileSize,false,FMargin(0,0,0,20));
		}
		Row(Body,Rail,.11f,FMargin(0,0,28,0));
		UVerticalBox* EntryColumn=Make<UVerticalBox>(BP,TEXT("Column_CodexEntries"));
		Col(EntryColumn,Text(BP,TEXT("Label_CodexEntries"),INVTEXT("세계관 기록"),17,true,false,true),false,FMargin(3,0,0,16));
		UWidgetSwitcher* Entries=Make<UWidgetSwitcher>(BP,TEXT("Switcher_Entries"),true); Col(EntryColumn,Entries,true);
		Entries->AddChild(Scroll(BP,TEXT("Scroll_WorldEntries"),Make<UVerticalBox>(BP,TEXT("Box_WorldEntries"))));
		Entries->AddChild(Scroll(BP,TEXT("Scroll_MechEntries"),Make<UVerticalBox>(BP,TEXT("Box_MechEntries"))));
		UVerticalBox* BossList=Make<UVerticalBox>(BP,TEXT("Column_BossList"));
		Col(BossList,Text(BP,TEXT("Text_NoBosses"),INVTEXT("등록된 몬스터가 없습니다."),16,false,true,true));
		Col(BossList,Make<UVerticalBox>(BP,TEXT("Box_Bosses"),true));
		Entries->AddChild(Scroll(BP,TEXT("Scroll_BossList"),BossList));
		Row(Body,Panel(BP,TEXT("Panel_EntryList"),EntryColumn,.55f,FMargin(18,20,16,18)),.34f,FMargin(0,0,28,0));
		UWidgetSwitcher* Content=Make<UWidgetSwitcher>(BP,TEXT("Switcher_Content"),true);
		Content->AddChild(TextPage(BP,TEXT("World"),INVTEXT("세계관")));
		Content->AddChild(TextPage(BP,TEXT("Mech"),INVTEXT("기체 설명")));
		UVerticalBox* MonsterPage=Make<UVerticalBox>(BP,TEXT("Column_MonsterPage")); Content->AddChild(MonsterPage);
		Col(MonsterPage,Text(BP,TEXT("Text_BossName"),INVTEXT("몬스터 기록"),23,true,false,true),false,FMargin(0,0,0,18));
		Col(MonsterPage,Rule(BP,TEXT("Rule_MonsterTitle")),false,FMargin(0,0,0,24));
		UVerticalBox* MonsterBody=Make<UVerticalBox>(BP,TEXT("Column_BossDetail"));
		Col(MonsterBody,OptionalImage(BP,TEXT("Size_Portrait"),TEXT("Image_Portrait"),0,220),false,FMargin(0,0,0,10));
		Col(MonsterBody,Text(BP,TEXT("Text_BossDescription"),INVTEXT("설명 준비 중"),18,false,false,true),false,FMargin(0,0,16,22));
		Col(MonsterBody,Text(BP,TEXT("Text_BossExtra"),FText::GetEmpty(),17,false,true,true),false,FMargin(0,0,16,22));
		Col(MonsterBody,Text(BP,TEXT("Label_Drops"),INVTEXT("드랍 아이템"),18,true),false,FMargin(0,12,0,14));
		Col(MonsterBody,Make<UVerticalBox>(BP,TEXT("Box_Drops"),true));
		Col(MonsterBody,Text(BP,TEXT("Text_NoDrops"),INVTEXT("등록된 드랍 정보가 없습니다."),15,false,true,true));
		Col(MonsterPage,Scroll(BP,TEXT("Scroll_BossDetail"),MonsterBody),true);
		Row(Body,Panel(BP,TEXT("Panel_Reader"),Content,.73f,FMargin(34,28,28,24),true),.55f);
		UHorizontalBox* Footer=Make<UHorizontalBox>(BP,TEXT("Row_ArchiveFooter")); Col(Shell,Footer,false,FMargin(4,22,4,0));
		Row(Footer,Text(BP,TEXT("Label_CodexHints"),INVTEXT("항목을 선택하여 기록을 열람하세요."),14,false,true),1.f);
		Row(Footer,Text(BP,TEXT("Label_CodexControls"),INVTEXT("마우스 휠  스크롤     Esc  뒤로"),14,false,true));
	}
	void BuildBoss(UWidgetBlueprint* BP)
	{
		USizeBox* Root=Size(BP,TEXT("Size_BossRecord"),nullptr,0,100); BP->WidgetTree->RootWidget=Root;
		UHorizontalBox* RowWidget=Make<UHorizontalBox>(BP,TEXT("Row_BossRecord"));
		Root->AddChild(Button(BP,TEXT("Button_Select"),RowWidget,FMargin(18,11,12,11)));
		Row(RowWidget,OptionalImage(BP,TEXT("Size_Icon"),TEXT("Image_Icon"),48,48),0,FMargin(0,0,14,0));
		UVerticalBox* Labels=Make<UVerticalBox>(BP,TEXT("Column_Entry")); Row(RowWidget,Labels,1.f);
		Col(Labels,Text(BP,TEXT("Text_Name"),INVTEXT("몬스터"),18,true,false,true));
		UHorizontalBox* Lock=Make<UHorizontalBox>(BP,TEXT("Row_LockState"),true); Col(Labels,Lock,false,FMargin(0,4,0,0));
		Row(Lock,Text(BP,TEXT("Text_LockState"),INVTEXT("미해금"),13,false,true,true),1.f);
	}
	void BuildDrop(UWidgetBlueprint* BP)
	{
		UHorizontalBox* RowWidget=Make<UHorizontalBox>(BP,TEXT("Row_Drop"));
		BP->WidgetTree->RootWidget=Panel(BP,TEXT("Panel_DropRow"),RowWidget,.36f,FMargin(12,10));
		Row(RowWidget,OptionalImage(BP,TEXT("Size_ItemIcon"),TEXT("Image_Icon"),42,42),0,FMargin(0,0,14,0));
		UVerticalBox* Labels=Make<UVerticalBox>(BP,TEXT("Column_DropLabels")); Row(RowWidget,Labels,1.f);
		Col(Labels,Text(BP,TEXT("Text_Name"),INVTEXT("아이템"),16,true,false,true));
		Col(Labels,Text(BP,TEXT("Text_Details"),FText::GetEmpty(),13,false,true,true),false,FMargin(0,4,0,0));
	}
}

bool UKnightCodexEditorLibrary::BuildArchivePresentation(UWidgetBlueprint* BP, int32 Kind)
{
	if (!BP || !BP->WidgetTree || Kind<0 || Kind>2) return false;
	// These assets have native behavior. Refuse to discard authored animation/binding work.
	if (BP->Animations.Num() || BP->Bindings.Num()) return false;
	BP->Modify(); BP->WidgetTree->Modify();
	TArray<UWidget*> OldWidgets; BP->WidgetTree->GetAllWidgets(OldWidgets);
	// Retain real artwork authored directly on existing Image widgets as well as class defaults.
	TMap<FName,FSlateBrush> Artwork;
	for (UWidget* W : OldWidgets)
		if (const UImage* Image=Cast<UImage>(W); Image && Image->GetBrush().GetResourceObject())
			Artwork.Add(Image->GetFName(),Image->GetBrush());
	for (UWidget* W : OldWidgets) BP->OnVariableRemoved(W->GetFName());
	BP->WidgetTree->Rename(nullptr,GetTransientPackage(),REN_DontCreateRedirectors | REN_NonTransactional);
	BP->WidgetTree=NewObject<UWidgetTree>(BP,TEXT("WidgetTree"),RF_Transactional);
	if (Kind==0) CodexArchiveDesigner::BuildMain(BP);
	else if (Kind==1) CodexArchiveDesigner::BuildBoss(BP);
	else CodexArchiveDesigner::BuildDrop(BP);
	for (const TPair<FName,FSlateBrush>& Pair : Artwork)
		if (UImage* Image=Cast<UImage>(BP->WidgetTree->FindWidget(Pair.Key)))
		{
			Image->SetBrush(Pair.Value); Image->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UWidget* Container=Image->GetParent() ? Image->GetParent()->GetParent() : nullptr)
				Container->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return true;
}
