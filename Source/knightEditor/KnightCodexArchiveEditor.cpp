#include "KnightCodexEditorLibrary.h"

#include "Codex/KnightCodexPresentation.h"
#include "Codex/KnightCodexReveal.h"
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
#include "Components/ScrollBoxSlot.h"
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
		return P;
	}
	UWidget* SectionHeading(UWidgetBlueprint* BP,const TCHAR* Name,const FText& Value)
	{
		UHorizontalBox* R=Make<UHorizontalBox>(BP,*FString::Printf(TEXT("Row_%s"),Name));
		Row(R,Text(BP,Name,Value,21,true,false,true),1.f);
		return R;
	}
	UWidget* Reveal(UWidgetBlueprint* BP,const TCHAR* Name,UWidget* Content)
	{
		UKnightCodexReveal* Mask=Make<UKnightCodexReveal>(BP,Name);
		Mask->AddChild(Content); return Mask;
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
		W->SetAnimateWheelScrolling(false); W->SetWheelScrollMultiplier(1.3f);
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
		Col(Page,Text(BP,*FString::Printf(TEXT("Text_%sTitle"),Prefix),Title,28,true,false,true),false,FMargin(0,0,0,20));
		Col(Page,Rule(BP,*FString::Printf(TEXT("Rule_%sTitle"),Prefix)),false,FMargin(0,0,0,24));
		UVerticalBox* Body=Make<UVerticalBox>(BP,*FString::Printf(TEXT("Body_%s"),Prefix));
		Col(Body,OptionalImage(BP,*FString::Printf(TEXT("Size_%sIllustration"),Prefix),*FString::Printf(TEXT("Image_%sIllustration"),Prefix),0,280),false,FMargin(0,0,0,16));
		Col(Body,Text(BP,*FString::Printf(TEXT("Text_%sBody"),Prefix),NSLOCTEXT("KnightCodexEditor", "Archive01", "설명 준비 중"),19,false,false,true),false,FMargin(0,0,24,24));
		USizeBox* ReadingWidth=Size(BP,*FString::Printf(TEXT("Size_%sReadingMeasure"),Prefix),Body);
		ReadingWidth->SetMaxDesiredWidth(1000);
		UScrollBox* Reader=Scroll(BP,*FString::Printf(TEXT("Scroll_%s"),Prefix),ReadingWidth);
		CastChecked<UScrollBoxSlot>(ReadingWidth->Slot)->SetHorizontalAlignment(HAlign_Left);
		Col(Page,Reader,true);
		return Page;
	}
	void BuildMain(UWidgetBlueprint* BP)
	{
		UCanvasPanel* Root=Make<UCanvasPanel>(BP,TEXT("Canvas_Root")); BP->WidgetTree->RootWidget=Root;
		UBorder* Scrim=Make<UBorder>(BP,TEXT("Border_CodexScrim")); Scrim->SetBrushColor(Color(TEXT("020B13"),.78f));
		Scrim->SetVisibility(ESlateVisibility::HitTestInvisible);
		UCanvasPanelSlot* ScrimSlot=Root->AddChildToCanvas(Scrim); ScrimSlot->SetAnchors(FAnchors(0,0,1,1)); ScrimSlot->SetOffsets(FMargin());
		UVerticalBox* Shell=Make<UVerticalBox>(BP,TEXT("Column_CodexShell"));
		UCanvasPanelSlot* ShellSlot=Root->AddChildToCanvas(Shell); ShellSlot->SetAnchors(FAnchors(0,0,1,1));
		// Stretched canvas offsets are positive distances from the far edges, not negative positions.
		ShellSlot->SetOffsets(FMargin(32.f,24.f,32.f,28.f));
		UHorizontalBox* Header=Make<UHorizontalBox>(BP,TEXT("Row_CodexHeader")); Col(Shell,Header,false,FMargin(4,0,4,18));
		UHorizontalBox* Brand=Make<UHorizontalBox>(BP,TEXT("Row_Brand")); Row(Header,Size(BP,TEXT("Size_Brand"),Brand,460));
		UVerticalBox* Heading=Make<UVerticalBox>(BP,TEXT("Column_ArchiveHeading")); Row(Brand,Heading,1.f);
		UTextBlock* CodexLabel=Text(BP,TEXT("Label_ArchiveSubtitle"),FText::FromString(TEXT("CODEX")),17,false,true);
		FSlateFontInfo BrandFont=CodexLabel->GetFont(); BrandFont.LetterSpacing=300; CodexLabel->SetFont(BrandFont); Col(Heading,CodexLabel);
		Col(Heading,Text(BP,TEXT("Label_Codex"),NSLOCTEXT("KnightCodexEditor", "Archive02", "도감"),44,true));
		UHorizontalBox* Rail=Make<UHorizontalBox>(BP,TEXT("Row_Categories")); Row(Header,Rail,1.f);
		CastChecked<UHorizontalBoxSlot>(Rail->Slot)->SetVerticalAlignment(VAlign_Center);
		const TCHAR* Names[]={TEXT("World"),TEXT("Mech"),TEXT("Monster")};
		const FText Labels[]={NSLOCTEXT("KnightCodexEditor", "Archive05", "세계관"),NSLOCTEXT("KnightCodexEditor", "Archive06", "기체 설명"),NSLOCTEXT("KnightCodexEditor", "Archive07", "몬스터")};
		for(int32 Index=0;Index<3;++Index)
		{
			UVerticalBox* Tile=Make<UVerticalBox>(BP,*FString::Printf(TEXT("Tile_%s"),Names[Index]));
			UTextBlock* Label=Text(BP,*FString::Printf(TEXT("Label_Button_%s"),Names[Index]),Labels[Index],29,true);
			Label->SetJustification(ETextJustify::Center); Label->SetAutoWrapText(false); Col(Tile,Label);
			UKnightCodexArchiveButton* B=Button(BP,*FString::Printf(TEXT("Button_%s"),Names[Index]),Tile,FMargin(24,12,24,22));
			B->bCategoryTile=true;
			USizeBox* TileSize=Size(BP,*FString::Printf(TEXT("Size_Category_%s"),Names[Index]),B);
			TileSize->SetMinDesiredHeight(84.f); Row(Rail,TileSize,1.f,FMargin(12,0,12,0));
			if(Index<2)
			{
				UBorder* Divider=Make<UBorder>(BP,*FString::Printf(TEXT("TabSeparator_%d"),Index));
				Divider->SetBrushColor(Color(TEXT("8DB0C5"),.55f));
				USizeBox* D=Size(BP,*FString::Printf(TEXT("TabSeparatorSize_%d"),Index),Divider,1,36);
				Row(Rail,D); CastChecked<UHorizontalBoxSlot>(D->Slot)->SetVerticalAlignment(VAlign_Center);
			}
		}
		Row(Header,Size(BP,TEXT("Header_EndSpace"),nullptr,220));
		Col(Shell,Rule(BP,TEXT("Rule_Header")),false,FMargin(0,0,0,36));
		UHorizontalBox* Body=Make<UHorizontalBox>(BP,TEXT("Row_Main")); Col(Shell,Body,true);
		UVerticalBox* EntryColumn=Make<UVerticalBox>(BP,TEXT("Column_CodexEntries"));
		Col(EntryColumn,SectionHeading(BP,TEXT("Label_CodexEntries"),NSLOCTEXT("KnightCodexEditor", "Archive08", "세계관 기록")),false,FMargin(8,0,0,32));
		UWidgetSwitcher* Entries=Make<UWidgetSwitcher>(BP,TEXT("Switcher_Entries"),true); Col(EntryColumn,Entries,true);
		Entries->AddChild(Scroll(BP,TEXT("Scroll_WorldEntries"),Make<UVerticalBox>(BP,TEXT("Box_WorldEntries"))));
		Entries->AddChild(Scroll(BP,TEXT("Scroll_MechEntries"),Make<UVerticalBox>(BP,TEXT("Box_MechEntries"))));
		UVerticalBox* BossList=Make<UVerticalBox>(BP,TEXT("Column_BossList"));
		Col(BossList,Text(BP,TEXT("Text_NoBosses"),NSLOCTEXT("KnightCodexEditor", "Archive09", "등록된 몬스터가 없습니다."),16,false,true,true));
		Col(BossList,Make<UVerticalBox>(BP,TEXT("Box_Bosses"),true));
		Entries->AddChild(Scroll(BP,TEXT("Scroll_BossList"),BossList));
		Row(Body,Size(BP,TEXT("Size_EntryList"),Panel(BP,TEXT("Panel_EntryList"),EntryColumn,.12f,FMargin(0,0,0,12)),292),0,FMargin(0,0,20,0));
		UWidgetSwitcher* Content=Make<UWidgetSwitcher>(BP,TEXT("Switcher_Content"),true);
		Content->AddChild(Reveal(BP,TEXT("Reveal_World"),Panel(BP,TEXT("Panel_WorldReader"),TextPage(BP,TEXT("World"),NSLOCTEXT("KnightCodexEditor", "Archive10", "세계관")),.28f,FMargin(36,12,36,20))));
		Content->AddChild(Reveal(BP,TEXT("Reveal_Mech"),Panel(BP,TEXT("Panel_MechReader"),TextPage(BP,TEXT("Mech"),NSLOCTEXT("KnightCodexEditor", "Archive11", "기체 설명")),.28f,FMargin(36,12,36,20))));
		UHorizontalBox* MonsterPage=Make<UHorizontalBox>(BP,TEXT("Row_MonsterPage")); Content->AddChild(MonsterPage);
		// The full illustration owns its column, outside the independently scrolling information.
		// Its baked-in frame and labels need neither an additional frame nor overlay text.
		UOverlay* Art=Make<UOverlay>(BP,TEXT("Panel_Illustration"));
		UBorder* ArtSurface=Make<UBorder>(BP,TEXT("Surface_Illustration"));
		ArtSurface->SetBrushColor(Color(TEXT("030D16"),0.f)); ArtSurface->SetVisibility(ESlateVisibility::HitTestInvisible);
		Overlay(Art,ArtSurface);
		Overlay(Art,OptionalImage(BP,TEXT("Size_Portrait"),TEXT("Image_Portrait"),0,0));
		UTextBlock* NoArt=Text(BP,TEXT("Text_NoPortrait"),NSLOCTEXT("KnightCodexEditor","NoIllustration","미해금 기록"),18,false,true,true);
		UOverlaySlot* NoArtSlot=Art->AddChildToOverlay(NoArt);
		NoArtSlot->SetHorizontalAlignment(HAlign_Center); NoArtSlot->SetVerticalAlignment(VAlign_Center);
		Row(MonsterPage,Reveal(BP,TEXT("Reveal_Monster"),Art),1.f,FMargin(0,0,24,0));
		UVerticalBox* Info=Make<UVerticalBox>(BP,TEXT("Column_MonsterInfo"));
		UTextBlock* AccessibleName=Text(BP,TEXT("Text_BossName"),FText::GetEmpty(),18,true,false,true);
		AccessibleName->SetVisibility(ESlateVisibility::Collapsed); Col(Info,AccessibleName);
		Col(Info,SectionHeading(BP,TEXT("Label_Observation"),NSLOCTEXT("KnightCodexEditor","Observation","관찰 기록")),false,FMargin(0,0,0,18));
		UVerticalBox* Observation=Make<UVerticalBox>(BP,TEXT("Column_Observation"));
		Col(Observation,Text(BP,TEXT("Text_BossDescription"),NSLOCTEXT("KnightCodexEditor", "Archive13", "설명 준비 중"),17,false,false,true),false,FMargin(0,0,6,16));
		Col(Observation,Text(BP,TEXT("Text_BossExtra"),FText::GetEmpty(),16,false,true,true),false,FMargin(0,0,6,16));
		Col(Info,Size(BP,TEXT("Size_Observation"),Scroll(BP,TEXT("Scroll_BossObservation"),Observation),0,204));
		Col(Info,Rule(BP,TEXT("Rule_MonsterTitle")),false,FMargin(0,22,0,22));
		Col(Info,SectionHeading(BP,TEXT("Label_Drops"),NSLOCTEXT("KnightCodexEditor", "Materials", "획득 소재")),false,FMargin(0,0,0,18));
		UVerticalBox* MonsterBody=Make<UVerticalBox>(BP,TEXT("Column_BossDetail"));
		Col(MonsterBody,Make<UVerticalBox>(BP,TEXT("Box_Drops"),true));
		Col(MonsterBody,Text(BP,TEXT("Text_NoDrops"),NSLOCTEXT("KnightCodexEditor", "Archive15", "등록된 드랍 정보가 없습니다."),15,false,true,true));
		Col(Info,Scroll(BP,TEXT("Scroll_BossDetail"),MonsterBody),true);
		Row(MonsterPage,Size(BP,TEXT("Size_MonsterInfo"),Panel(BP,TEXT("Panel_Reader"),Info,.22f,FMargin(16,0,0,12)),360));
		Row(Body,Content,1.f);
		Col(Shell,Rule(BP,TEXT("Rule_Footer")),false,FMargin(0,28,0,12));
		UHorizontalBox* Footer=Make<UHorizontalBox>(BP,TEXT("Row_ArchiveFooter")); Col(Shell,Footer,false);
		Row(Footer,Make<USpacer>(BP,TEXT("Footer_Space")),1.f);
		UTextBlock* CloseLabel=Text(BP,TEXT("Label_Button_Close"),NSLOCTEXT("KnightCodexEditor", "FooterReturn", "ESC    돌아가기"),18);
		CloseLabel->SetJustification(ETextJustify::Center);
		CloseLabel->SetAutoWrapText(false);
		UKnightCodexArchiveButton* Back=Button(BP,TEXT("Button_Close"),CloseLabel,FMargin(18,8)); Back->bTextOnly=true;
		Row(Footer,Size(BP,TEXT("Size_Close"),Back,220,48));
	}
	void BuildBoss(UWidgetBlueprint* BP)
	{
		USizeBox* Root=Size(BP,TEXT("Size_BossRecord"),nullptr); Root->SetMinDesiredHeight(116); BP->WidgetTree->RootWidget=Root;
		UHorizontalBox* RowWidget=Make<UHorizontalBox>(BP,TEXT("Row_BossRecord"));
		Root->AddChild(Button(BP,TEXT("Button_Select"),RowWidget,FMargin(14,10,12,10)));
		Row(RowWidget,OptionalImage(BP,TEXT("Size_Icon"),TEXT("Image_Icon"),88,88),0,FMargin(0,0,14,0));
		UVerticalBox* Labels=Make<UVerticalBox>(BP,TEXT("Column_Entry")); Row(RowWidget,Labels,1.f);
		CastChecked<UHorizontalBoxSlot>(Labels->Slot)->SetVerticalAlignment(VAlign_Center);
		UHorizontalBox* NameRow=Make<UHorizontalBox>(BP,TEXT("Row_BossName")); Col(Labels,NameRow);
		Row(NameRow,Text(BP,TEXT("Text_Ordinal"),FText::GetEmpty(),16,false,true,true),0,FMargin(0,0,10,0));
		Row(NameRow,Text(BP,TEXT("Text_Name"),NSLOCTEXT("KnightCodexEditor", "Archive18", "몬스터"),20,true,false,true),1.f);
		UHorizontalBox* Lock=Make<UHorizontalBox>(BP,TEXT("Row_LockState"),true); Col(Labels,Lock,false,FMargin(0,4,0,0));
		Row(Lock,Text(BP,TEXT("Text_LockState"),NSLOCTEXT("KnightCodexEditor", "Archive19", "미해금"),13,false,true,true),1.f);
	}
	void BuildDrop(UWidgetBlueprint* BP)
	{
		UHorizontalBox* RowWidget=Make<UHorizontalBox>(BP,TEXT("Row_Drop"));
		UVerticalBox* Root=Make<UVerticalBox>(BP,TEXT("Column_DropRow")); BP->WidgetTree->RootWidget=Root;
		Col(Root,Rule(BP,TEXT("Rule_Drop")),false,FMargin(0,0,0,12));
		Col(Root,RowWidget,false,FMargin(0,0,0,16));
		Row(RowWidget,OptionalImage(BP,TEXT("Size_ItemIcon"),TEXT("Image_Icon"),64,64),0,FMargin(0,0,16,0));
		UVerticalBox* Labels=Make<UVerticalBox>(BP,TEXT("Column_DropLabels")); Row(RowWidget,Labels,1.f);
		Col(Labels,Text(BP,TEXT("Text_Name"),NSLOCTEXT("KnightCodexEditor", "Archive20", "아이템"),17,true,false,true));
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
