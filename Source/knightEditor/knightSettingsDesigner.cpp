#include "knightUIEditorLibrary.h"
#include "Screens/knightSettingsScreen.h"
#include "Widgets/knightSettingRow.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Brushes/SlateNoResource.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/Spacer.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "WidgetBlueprint.h"

namespace KnightSettingsDesigner
{
    FLinearColor Ink(uint8 R,uint8 G,uint8 B,float A=1.f) { return FLinearColor(FColor(R,G,B)).CopyWithNewOpacity(A); }
    template<class T> T* Make(UWidgetBlueprint* BP,const TCHAR* Name,bool Variable=false,UClass* Class=T::StaticClass())
    {
        T* W=BP->WidgetTree->ConstructWidget<T>(Class,Name); W->bIsVariable=Variable; BP->OnVariableAdded(Name); return W;
    }
    UCommonTextBlock* Text(UWidgetBlueprint* BP,const TCHAR* Name,const FText& Value,int32 FontSize,bool Bold=false,bool Variable=false)
    {
        auto* W=Make<UCommonTextBlock>(BP,Name,Variable); W->SetText(Value); W->SetStyle(nullptr);
        auto* Font=LoadObject<UFont>(nullptr,TEXT("/Game/Knight/UI/Style/Fonts/F_Knight_PlexKR.F_Knight_PlexKR"));
        W->SetFont(FSlateFontInfo(Font,FontSize,Bold?TEXT("SemiBold"):TEXT("Regular")));
        W->SetColorAndOpacity(Ink(226,237,244)); return W;
    }
    void Col(UVerticalBox* P,UWidget* W,bool Fill=false,FMargin Pad=FMargin())
    { auto* S=P->AddChildToVerticalBox(W); S->SetSize(FSlateChildSize(Fill?ESlateSizeRule::Fill:ESlateSizeRule::Automatic)); S->SetPadding(Pad); }
    void Row(UHorizontalBox* P,UWidget* W,bool Fill=false,FMargin Pad=FMargin())
    { auto* S=P->AddChildToHorizontalBox(W); S->SetSize(FSlateChildSize(Fill?ESlateSizeRule::Fill:ESlateSizeRule::Automatic)); S->SetPadding(Pad); S->SetVerticalAlignment(VAlign_Center); }
    void Over(UOverlay* P,UWidget* W,EHorizontalAlignment H=HAlign_Fill,EVerticalAlignment V=VAlign_Fill,FMargin Pad=FMargin())
    { auto* S=P->AddChildToOverlay(W); S->SetHorizontalAlignment(H); S->SetVerticalAlignment(V); S->SetPadding(Pad); }
    USizeBox* Size(UWidgetBlueprint* BP,const TCHAR* Name,UWidget* Child,float Width=0,float Height=0)
    { auto* S=Make<USizeBox>(BP,Name); if(Width>0)S->SetWidthOverride(Width); if(Height>0)S->SetHeightOverride(Height); if(Child)S->AddChild(Child); return S; }
    FSlateBrush Brush(FLinearColor Color,FVector2D Extent=FVector2D(1,1))
    { FSlateBrush B; B.DrawAs=ESlateBrushDrawType::Image; B.TintColor=Color; B.ImageSize=Extent; return B; }
    UImage* Line(UWidgetBlueprint* BP,const TCHAR* Name,FLinearColor Color,FVector2D Extent=FVector2D(1,1),bool Variable=false)
    { auto* I=Make<UImage>(BP,Name,Variable); I->SetBrush(Brush(Color,Extent)); I->SetVisibility(ESlateVisibility::HitTestInvisible); return I; }
    UBorder* State(UWidgetBlueprint* BP)
    { auto* B=Make<UBorder>(BP,TEXT("Border_State")); B->SetPadding(FMargin()); B->SetBrushColor(FLinearColor::Transparent); B->SetVisibility(ESlateVisibility::HitTestInvisible); return B; }
    void Ellipsis(UCommonTextBlock* T) { T->SetAutoWrapText(false); T->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis); T->SetClipping(EWidgetClipping::ClipToBounds); }
    UButton* Arrow(UWidgetBlueprint* BP,const TCHAR* Name,const TCHAR* Glyph)
    {
        auto* B=Make<UButton>(BP,Name,true); FButtonStyle S;
        S.SetNormal(FSlateNoResource()).SetHovered(FSlateNoResource()).SetPressed(FSlateNoResource()).SetDisabled(FSlateNoResource());
        S.SetNormalPadding(FMargin()).SetPressedPadding(FMargin()); B->SetStyle(S);
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        B->IsFocusable=false;
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
        auto* T=Text(BP,*FString::Printf(TEXT("%s_Glyph"),Name),FText::FromString(Glyph),26);
        T->SetJustification(ETextJustify::Center); B->AddChild(Size(BP,*FString::Printf(TEXT("%s_Size"),Name),T,42,36)); return B;
    }
    void BuildRow(UWidgetBlueprint* BP,int32 Kind)
    {
        auto* Root=Make<UOverlay>(BP,TEXT("Overlay_Root")); BP->WidgetTree->RootWidget=Root; Over(Root,State(BP));
        Over(Root,Line(BP,TEXT("Img_Accent"),Ink(72,209,240),FVector2D(4,1),true),HAlign_Left);
        auto* H=Make<UHorizontalBox>(BP,TEXT("HorizontalBox_Row"));
        Over(Root,Size(BP,TEXT("SizeBox_Height"),H,0,42),HAlign_Fill,VAlign_Center,FMargin(28,0,10,0));
        auto* Label=Text(BP,TEXT("Txt_Label"),NSLOCTEXT("knightUI","SettingRowPlaceholder","항목"),20,false,true); Ellipsis(Label); Row(H,Label,true);
        Row(H,Line(BP,TEXT("Img_Dirty"),Ink(110,191,220),FVector2D(4,4),true),false,FMargin(12,0,12,0));
        auto* Control=Size(BP,TEXT("SizeBox_Control"),nullptr,360); Row(H,Control);
        if(Kind==4)
        {
            auto* Sliders=Make<UHorizontalBox>(BP,TEXT("HorizontalBox_Slider")); Control->AddChild(Sliders);
            auto* Slider=Make<USlider>(BP,TEXT("Slider_Value"),true); FSliderStyle S;
            S.SetNormalBarImage(Brush(Ink(70,110,133))).SetHoveredBarImage(Brush(Ink(117,183,210))).SetDisabledBarImage(Brush(Ink(60,78,90)));
            S.SetNormalThumbImage(Brush(Ink(149,221,240),FVector2D(5,17))).SetHoveredThumbImage(Brush(Ink(202,244,255),FVector2D(5,17))).SetDisabledThumbImage(Brush(Ink(100,120,130),FVector2D(5,17))).SetBarThickness(2);
            Slider->SetWidgetStyle(S); Slider->IsFocusable=false; Row(Sliders,Slider,true,FMargin(8,0,10,0));
            auto* Value=Text(BP,TEXT("Txt_Value"),FText::GetEmpty(),20,false,true); Value->SetJustification(ETextJustify::Right); Row(Sliders,Size(BP,TEXT("SizeBox_Value"),Value,62));
        }
        else
        {
            auto* Rotator=Make<UHorizontalBox>(BP,TEXT("HorizontalBox_Control")); Control->AddChild(Rotator);
            if(Kind==3)Row(Rotator,Arrow(BP,TEXT("Btn_Prev"),TEXT("‹")));
            auto* Value=Text(BP,TEXT("Txt_Value"),FText::GetEmpty(),20,false,true); Ellipsis(Value); Value->SetJustification(ETextJustify::Left); Row(Rotator,Value,true);
            // Slate disables single-line ellipsis for center-justified text. Center the
            // widget in the fixed value column instead; long text can then ellipsize.
            CastChecked<UHorizontalBoxSlot>(Value->Slot)->SetHorizontalAlignment(HAlign_Center);
            if(Kind==3)Row(Rotator,Arrow(BP,TEXT("Btn_Next"),TEXT("›")));
        }
        Over(Root,Line(BP,TEXT("Rule_Row"),Ink(66,105,126,.5f)),HAlign_Fill,VAlign_Bottom,FMargin(28,0,10,0));
    }
    void BuildTab(UWidgetBlueprint* BP)
    {
        auto* Root=Make<UOverlay>(BP,TEXT("Overlay_Root")); BP->WidgetTree->RootWidget=Root;
        auto* Label=Text(BP,TEXT("Txt_Label"),FText::GetEmpty(),22,true,true); Label->SetJustification(ETextJustify::Center);
        Over(Root,Size(BP,TEXT("Size_Tab"),Label,0,74),HAlign_Fill,VAlign_Center,FMargin(20,20,20,0));
        Over(Root,Line(BP,TEXT("Img_Selected"),Ink(65,210,240),FVector2D(1,3),true),HAlign_Fill,VAlign_Bottom,FMargin(20,0));
    }
    void BuildSection(UWidgetBlueprint* BP)
    {
        auto* H=Make<UHorizontalBox>(BP,TEXT("Row_Section")); BP->WidgetTree->RootWidget=H;
        auto* Label=Text(BP,TEXT("Txt_Caption"),FText::GetEmpty(),22,true,true); Label->SetColorAndOpacity(Ink(77,208,235));
        Row(H,Size(BP,TEXT("Size_SectionTitle"),Label,0,42),false,FMargin(0,0,22,0));
        Row(H,Line(BP,TEXT("Rule_Section"),Ink(75,143,170)),true,FMargin(0,12,6,0));
    }
    UknightButtonBase* Action(UWidgetBlueprint* BP,const TCHAR* Name,const FText& Label)
    {
        auto* B=Make<UknightButtonBase>(BP,Name,true,LoadClass<UknightButtonBase>(nullptr,TEXT("/Game/Knight/UI/Screens/Settings/WBP_KnightSettingsAction.WBP_KnightSettingsAction_C")));
        B->SetButtonText(Label); return B;
    }
    void StyleScroll(UScrollBox* Scroll)
    {
        FScrollBoxStyle S=Scroll->GetWidgetStyle();
        S.SetTopShadowBrush(FSlateNoResource()).SetBottomShadowBrush(FSlateNoResource()).SetLeftShadowBrush(FSlateNoResource()).SetRightShadowBrush(FSlateNoResource());
        Scroll->SetWidgetStyle(S);
        FScrollBarStyle Bar; Bar.SetThickness(3.f);
        Bar.SetHorizontalBackgroundImage(FSlateNoResource()).SetVerticalBackgroundImage(FSlateNoResource());
        Bar.SetVerticalTopSlotImage(FSlateNoResource()).SetVerticalBottomSlotImage(FSlateNoResource());
        Bar.SetHorizontalTopSlotImage(FSlateNoResource()).SetHorizontalBottomSlotImage(FSlateNoResource());
        Bar.SetNormalThumbImage(Brush(Ink(65,113,139))).SetHoveredThumbImage(Brush(Ink(100,176,204))).SetDraggedThumbImage(Brush(Ink(119,203,229)));
        Scroll->SetWidgetBarStyle(Bar); Scroll->SetScrollbarThickness(FVector2D(3,3)); Scroll->SetScrollbarPadding(FMargin());
        Scroll->SetAnimateWheelScrolling(false);
    }
    void BuildScreen(UWidgetBlueprint* BP)
    {
        auto* Root=Make<UOverlay>(BP,TEXT("Overlay_Root")); BP->WidgetTree->RootWidget=Root;
        auto* Backdrop=Line(BP,TEXT("Img_Backdrop"),FLinearColor::White); Backdrop->SetColorAndOpacity(Ink(3,13,21,.78f)); Over(Root,Backdrop);
        auto* Page=Make<UVerticalBox>(BP,TEXT("Page_Settings")); Over(Root,Page,HAlign_Fill,VAlign_Fill,FMargin(80,30,80,46));
        auto* Editor=Make<UVerticalBox>(BP,TEXT("Panel_Editor"),true); Col(Page,Editor,true);
        auto* Header=Make<UHorizontalBox>(BP,TEXT("Row_Header")); Col(Editor,Header);
        auto* Title=Make<UVerticalBox>(BP,TEXT("Column_Title")); Row(Header,Size(BP,TEXT("Size_Title"),Title,380));
        auto* Caption=Text(BP,TEXT("Txt_Caption"),INVTEXT("SETTINGS"),15); Caption->SetColorAndOpacity(Ink(164,210,233)); Col(Title,Caption);
        Col(Title,Text(BP,TEXT("Txt_Title"),NSLOCTEXT("knightUI","SettingsTitle","설정"),44,true),false,FMargin(0,6,0,0));
        auto* Tabs=Make<UHorizontalBox>(BP,TEXT("Row_TopTabs")); Row(Header,Tabs,true);
        UClass* TabClass=LoadClass<UknightSettingsTab>(nullptr,TEXT("/Game/Knight/UI/Screens/Settings/WBP_KnightSettingsTab.WBP_KnightSettingsTab_C"));
        for(int32 I=0;I<4;++I)Row(Tabs,Make<UknightSettingsTab>(BP,*FString::Printf(TEXT("Tab_%d"),I),true,TabClass),true);
        Row(Header,Size(BP,TEXT("Space_HeaderEnd"),nullptr,270));
        Col(Editor,Line(BP,TEXT("Rule_Header"),Ink(89,145,170)),false,FMargin(0,10,0,0));
        auto* Body=Make<UHorizontalBox>(BP,TEXT("Row_Body")); Col(Editor,Body,true,FMargin(36,26,18,0));
        auto* Scroll=Make<UScrollBox>(BP,TEXT("Scroll_Rows"),true); Scroll->SetNavigationDestination(EDescendantScrollDestination::IntoView);
        StyleScroll(Scroll);
        Scroll->AddChild(Make<UVerticalBox>(BP,TEXT("Box_Rows"),true));
        auto* ListSlot=Body->AddChildToHorizontalBox(Scroll); ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); ListSlot->SetPadding(FMargin(0,0,34,0));
        Row(Body,Line(BP,TEXT("Rule_Detail"),Ink(89,145,170)),false); CastChecked<UHorizontalBoxSlot>(Body->GetChildAt(1)->Slot)->SetVerticalAlignment(VAlign_Fill);
        auto* Detail=Make<UVerticalBox>(BP,TEXT("Column_Detail")); auto* DetailScroll=Make<UScrollBox>(BP,TEXT("Scroll_Detail")); DetailScroll->AddChild(Detail); StyleScroll(DetailScroll);
        auto* DS=Body->AddChildToHorizontalBox(Size(BP,TEXT("Size_Detail"),DetailScroll,420)); DS->SetPadding(FMargin(44,0,0,0)); DS->SetVerticalAlignment(VAlign_Fill);
        auto* DC=Text(BP,TEXT("Txt_DetailCaption"),NSLOCTEXT("knightUI","SettingsExplanation","설정 설명"),20,true); DC->SetColorAndOpacity(Ink(102,201,228)); Col(Detail,DC);
        auto* DetailTitle=Text(BP,TEXT("Txt_DetailTitle"),FText::GetEmpty(),30,true,true); DetailTitle->SetAutoWrapText(true);
        Col(Detail,DetailTitle,false,FMargin(0,12,0,24));
        Col(Detail,Line(BP,TEXT("Rule_Description"),Ink(89,145,170)),false,FMargin(0,0,0,24));
        auto* DT=Text(BP,TEXT("Txt_DetailBody"),FText::GetEmpty(),22,false,true); DT->SetAutoWrapText(true); Col(Detail,DT);
        Col(Page,Line(BP,TEXT("Rule_Footer"),Ink(89,145,170)),false,FMargin(0,28,0,0));
        auto* Status=Make<UHorizontalBox>(BP,TEXT("Row_Status")); Col(Page,Status,false,FMargin(0,4,0,4));
        auto* ST=Text(BP,TEXT("Txt_Status"),FText::GetEmpty(),18,false,true); Ellipsis(ST); Row(Status,ST,true);
        auto* Dirty=Text(BP,TEXT("Txt_DirtyCount"),FText::GetEmpty(),18,false,true); Row(Status,Dirty);
        auto* Footer=Make<UHorizontalBox>(BP,TEXT("Panel_Footer"),true); Col(Page,Footer);
        Row(Footer,Size(BP,TEXT("Size_Defaults"),Action(BP,TEXT("Btn_Defaults"),NSLOCTEXT("knightUI","SettingsDefaults","기본값")),174,50));
        Row(Footer,Size(BP,TEXT("Size_Revert"),Action(BP,TEXT("Btn_Revert"),NSLOCTEXT("knightUI","SettingsRevert","되돌리기")),174,50),false,FMargin(20,0,0,0));
        Row(Footer,Make<USpacer>(BP,TEXT("Space_Footer")),true);
        Row(Footer,Size(BP,TEXT("Size_Apply"),Action(BP,TEXT("Btn_Apply"),NSLOCTEXT("knightUI","SettingsApply","적용")),188,50));
        Row(Footer,Size(BP,TEXT("Size_Back"),Action(BP,TEXT("Btn_Back"),NSLOCTEXT("knightUI","SettingsBack","ESC  돌아가기")),196,50),false,FMargin(42,0,0,0));
        auto* Prompt=Make<UHorizontalBox>(BP,TEXT("Panel_Prompt"),true); Col(Page,Prompt);
        Row(Prompt,Make<USpacer>(BP,TEXT("Space_Prompt")),true);
        for(const TPair<const TCHAR*,FText>& P : {TPair<const TCHAR*,FText>{TEXT("Btn_PromptContinue"),NSLOCTEXT("knightUI","SettingsPromptContinue","계속 편집")},{TEXT("Btn_PromptDiscard"),NSLOCTEXT("knightUI","SettingsPromptDiscard","버리고 닫기")},{TEXT("Btn_PromptApply"),NSLOCTEXT("knightUI","SettingsPromptApply","적용하고 닫기")}})
            Row(Prompt,Size(BP,*FString::Printf(TEXT("Size_%s"),P.Key),Action(BP,P.Key,P.Value),220,50),false,FMargin(16,0,0,0));
        Prompt->SetVisibility(ESlateVisibility::Collapsed);
    }
}

bool UknightUIEditorLibrary::RestyleSettings(UWidgetBlueprint* BP,int32 Kind)
{
    if(!BP || !BP->WidgetTree || Kind<0 || Kind>6 || BP->Animations.Num() || BP->Bindings.Num())return false;
    BP->Modify(); BP->WidgetTree->Modify(); TArray<UWidget*> Old; BP->WidgetTree->GetAllWidgets(Old);
    for(UWidget* W:Old)BP->OnVariableRemoved(W->GetFName());
    BP->WidgetTree->Rename(nullptr,GetTransientPackage(),REN_DontCreateRedirectors|REN_NonTransactional);
    BP->WidgetTree=NewObject<UWidgetTree>(BP,TEXT("WidgetTree"),RF_Transactional);
    if(Kind==0)KnightSettingsDesigner::BuildScreen(BP);
    else if(Kind==1)KnightSettingsDesigner::BuildTab(BP);
    else if(Kind==2)KnightSettingsDesigner::BuildSection(BP);
    else if(Kind==6)
    {
        using namespace KnightSettingsDesigner;
        auto* Root=Make<UOverlay>(BP,TEXT("Overlay_Root")); BP->WidgetTree->RootWidget=Root; Over(Root,State(BP));
        auto* Label=Text(BP,TEXT("Txt_Label"),FText::GetEmpty(),20,true,true); Label->SetJustification(ETextJustify::Center);
        Over(Root,Label,HAlign_Fill,VAlign_Center,FMargin(12,0));
    }
    else KnightSettingsDesigner::BuildRow(BP,Kind);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); return true;
}
