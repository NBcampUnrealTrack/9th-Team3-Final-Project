#include "knightUIEditorLibrary.h"

#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "WidgetBlueprint.h"
#include "Screens/knightSettingsScreen.h"
#include "Widgets/knightButtonBase.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

namespace
{
	// 스타일 가이드 색 (sRGB). FLinearColor(FColor)가 선형으로 바꾼다.
	const FLinearColor Navy900(FColor(0x10, 0x1A, 0x2A));
	const FLinearColor FrostDim(FColor(0x5F, 0x7F, 0xA8));
	const FLinearColor White(FColor(0xF4, 0xF8, 0xFC));
	const FLinearColor Frost(FColor(0xA8, 0xC8, 0xF0));
	const FLinearColor Ember(FColor(0xFF, 0x7A, 0x1A));

	/** 트리가 비어 있을 때만 작업을 시작한다. 디자이너에서 손댄 블루프린트는 덮어쓰지 않는다. */
	bool BeginBuild(UWidgetBlueprint* WidgetBlueprint, const TCHAR* Caller)
	{
		if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
		{
			return false;
		}
		if (WidgetBlueprint->WidgetTree->RootWidget)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: %s already has a layout; left unchanged."), Caller, *WidgetBlueprint->GetName());
			return false;
		}
		WidgetBlueprint->Modify();
		WidgetBlueprint->WidgetTree->Modify();
		return true;
	}

	void EndBuild(UWidgetBlueprint* WidgetBlueprint)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	}

	/**
	 * 위젯을 만들고 GUID를 등록한다. 위젯 블루프린트는 변수가 아닌 위젯도 모두 GUID가 있어야 하고, 빠지면 컴파일러가 ensure를 낸다.
	 * C++ 부모의 BindWidgetOptional이 찾을 위젯은 bVariable로 변수 표시를 한다.
	 */
	template<typename WidgetT>
	WidgetT* NewWidget(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<WidgetT> WidgetClass, const FName Name, bool bVariable)
	{
		WidgetT* const Widget = WidgetBlueprint->WidgetTree->ConstructWidget<WidgetT>(WidgetClass, Name);
		Widget->bIsVariable = bVariable;
		WidgetBlueprint->OnVariableAdded(Name);
		return Widget;
	}

	template<typename WidgetT>
	WidgetT* NewWidget(UWidgetBlueprint* WidgetBlueprint, const FName Name, bool bVariable = false)
	{
		return NewWidget<WidgetT>(WidgetBlueprint, WidgetT::StaticClass(), Name, bVariable);
	}

	UCommonTextBlock* NewText(UWidgetBlueprint* WidgetBlueprint, const FName Name, TSubclassOf<UCommonTextStyle> Style, const FText& Text, bool bVariable)
	{
		UCommonTextBlock* const Label = NewWidget<UCommonTextBlock>(WidgetBlueprint, Name, bVariable);
		Label->SetText(Text);
		if (Style)
		{
			Label->SetStyle(Style);
		}
		return Label;
	}

	FSlateBrush SolidBrush(const FLinearColor& Color, const FVector2D& Size = FVector2D(32.f, 32.f))
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.TintColor = FSlateColor(Color);
		Brush.ImageSize = Size;
		return Brush;
	}

	FSlateBrush NoBrush()
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
		return Brush;
	}

	/** 남색 반투명 바탕 + 1px 테두리. */
	FSlateBrush FrameBrush(float Opacity = 0.8f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor(Navy900.R, Navy900.G, Navy900.B, Opacity));
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(0.f, 0.f, 0.f, 0.f), FSlateColor(FrostDim), 1.f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return Brush;
	}

	/** 채움은 런타임에 SetFillColorAndOpacity로 칠하므로 흰색 단색. 테두리 안쪽으로 1px 들여 그린다. */
	UProgressBar* NewBar(UWidgetBlueprint* WidgetBlueprint, const FName Name, bool bWithFrame, float Percent)
	{
		UProgressBar* const Bar = NewWidget<UProgressBar>(WidgetBlueprint, Name, true);
		FProgressBarStyle Style;
		Style.SetBackgroundImage(bWithFrame ? FrameBrush() : NoBrush());
		Style.SetFillImage(SolidBrush(FLinearColor::White));
		Style.SetMarqueeImage(NoBrush());
		Bar->SetWidgetStyle(Style);
		Bar->SetBorderPadding(FVector2D(1.f, 1.f));
		Bar->SetPercent(Percent);
		return Bar;
	}

	USizeBox* NewSizeBox(UWidgetBlueprint* WidgetBlueprint, const FName Name, float Width, float Height)
	{
		USizeBox* const Box = NewWidget<USizeBox>(WidgetBlueprint, Name);
		if (Width > 0.f)
		{
			Box->SetWidthOverride(Width);
		}
		if (Height > 0.f)
		{
			Box->SetHeightOverride(Height);
		}
		return Box;
	}

	void AddToRow(UHorizontalBox* Row, UWidget* Widget, bool bFill, EHorizontalAlignment HAlign, const FMargin& Padding = FMargin())
	{
		UHorizontalBoxSlot* const Slot = Row->AddChildToHorizontalBox(Widget);
		Slot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
		Slot->SetHorizontalAlignment(HAlign);
		Slot->SetVerticalAlignment(VAlign_Bottom);
		Slot->SetPadding(Padding);
	}

	void AddToColumn(UVerticalBox* Column, UWidget* Widget, EHorizontalAlignment HAlign, const FMargin& Padding = FMargin())
	{
		UVerticalBoxSlot* const Slot = Column->AddChildToVerticalBox(Widget);
		Slot->SetHorizontalAlignment(HAlign);
		Slot->SetPadding(Padding);
	}

	void AddToOverlay(UOverlay* Overlay, UWidget* Widget, EHorizontalAlignment HAlign, EVerticalAlignment VAlign, const FMargin& Padding = FMargin())
	{
		UOverlaySlot* const Slot = Overlay->AddChildToOverlay(Widget);
		Slot->SetHorizontalAlignment(HAlign);
		Slot->SetVerticalAlignment(VAlign);
		Slot->SetPadding(Padding);
	}

	/**
	 * HUD용 단색 이미지. 마우스를 받지 않고, 픽셀 스냅을 끈다: 1080p보다 작은 창에서는 UI가 축소되어
	 * 1px 선이 1px보다 얇아지는데, 스냅이 켜져 있으면 위치에 따라 선이 통째로 사라진다.
	 */
	UImage* NewHUDImage(UWidgetBlueprint* WidgetBlueprint, const FName Name, const FLinearColor& Color, const FVector2D& Size, bool bVariable = false)
	{
		UImage* const Image = NewWidget<UImage>(WidgetBlueprint, Name, bVariable);
		Image->SetBrush(SolidBrush(FLinearColor::White, Size));
		Image->SetColorAndOpacity(Color);
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		Image->SetPixelSnapping(EWidgetPixelSnapping::Disabled);
		return Image;
	}

	/** 1080p 기준 세이프존 4.5% (86 x 49)보다 조금 안쪽. */
	const FVector2D HUDMargin(96.f, 64.f);
}

bool UknightUIEditorLibrary::BuildKnightButtonLayout(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> BracketClass, TSubclassOf<UCommonTextStyle> LabelStyle)
{
	if (!BracketClass || !BeginBuild(WidgetBlueprint, TEXT("BuildKnightButtonLayout")))
	{
		return false;
	}

	UOverlay* const Root = NewWidget<UOverlay>(WidgetBlueprint, TEXT("Overlay_Root"));
	WidgetBlueprint->WidgetTree->RootWidget = Root;

	// 버튼 스타일의 좌우 여백(8)과 합쳐 글자가 테두리에서 30px 떨어진다. 브래킷은 여백 8 안쪽 끝에 붙는다.
	AddToOverlay(Root, NewText(WidgetBlueprint, TEXT("Txt_Label"), LabelStyle, NSLOCTEXT("knightUI", "ButtonPlaceholder", "Button"), true),
		HAlign_Center, VAlign_Center, FMargin(22.f, 0.f));
	AddToOverlay(Root, NewWidget<UUserWidget>(WidgetBlueprint, BracketClass, TEXT("Bracket_L"), true), HAlign_Left, VAlign_Center);
	AddToOverlay(Root, NewWidget<UUserWidget>(WidgetBlueprint, BracketClass, TEXT("Bracket_R"), true), HAlign_Right, VAlign_Center);

	EndBuild(WidgetBlueprint);
	return true;
}

bool UknightUIEditorLibrary::BuildPrimaryLayout(UWidgetBlueprint* WidgetBlueprint)
{
	if (!BeginBuild(WidgetBlueprint, TEXT("BuildPrimaryLayout")))
	{
		return false;
	}

	UOverlay* const Root = NewWidget<UOverlay>(WidgetBlueprint, TEXT("Overlay_Root"));
	WidgetBlueprint->WidgetTree->RootWidget = Root;

	// 나중에 추가한 자식이 위에 그려진다: Game(HUD)이 맨 아래, Modal이 맨 위.
	for (const TCHAR* Name : { TEXT("Layer_Game"), TEXT("Layer_GameMenu"), TEXT("Layer_Menu"), TEXT("Layer_Modal") })
	{
		AddToOverlay(Root, NewWidget<UCommonActivatableWidgetStack>(WidgetBlueprint, Name, true), HAlign_Fill, VAlign_Fill);
	}

	EndBuild(WidgetBlueprint);
	return true;
}

bool UknightUIEditorLibrary::BuildHUDPlayerStatus(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> CaptionStyle, TSubclassOf<UCommonTextStyle> WarningStyle)
{
	if (!BeginBuild(WidgetBlueprint, TEXT("BuildHUDPlayerStatus")))
	{
		return false;
	}

	// 1080p 화면 폭의 약 27%.
	USizeBox* const Root = NewSizeBox(WidgetBlueprint, TEXT("SizeBox_Root"), 520.f, 0.f);
	WidgetBlueprint->WidgetTree->RootWidget = Root;
	UVerticalBox* const Column = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("VerticalBox_Panel"));
	Root->AddChild(Column);

	UHorizontalBox* const HealthRow = NewWidget<UHorizontalBox>(WidgetBlueprint, TEXT("HorizontalBox_HealthLabels"));
	AddToRow(HealthRow, NewText(WidgetBlueprint, TEXT("Txt_Name"), CaptionStyle, INVTEXT("KNIGHT"), false), true, HAlign_Left);
	AddToRow(HealthRow, NewText(WidgetBlueprint, TEXT("Txt_Health"), CaptionStyle, INVTEXT("1000 / 1000"), true), false, HAlign_Right);
	AddToColumn(Column, HealthRow, HAlign_Fill);

	USizeBox* const HealthBox = NewSizeBox(WidgetBlueprint, TEXT("SizeBox_Health"), 0.f, 14.f);
	UOverlay* const HealthBars = NewWidget<UOverlay>(WidgetBlueprint, TEXT("Overlay_HealthBars"));
	HealthBox->AddChild(HealthBars);
	// 고스트 바가 테두리와 바탕을 그리고, 그 위에 실제 체력 바를 겹친다.
	AddToOverlay(HealthBars, NewBar(WidgetBlueprint, TEXT("Bar_HealthGhost"), true, 1.f), HAlign_Fill, VAlign_Fill);
	AddToOverlay(HealthBars, NewBar(WidgetBlueprint, TEXT("Bar_Health"), false, 1.f), HAlign_Fill, VAlign_Fill);
	AddToColumn(Column, HealthBox, HAlign_Fill, FMargin(0.f, 4.f, 0.f, 0.f));

	UHorizontalBox* const BoostRow = NewWidget<UHorizontalBox>(WidgetBlueprint, TEXT("HorizontalBox_BoostLabels"));
	AddToRow(BoostRow, NewText(WidgetBlueprint, TEXT("Txt_BoostLabel"), CaptionStyle, INVTEXT("BOOST"), false), false, HAlign_Left);
	AddToRow(BoostRow, NewText(WidgetBlueprint, TEXT("Txt_Overheat"), WarningStyle, INVTEXT("OVERHEAT"), true), false, HAlign_Left, FMargin(12.f, 0.f, 0.f, 0.f));
	// 수치가 남은 폭을 채우고 오른쪽에 붙는다. OVERHEAT가 접혀도 위치가 그대로다.
	AddToRow(BoostRow, NewText(WidgetBlueprint, TEXT("Txt_Boost"), CaptionStyle, INVTEXT("100%"), true), true, HAlign_Right);
	AddToColumn(Column, BoostRow, HAlign_Fill, FMargin(0.f, 10.f, 0.f, 0.f));

	USizeBox* const BoostBox = NewSizeBox(WidgetBlueprint, TEXT("SizeBox_Boost"), 0.f, 9.f);
	BoostBox->AddChild(NewBar(WidgetBlueprint, TEXT("Bar_Boost"), true, 1.f));
	AddToColumn(Column, BoostBox, HAlign_Fill, FMargin(0.f, 4.f, 0.f, 0.f));

	EndBuild(WidgetBlueprint);
	return true;
}

bool UknightUIEditorLibrary::BuildHUDRaidTimer(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> TimeStyle, TSubclassOf<UCommonTextStyle> CaptionStyle)
{
	if (!BeginBuild(WidgetBlueprint, TEXT("BuildHUDRaidTimer")))
	{
		return false;
	}

	UVerticalBox* const Root = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("VerticalBox_Root"));
	WidgetBlueprint->WidgetTree->RootWidget = Root;
	AddToColumn(Root, NewText(WidgetBlueprint, TEXT("Txt_Time"), TimeStyle, INVTEXT("12:48"), true), HAlign_Right);
	AddToColumn(Root, NewText(WidgetBlueprint, TEXT("Txt_Caption"), CaptionStyle, INVTEXT("TIME LEFT"), true), HAlign_Right);

	EndBuild(WidgetBlueprint);
	return true;
}

bool UknightUIEditorLibrary::BuildHUDLockOnMarker(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> CaptionStyle, TSubclassOf<UCommonTextStyle> NameStyle)
{
	if (!BeginBuild(WidgetBlueprint, TEXT("BuildHUDLockOnMarker")))
	{
		return false;
	}

	UCanvasPanel* const Root = NewWidget<UCanvasPanel>(WidgetBlueprint, TEXT("Canvas_Root"));
	WidgetBlueprint->WidgetTree->RootWidget = Root;
	// 디자이너에서 보이도록 놓는 자리. 실행 중에는 C++가 대상 위치로 옮긴다.
	const FVector2D Preview(400.f, 300.f);

	// 주황 귀퉁이 4개(각 2px L자) + 가운데 마름모. 버튼의 포커스 브래킷과 같은 형태 언어.
	USizeBox* const Box = NewSizeBox(WidgetBlueprint, TEXT("Box"), 64.f, 64.f);
	Box->bIsVariable = true;
	UOverlay* const Corners = NewWidget<UOverlay>(WidgetBlueprint, TEXT("Overlay_Corners"));
	Box->AddChild(Corners);
	struct FCorner { const TCHAR* Name; EHorizontalAlignment H; EVerticalAlignment V; };
	for (const FCorner& Corner : { FCorner{ TEXT("TL"), HAlign_Left, VAlign_Top }, FCorner{ TEXT("TR"), HAlign_Right, VAlign_Top },
		FCorner{ TEXT("BL"), HAlign_Left, VAlign_Bottom }, FCorner{ TEXT("BR"), HAlign_Right, VAlign_Bottom } })
	{
		AddToOverlay(Corners, NewHUDImage(WidgetBlueprint, *FString::Printf(TEXT("Img_Corner%s_H"), Corner.Name), Ember, FVector2D(14.f, 2.f)), Corner.H, Corner.V);
		AddToOverlay(Corners, NewHUDImage(WidgetBlueprint, *FString::Printf(TEXT("Img_Corner%s_V"), Corner.Name), Ember, FVector2D(2.f, 14.f)), Corner.H, Corner.V);
	}
	UImage* const Diamond = NewHUDImage(WidgetBlueprint, TEXT("Img_Center"), Ember, FVector2D(5.f, 5.f));
	Diamond->SetRenderTransformAngle(45.f);
	AddToOverlay(Corners, Diamond, HAlign_Center, VAlign_Center);
	UCanvasPanelSlot* const BoxSlot = Root->AddChildToCanvas(Box);
	BoxSlot->SetAutoSize(true);
	BoxSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	BoxSlot->SetPosition(Preview);

	// 가지선: 길이는 C++의 DiagonalLength / ShelfLength가 매 프레임 덮어쓴다.
	UCanvasPanelSlot* const DiagonalSlot = Root->AddChildToCanvas(NewHUDImage(WidgetBlueprint, TEXT("Line_Diagonal"), Frost, FVector2D(48.f, 1.f), true));
	DiagonalSlot->SetSize(FVector2D(48.f, 1.f));
	DiagonalSlot->SetAlignment(FVector2D(0.f, 0.5f));
	DiagonalSlot->SetPosition(Preview + FVector2D(32.f, -32.f));
	UCanvasPanelSlot* const ShelfSlot = Root->AddChildToCanvas(NewHUDImage(WidgetBlueprint, TEXT("Line_Shelf"), Frost, FVector2D(150.f, 1.f), true));
	ShelfSlot->SetSize(FVector2D(150.f, 1.f));
	ShelfSlot->SetAlignment(FVector2D(0.f, 0.5f));
	ShelfSlot->SetPosition(Preview + FVector2D(56.f, -74.f));

	// 이름표: TARGET LOCK + 주황 눈금 + 몬스터 이름.
	UVerticalBox* const Plate = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("Plate"), true);
	Plate->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddToColumn(Plate, NewText(WidgetBlueprint, TEXT("Txt_LockCaption"), CaptionStyle, INVTEXT("TARGET LOCK"), false), HAlign_Left);
	UHorizontalBox* const NameRow = NewWidget<UHorizontalBox>(WidgetBlueprint, TEXT("HorizontalBox_Name"));
	UHorizontalBoxSlot* const TickSlot = NameRow->AddChildToHorizontalBox(NewHUDImage(WidgetBlueprint, TEXT("Img_PlateTick"), Ember, FVector2D(4.f, 20.f)));
	TickSlot->SetVerticalAlignment(VAlign_Center);
	TickSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	UHorizontalBoxSlot* const NameSlot = NameRow->AddChildToHorizontalBox(NewText(WidgetBlueprint, TEXT("Txt_TargetName"), NameStyle, INVTEXT("TARGET"), true));
	NameSlot->SetVerticalAlignment(VAlign_Center);
	AddToColumn(Plate, NameRow, HAlign_Left);
	UCanvasPanelSlot* const PlateSlot = Root->AddChildToCanvas(Plate);
	PlateSlot->SetAutoSize(true);
	PlateSlot->SetAlignment(FVector2D(0.f, 1.f));
	PlateSlot->SetPosition(Preview + FVector2D(64.f, -78.f));

	EndBuild(WidgetBlueprint);
	return true;
}

bool UknightUIEditorLibrary::BuildHUDLayout(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> PlayerStatusClass, TSubclassOf<UUserWidget> RaidTimerClass, TSubclassOf<UUserWidget> LockOnMarkerClass)
{
	if (!PlayerStatusClass || !RaidTimerClass || !LockOnMarkerClass || !BeginBuild(WidgetBlueprint, TEXT("BuildHUDLayout")))
	{
		return false;
	}

	UCanvasPanel* const Root = NewWidget<UCanvasPanel>(WidgetBlueprint, TEXT("Canvas_Root"));
	WidgetBlueprint->WidgetTree->RootWidget = Root;

	// 화면 전체를 덮고 맨 아래에 그려진다. 마커가 플레이어 패널이나 타이머를 가리지 않는다.
	UCanvasPanelSlot* const LockOnSlot = Root->AddChildToCanvas(NewWidget<UUserWidget>(WidgetBlueprint, LockOnMarkerClass, TEXT("LockOnMarker"), true));
	LockOnSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	LockOnSlot->SetOffsets(FMargin(0.f));

	UCanvasPanelSlot* const PlayerSlot = Root->AddChildToCanvas(NewWidget<UUserWidget>(WidgetBlueprint, PlayerStatusClass, TEXT("PlayerStatus"), true));
	PlayerSlot->SetAnchors(FAnchors(0.f, 0.f));
	PlayerSlot->SetAlignment(FVector2D(0.f, 0.f));
	PlayerSlot->SetPosition(HUDMargin);
	PlayerSlot->SetAutoSize(true);

	UCanvasPanelSlot* const TimerSlot = Root->AddChildToCanvas(NewWidget<UUserWidget>(WidgetBlueprint, RaidTimerClass, TEXT("RaidTimer"), true));
	TimerSlot->SetAnchors(FAnchors(1.f, 0.f));
	TimerSlot->SetAlignment(FVector2D(1.f, 0.f));
	TimerSlot->SetPosition(FVector2D(-HUDMargin.X, HUDMargin.Y));
	TimerSlot->SetAutoSize(true);

	// 조준점: 24px 십자, 1px 선.
	USizeBox* const Reticle = NewSizeBox(WidgetBlueprint, TEXT("Reticle"), 24.f, 24.f);
	UOverlay* const Cross = NewWidget<UOverlay>(WidgetBlueprint, TEXT("Overlay_Reticle"));
	Reticle->AddChild(Cross);
	const FLinearColor ReticleColor(White.R, White.G, White.B, 0.8f);
	AddToOverlay(Cross, NewHUDImage(WidgetBlueprint, TEXT("Img_ReticleH"), ReticleColor, FVector2D(24.f, 1.f)), HAlign_Center, VAlign_Center);
	AddToOverlay(Cross, NewHUDImage(WidgetBlueprint, TEXT("Img_ReticleV"), ReticleColor, FVector2D(1.f, 24.f)), HAlign_Center, VAlign_Center);

	UCanvasPanelSlot* const ReticleSlot = Root->AddChildToCanvas(Reticle);
	ReticleSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	ReticleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	ReticleSlot->SetPosition(FVector2D::ZeroVector);
	ReticleSlot->SetAutoSize(true);

	EndBuild(WidgetBlueprint);
	return true;
}

bool UknightUIEditorLibrary::BuildConfirmModal(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> ButtonClass, TSubclassOf<UCommonButtonStyle> CancelButtonStyle,
	TSubclassOf<UCommonTextStyle> TitleStyle, TSubclassOf<UCommonTextStyle> BodyStyle)
{
	if (!ButtonClass || !ButtonClass->IsChildOf<UknightButtonBase>() || !BeginBuild(WidgetBlueprint, TEXT("BuildConfirmModal")))
	{
		return false;
	}

	UOverlay* const Root = NewWidget<UOverlay>(WidgetBlueprint, TEXT("Overlay_Root"));
	WidgetBlueprint->WidgetTree->RootWidget = Root;

	// 화면 전체를 어둡게 덮고 클릭을 막는다. 바깥을 눌러도 닫히지 않는다: 되돌릴 수 없는 행동이라 답을 꼭 고르게 한다.
	UImage* const Dim = NewWidget<UImage>(WidgetBlueprint, TEXT("Img_Dim"));
	Dim->SetBrush(SolidBrush(FLinearColor::White));
	Dim->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	AddToOverlay(Root, Dim, HAlign_Fill, VAlign_Fill);

	// Panel은 C++가 등장 연출에서 크기를 키운다.
	USizeBox* const Panel = NewSizeBox(WidgetBlueprint, TEXT("Panel"), 560.f, 0.f);
	Panel->bIsVariable = true;
	AddToOverlay(Root, Panel, HAlign_Center, VAlign_Center);

	// 메뉴 위에 뜨므로 HUD 패널보다 진하게.
	UBorder* const Frame = NewWidget<UBorder>(WidgetBlueprint, TEXT("Border_Frame"));
	Frame->SetBrush(FrameBrush(0.94f));
	Frame->SetPadding(FMargin(32.f, 28.f, 32.f, 24.f));
	Panel->AddChild(Frame);

	UVerticalBox* const Column = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("VerticalBox_Content"));
	Frame->SetContent(Column);
	AddToColumn(Column, NewHUDImage(WidgetBlueprint, TEXT("Img_Accent"), Frost, FVector2D(32.f, 2.f)), HAlign_Left, FMargin(0.f, 0.f, 0.f, 14.f));
	AddToColumn(Column, NewText(WidgetBlueprint, TEXT("Txt_Title"), TitleStyle, NSLOCTEXT("knightUI", "ConfirmTitlePlaceholder", "레이드 포기"), true), HAlign_Left);
	UCommonTextBlock* const Message = NewText(WidgetBlueprint, TEXT("Txt_Message"), BodyStyle,
		NSLOCTEXT("knightUI", "ConfirmMessagePlaceholder", "지금 포기하면 이번 레이드의 보상을 받을 수 없습니다.\n기지로 돌아가시겠습니까?"), true);
	Message->SetAutoWrapText(true);
	AddToColumn(Column, Message, HAlign_Fill, FMargin(0.f, 12.f, 0.f, 0.f));

	// 오른쪽 아래 [취소] [확인]. 기본 포커스는 C++가 취소에 둔다.
	UHorizontalBox* const Buttons = NewWidget<UHorizontalBox>(WidgetBlueprint, TEXT("HorizontalBox_Buttons"));
	UknightButtonBase* const Cancel = NewWidget<UknightButtonBase>(WidgetBlueprint, TSubclassOf<UknightButtonBase>(ButtonClass), TEXT("Btn_Cancel"), true);
	Cancel->SetButtonText(NSLOCTEXT("knightUI", "ConfirmCancel", "취소"));
	if (CancelButtonStyle)
	{
		Cancel->SetStyle(CancelButtonStyle);
	}
	UknightButtonBase* const Confirm = NewWidget<UknightButtonBase>(WidgetBlueprint, TSubclassOf<UknightButtonBase>(ButtonClass), TEXT("Btn_Confirm"), true);
	Confirm->SetButtonText(NSLOCTEXT("knightUI", "ConfirmOk", "확인"));
	AddToRow(Buttons, Cancel, false, HAlign_Right);
	AddToRow(Buttons, Confirm, false, HAlign_Right, FMargin(12.f, 0.f, 0.f, 0.f));
	AddToColumn(Column, Buttons, HAlign_Right, FMargin(0.f, 28.f, 0.f, 0.f));

	EndBuild(WidgetBlueprint);
	return true;
}

bool UknightUIEditorLibrary::BuildPauseMenu(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> ButtonClass, TSubclassOf<UCommonButtonStyle> SecondaryButtonStyle,
	TSubclassOf<UCommonTextStyle> TitleStyle, TSubclassOf<UCommonTextStyle> CaptionStyle)
{
	if (!ButtonClass || !ButtonClass->IsChildOf<UknightButtonBase>() || !BeginBuild(WidgetBlueprint, TEXT("BuildPauseMenu")))
	{
		return false;
	}

	UOverlay* const Root = NewWidget<UOverlay>(WidgetBlueprint, TEXT("Overlay_Root"));
	WidgetBlueprint->WidgetTree->RootWidget = Root;

	// 확인창보다 옅게: 멈춘 전투 화면이 비쳐 보이도록.
	UImage* const Dim = NewWidget<UImage>(WidgetBlueprint, TEXT("Img_Dim"));
	Dim->SetBrush(SolidBrush(FLinearColor::White));
	Dim->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f));
	AddToOverlay(Root, Dim, HAlign_Fill, VAlign_Fill);

	// 왼쪽 세로 패널. 안쪽 왼쪽 여백 96은 HUD와 같은 세이프존.
	USizeBox* const Panel = NewSizeBox(WidgetBlueprint, TEXT("SizeBox_Panel"), 440.f, 0.f);
	AddToOverlay(Root, Panel, HAlign_Left, VAlign_Fill);
	UBorder* const Frame = NewWidget<UBorder>(WidgetBlueprint, TEXT("Border_Frame"));
	Frame->SetBrush(FrameBrush(0.88f));
	Frame->SetPadding(FMargin(HUDMargin.X, 0.f, 48.f, 0.f));
	Frame->SetVerticalAlignment(VAlign_Center);
	Frame->SetHorizontalAlignment(HAlign_Fill);
	Panel->AddChild(Frame);

	UVerticalBox* const Column = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("VerticalBox_Content"));
	Frame->SetContent(Column);
	AddToColumn(Column, NewText(WidgetBlueprint, TEXT("Txt_Caption"), CaptionStyle, INVTEXT("SYSTEM PAUSE"), false), HAlign_Left);
	AddToColumn(Column, NewHUDImage(WidgetBlueprint, TEXT("Img_Accent"), Frost, FVector2D(32.f, 2.f)), HAlign_Left, FMargin(0.f, 10.f, 0.f, 14.f));
	AddToColumn(Column, NewText(WidgetBlueprint, TEXT("Txt_Title"), TitleStyle, NSLOCTEXT("knightUI", "PauseTitle", "일시정지"), false), HAlign_Left);

	// 계속하기만 주 버튼. 나머지는 보조 스타일(뒤로 가기 소리).
	struct FEntry { const TCHAR* Name; FText Label; bool bPrimary; float Top; };
	for (const FEntry& Entry : {
		FEntry{ TEXT("Btn_Resume"), NSLOCTEXT("knightUI", "PauseResume", "계속하기"), true, 40.f },
		FEntry{ TEXT("Btn_Settings"), NSLOCTEXT("knightUI", "PauseSettings", "설정"), false, 12.f },
		FEntry{ TEXT("Btn_Abandon"), NSLOCTEXT("knightUI", "PauseAbandon", "레이드 포기"), false, 12.f } })
	{
		UknightButtonBase* const Button = NewWidget<UknightButtonBase>(WidgetBlueprint, TSubclassOf<UknightButtonBase>(ButtonClass), Entry.Name, true);
		Button->SetButtonText(Entry.Label);
		if (!Entry.bPrimary && SecondaryButtonStyle)
		{
			Button->SetStyle(SecondaryButtonStyle);
		}
		AddToColumn(Column, Button, HAlign_Fill, FMargin(0.f, Entry.Top, 0.f, 0.f));
	}

	AddToColumn(Column, NewText(WidgetBlueprint, TEXT("Txt_Hint"), CaptionStyle, NSLOCTEXT("knightUI", "PauseHint", "ESC   계속하기"), false), HAlign_Left, FMargin(0.f, 40.f, 0.f, 0.f));

	EndBuild(WidgetBlueprint);
	return true;
}


namespace
{
	/** 배경 없는 화살표 버튼. 포커스는 줄이 갖고 이 버튼은 마우스만 받는다. */
	UButton* NewArrowButton(UWidgetBlueprint* WidgetBlueprint, const FName Name, const FText& Glyph, TSubclassOf<UCommonTextStyle> Style)
	{
		UButton* const Button = NewWidget<UButton>(WidgetBlueprint, Name, true);
		FButtonStyle ButtonStyle;
		ButtonStyle.SetNormal(NoBrush());
		ButtonStyle.SetHovered(NoBrush());
		ButtonStyle.SetPressed(NoBrush());
		ButtonStyle.SetDisabled(NoBrush());
		ButtonStyle.SetNormalPadding(FMargin(8.f, 0.f));
		ButtonStyle.SetPressedPadding(FMargin(8.f, 0.f));
		Button->SetStyle(ButtonStyle);
		// Set before the Slate widget exists; there is no public setter for that.
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Button->IsFocusable = false;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		UCommonTextBlock* const Text = NewText(WidgetBlueprint, *(Name.ToString() + TEXT("_Glyph")), Style, Glyph, false);
		Text->SetColorAndOpacity(FrostDim);
		Button->AddChild(Text);
		return Button;
	}

	UHorizontalBoxSlot* AddCentered(UHorizontalBox* Box, UWidget* Widget, bool bFill, EVerticalAlignment VAlign = VAlign_Center, const FMargin& Padding = FMargin())
	{
		UHorizontalBoxSlot* const Slot = Box->AddChildToHorizontalBox(Widget);
		Slot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
		Slot->SetVerticalAlignment(VAlign);
		Slot->SetPadding(Padding);
		return Slot;
	}

	UImage* NewLine(UWidgetBlueprint* WidgetBlueprint, const FName Name, bool bVertical)
	{
		return NewHUDImage(WidgetBlueprint, Name, FrostDim, bVertical ? FVector2D(1.f, 8.f) : FVector2D(8.f, 1.f));
	}
}

namespace
{
	/** 바닥 키 안내 한 묶음: [키][키] 설명. 키는 1px 틀 안의 글자(키캡). */
	struct FKeyHint
	{
		TArray<FText> Keys;
		FText Label;
	};

	UHorizontalBox* NewKeyHints(UWidgetBlueprint* WidgetBlueprint, const FName Name, TSubclassOf<UCommonTextStyle> Style, const TArray<FKeyHint>& Hints)
	{
		UHorizontalBox* const Panel = NewWidget<UHorizontalBox>(WidgetBlueprint, Name, true);
		int32 Index = 0;
		for (const FKeyHint& Hint : Hints)
		{
			for (int32 KeyIndex = 0; KeyIndex < Hint.Keys.Num(); ++KeyIndex)
			{
				USizeBox* const Cap = NewSizeBox(WidgetBlueprint, *FString::Printf(TEXT("%s_Key%d_%d"), *Name.ToString(), Index, KeyIndex), 0.f, 24.f);
				Cap->SetMinDesiredWidth(24.f);
				UBorder* const Frame = NewWidget<UBorder>(WidgetBlueprint, *FString::Printf(TEXT("%s_Cap%d_%d"), *Name.ToString(), Index, KeyIndex));
				Frame->SetBrush(FrameBrush(0.f));
				Frame->SetPadding(FMargin(6.f, 0.f));
				Frame->SetHorizontalAlignment(HAlign_Center);
				Frame->SetVerticalAlignment(VAlign_Center);
				Frame->SetContent(NewText(WidgetBlueprint, *FString::Printf(TEXT("%s_KeyText%d_%d"), *Name.ToString(), Index, KeyIndex), Style, Hint.Keys[KeyIndex], false));
				Cap->AddChild(Frame);
				AddCentered(Panel, Cap, false, VAlign_Center, FMargin(Index > 0 && KeyIndex == 0 ? 20.f : (KeyIndex > 0 ? 4.f : 0.f), 0.f, 0.f, 0.f));
			}
			AddCentered(Panel, NewText(WidgetBlueprint, *FString::Printf(TEXT("%s_Label%d"), *Name.ToString(), Index), Style, Hint.Label, false), false, VAlign_Center, FMargin(8.f, 0.f, 0.f, 0.f));
			++Index;
		}
		return Panel;
	}
}

bool UknightUIEditorLibrary::BuildSettingRow(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> BracketClass, int32 ControlType,
	TSubclassOf<UCommonTextStyle> LabelStyle, TSubclassOf<UCommonTextStyle> ValueStyle)
{
	if (!BracketClass || !BeginBuild(WidgetBlueprint, TEXT("BuildSettingRow")))
	{
		return false;
	}

	UOverlay* const Root = NewWidget<UOverlay>(WidgetBlueprint, TEXT("Overlay_Root"));
	WidgetBlueprint->WidgetTree->RootWidget = Root;

	// 줄 높이: 이 28 + 버튼 스타일 위아래 여백.
	USizeBox* const Height = NewSizeBox(WidgetBlueprint, TEXT("SizeBox_Height"), 0.f, 28.f);
	AddToOverlay(Root, Height, HAlign_Fill, VAlign_Center, FMargin(12.f, 0.f));
	UHorizontalBox* const Row = NewWidget<UHorizontalBox>(WidgetBlueprint, TEXT("HorizontalBox_Row"));
	Height->AddChild(Row);

	AddCentered(Row, NewText(WidgetBlueprint, TEXT("Txt_Label"), LabelStyle, NSLOCTEXT("knightUI", "SettingRowPlaceholder", "항목"), true), true);
	// 적용하지 않은 변경 표시.
	AddCentered(Row, NewHUDImage(WidgetBlueprint, TEXT("Img_Dirty"), Frost, FVector2D(6.f, 6.f), true), false, VAlign_Center, FMargin(0.f, 0.f, 16.f, 0.f));

	USizeBox* const Control = NewSizeBox(WidgetBlueprint, TEXT("SizeBox_Control"), 320.f, 0.f);
	AddCentered(Row, Control, false);
	if (ControlType == 1)
	{
		UHorizontalBox* const SliderRow = NewWidget<UHorizontalBox>(WidgetBlueprint, TEXT("HorizontalBox_Slider"));
		Control->AddChild(SliderRow);
		USlider* const Slider = NewWidget<USlider>(WidgetBlueprint, TEXT("Slider_Value"), true);
		FSliderStyle SliderStyle;
		SliderStyle.SetNormalBarImage(SolidBrush(FrostDim, FVector2D(8.f, 1.f)));
		SliderStyle.SetHoveredBarImage(SolidBrush(Frost, FVector2D(8.f, 1.f)));
		SliderStyle.SetDisabledBarImage(SolidBrush(FrostDim, FVector2D(8.f, 1.f)));
		SliderStyle.SetNormalThumbImage(SolidBrush(White, FVector2D(4.f, 14.f)));
		SliderStyle.SetHoveredThumbImage(SolidBrush(Ember, FVector2D(4.f, 14.f)));
		SliderStyle.SetDisabledThumbImage(SolidBrush(FrostDim, FVector2D(4.f, 14.f)));
		SliderStyle.SetBarThickness(1.f);
		Slider->SetWidgetStyle(SliderStyle);
		Slider->IsFocusable = false;
		AddCentered(SliderRow, Slider, true);
		USizeBox* const ValueBox = NewSizeBox(WidgetBlueprint, TEXT("SizeBox_Value"), 56.f, 0.f);
		UCommonTextBlock* const Value = NewText(WidgetBlueprint, TEXT("Txt_Value"), ValueStyle, INVTEXT("80"), true);
		Value->SetJustification(ETextJustify::Right);
		ValueBox->AddChild(Value);
		AddCentered(SliderRow, ValueBox, false, VAlign_Center, FMargin(16.f, 0.f, 0.f, 0.f));
	}
	else if (ControlType == 2)
	{
		// 키는 1px 틀 안에.
		UBorder* const KeyFrame = NewWidget<UBorder>(WidgetBlueprint, TEXT("Border_Key"));
		KeyFrame->SetBrush(FrameBrush(0.f));
		KeyFrame->SetPadding(FMargin(12.f, 2.f));
		KeyFrame->SetHorizontalAlignment(HAlign_Center);
		KeyFrame->SetVerticalAlignment(VAlign_Center);
		KeyFrame->SetContent(NewText(WidgetBlueprint, TEXT("Txt_Value"), ValueStyle, INVTEXT("Space Bar"), true));
		Control->AddChild(KeyFrame);
	}
	else
	{
		UHorizontalBox* const Rotator = NewWidget<UHorizontalBox>(WidgetBlueprint, TEXT("HorizontalBox_Rotator"));
		Control->AddChild(Rotator);
		AddCentered(Rotator, NewArrowButton(WidgetBlueprint, TEXT("Btn_Prev"), INVTEXT("<"), ValueStyle), false);
		UCommonTextBlock* const Value = NewText(WidgetBlueprint, TEXT("Txt_Value"), ValueStyle, NSLOCTEXT("knightUI", "SettingValuePlaceholder", "켜기"), true);
		Value->SetJustification(ETextJustify::Center);
		AddCentered(Rotator, Value, true);
		AddCentered(Rotator, NewArrowButton(WidgetBlueprint, TEXT("Btn_Next"), INVTEXT(">"), ValueStyle), false);
	}

	AddToOverlay(Root, NewWidget<UUserWidget>(WidgetBlueprint, BracketClass, TEXT("Bracket_L"), true), HAlign_Left, VAlign_Center);
	AddToOverlay(Root, NewWidget<UUserWidget>(WidgetBlueprint, BracketClass, TEXT("Bracket_R"), true), HAlign_Right, VAlign_Center);

	EndBuild(WidgetBlueprint);
	return true;
}

bool UknightUIEditorLibrary::BuildSettingsTab(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> CaptionStyle, TSubclassOf<UCommonTextStyle> LabelStyle)
{
	if (!BeginBuild(WidgetBlueprint, TEXT("BuildSettingsTab")))
	{
		return false;
	}

	UOverlay* const Root = NewWidget<UOverlay>(WidgetBlueprint, TEXT("Overlay_Root"));
	WidgetBlueprint->WidgetTree->RootWidget = Root;

	// 선택 표시: 옅은 바탕 + 왼쪽 주황 세로선. C++가 선택된 탭에서만 보이게 한다.
	UOverlay* const Selected = NewWidget<UOverlay>(WidgetBlueprint, TEXT("Img_Selected"), true);
	AddToOverlay(Root, Selected, HAlign_Fill, VAlign_Fill);
	AddToOverlay(Selected, NewHUDImage(WidgetBlueprint, TEXT("Img_SelectedFill"), FLinearColor(Frost.R, Frost.G, Frost.B, 0.10f), FVector2D(8.f, 8.f)), HAlign_Fill, VAlign_Fill);
	AddToOverlay(Selected, NewHUDImage(WidgetBlueprint, TEXT("Img_SelectedBar"), Ember, FVector2D(2.f, 8.f)), HAlign_Left, VAlign_Fill);

	UVerticalBox* const Column = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("VerticalBox_Text"));
	AddToOverlay(Root, Column, HAlign_Fill, VAlign_Center, FMargin(16.f, 8.f));
	AddToColumn(Column, NewText(WidgetBlueprint, TEXT("Txt_Caption"), CaptionStyle, INVTEXT("AUDIO"), true), HAlign_Left);
	AddToColumn(Column, NewText(WidgetBlueprint, TEXT("Txt_Label"), LabelStyle, NSLOCTEXT("knightUI", "SettingsTabPlaceholder", "소리"), true), HAlign_Left, FMargin(0.f, 2.f, 0.f, 0.f));

	EndBuild(WidgetBlueprint);
	return true;
}

bool UknightUIEditorLibrary::BuildSettingsSectionHeader(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> CaptionStyle)
{
	if (!BeginBuild(WidgetBlueprint, TEXT("BuildSettingsSectionHeader")))
	{
		return false;
	}

	UBorder* const Band = NewWidget<UBorder>(WidgetBlueprint, TEXT("Border_Band"));
	Band->SetBrush(SolidBrush(FLinearColor(FrostDim.R, FrostDim.G, FrostDim.B, 0.25f)));
	Band->SetPadding(FMargin(12.f, 4.f));
	WidgetBlueprint->WidgetTree->RootWidget = Band;
	Band->SetContent(NewText(WidgetBlueprint, TEXT("Txt_Caption"), CaptionStyle, INVTEXT("VOLUME"), true));

	EndBuild(WidgetBlueprint);
	return true;
}

bool UknightUIEditorLibrary::BuildSettingsScreen(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> ButtonClass, TSubclassOf<UUserWidget> TabClass,
	TSubclassOf<UCommonButtonStyle> SecondaryButtonStyle, TSubclassOf<UCommonTextStyle> TitleStyle, TSubclassOf<UCommonTextStyle> SubheadStyle,
	TSubclassOf<UCommonTextStyle> BodyDimStyle, TSubclassOf<UCommonTextStyle> CaptionStyle)
{
	if (!ButtonClass || !ButtonClass->IsChildOf<UknightButtonBase>() || !TabClass || !TabClass->IsChildOf<UknightSettingsTab>()
		|| !BeginBuild(WidgetBlueprint, TEXT("BuildSettingsScreen")))
	{
		return false;
	}

	UOverlay* const Root = NewWidget<UOverlay>(WidgetBlueprint, TEXT("Overlay_Root"));
	WidgetBlueprint->WidgetTree->RootWidget = Root;

	// 뒤의 메뉴나 전투 화면이 살짝 비친다.
	UImage* const Backdrop = NewWidget<UImage>(WidgetBlueprint, TEXT("Img_Backdrop"));
	Backdrop->SetBrush(SolidBrush(FLinearColor::White));
	Backdrop->SetColorAndOpacity(FLinearColor(Navy900.R, Navy900.G, Navy900.B, 0.94f));
	AddToOverlay(Root, Backdrop, HAlign_Fill, VAlign_Fill);

	UVerticalBox* const Page = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("VerticalBox_Page"));
	AddToOverlay(Root, Page, HAlign_Fill, VAlign_Fill, FMargin(HUDMargin.X, HUDMargin.Y, HUDMargin.X, 48.f));

	// 머리.
	UHorizontalBox* const Header = NewWidget<UHorizontalBox>(WidgetBlueprint, TEXT("HorizontalBox_Header"));
	UVerticalBox* const TitleColumn = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("VerticalBox_Title"));
	AddToColumn(TitleColumn, NewText(WidgetBlueprint, TEXT("Txt_Caption"), CaptionStyle, INVTEXT("SYSTEM / CONFIG"), false), HAlign_Left);
	AddToColumn(TitleColumn, NewText(WidgetBlueprint, TEXT("Txt_Title"), TitleStyle, NSLOCTEXT("knightUI", "SettingsTitle", "설정"), false), HAlign_Left, FMargin(0.f, 4.f, 0.f, 0.f));
	AddCentered(Header, TitleColumn, true, VAlign_Bottom);
	AddCentered(Header, NewText(WidgetBlueprint, TEXT("Txt_DirtyCount"), CaptionStyle, NSLOCTEXT("knightUI", "SettingsDirtyPlaceholder", "적용하지 않은 변경 2"), true), false, VAlign_Bottom);
	AddToColumn(Page, Header, HAlign_Fill);
	AddToColumn(Page, NewLine(WidgetBlueprint, TEXT("Img_HeaderLine"), false), HAlign_Fill, FMargin(0.f, 14.f, 0.f, 0.f));

	// 본문: 분류 | 항목 | 설명.
	UHorizontalBox* const Body = NewWidget<UHorizontalBox>(WidgetBlueprint, TEXT("HorizontalBox_Body"));
	UVerticalBoxSlot* const BodySlot = Page->AddChildToVerticalBox(Body);
	BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	BodySlot->SetPadding(FMargin(0.f, 28.f, 0.f, 0.f));

	USizeBox* const TabsWidth = NewSizeBox(WidgetBlueprint, TEXT("SizeBox_Tabs"), 220.f, 0.f);
	UVerticalBox* const Tabs = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("VerticalBox_Tabs"));
	TabsWidth->AddChild(Tabs);
	for (int32 Index = 0; Index < UKnightSettingsWidget::TabCount; ++Index)
	{
		UUserWidget* const Tab = NewWidget<UUserWidget>(WidgetBlueprint, TabClass, *FString::Printf(TEXT("Tab_%d"), Index), true);
		AddToColumn(Tabs, Tab, HAlign_Fill, FMargin(0.f, Index == 0 ? 0.f : 4.f, 0.f, 0.f));
	}
	AddCentered(Body, TabsWidth, false, VAlign_Top);

	UScrollBox* const Scroll = NewWidget<UScrollBox>(WidgetBlueprint, TEXT("Scroll_Rows"), true);
	// 넘칠 때만 보이는 얇은 스크롤바. 설명 패널 구분선 바로 왼쪽에 붙는다.
	FScrollBarStyle BarStyle;
	BarStyle.SetVerticalBackgroundImage(SolidBrush(FLinearColor(FrostDim.R, FrostDim.G, FrostDim.B, 0.25f), FVector2D(3.f, 3.f)));
	BarStyle.SetNormalThumbImage(SolidBrush(FrostDim, FVector2D(3.f, 3.f)));
	BarStyle.SetHoveredThumbImage(SolidBrush(Frost, FVector2D(3.f, 3.f)));
	BarStyle.SetDraggedThumbImage(SolidBrush(Frost, FVector2D(3.f, 3.f)));
	// 위아래 칸은 그리지 않지만 크기는 남는다. 기본 32px이면 바가 그만큼 넓어진다.
	FSlateBrush SlotBrush = NoBrush();
	SlotBrush.ImageSize = FVector2D(3.f, 3.f);
	BarStyle.SetVerticalTopSlotImage(SlotBrush);
	BarStyle.SetVerticalBottomSlotImage(SlotBrush);
	BarStyle.SetThickness(3.f);
	Scroll->SetWidgetBarStyle(BarStyle);
	Scroll->SetScrollBarVisibility(ESlateVisibility::Visible);
	Scroll->SetScrollbarThickness(FVector2D(3.f, 3.f));
	// 바 쪽 여백은 스크롤바 패딩이 아니라 목록 쪽에 둔다. 스크롤바 패딩에는 바 배경색이 같이 칠해진다.
	Scroll->SetScrollbarPadding(FMargin(0.f));
	Scroll->SetNavigationDestination(EDescendantScrollDestination::IntoView);
	if (UScrollBoxSlot* const RowsSlot = Cast<UScrollBoxSlot>(Scroll->AddChild(NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("Box_Rows"), true))))
	{
		RowsSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
	}
	AddCentered(Body, Scroll, true, VAlign_Fill, FMargin(40.f, 0.f, 24.f, 0.f));

	AddCentered(Body, NewLine(WidgetBlueprint, TEXT("Img_DetailLine"), true), false, VAlign_Fill);
	USizeBox* const DetailWidth = NewSizeBox(WidgetBlueprint, TEXT("SizeBox_Detail"), 400.f, 0.f);
	UVerticalBox* const Detail = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("VerticalBox_Detail"));
	DetailWidth->AddChild(Detail);
	AddToColumn(Detail, NewText(WidgetBlueprint, TEXT("Txt_DetailCaption"), CaptionStyle, INVTEXT("DETAIL"), false), HAlign_Left);
	UCommonTextBlock* const DetailTitle = NewText(WidgetBlueprint, TEXT("Txt_DetailTitle"), SubheadStyle, INVTEXT("BGM"), true);
	DetailTitle->SetColorAndOpacity(Ember);
	AddToColumn(Detail, DetailTitle, HAlign_Left, FMargin(0.f, 8.f, 0.f, 0.f));
	UCommonTextBlock* const DetailBody = NewText(WidgetBlueprint, TEXT("Txt_DetailBody"), BodyDimStyle,
		NSLOCTEXT("knightUI", "SettingsDetailPlaceholder", "전투와 탐색 음악의 크기입니다."), true);
	DetailBody->SetAutoWrapText(true);
	AddToColumn(Detail, DetailBody, HAlign_Fill, FMargin(0.f, 10.f, 0.f, 0.f));
	// 감마 계조표: 검정부터 흰색까지 11칸.
	UHorizontalBox* const Chart = NewWidget<UHorizontalBox>(WidgetBlueprint, TEXT("Panel_Chart"), true);
	for (int32 Step = 0; Step <= 10; ++Step)
	{
		const float Gray = Step / 10.f;
		AddCentered(Chart, NewHUDImage(WidgetBlueprint, *FString::Printf(TEXT("Img_Chart%02d"), Step), FLinearColor(Gray, Gray, Gray), FVector2D(30.f, 30.f)), false, VAlign_Top);
	}
	AddToColumn(Detail, Chart, HAlign_Left, FMargin(0.f, 24.f, 0.f, 0.f));
	AddCentered(Body, DetailWidth, false, VAlign_Top, FMargin(28.f, 0.f, 0.f, 0.f));

	// 바닥: 키 안내 | 상태 | 버튼. 저장하지 않고 닫으려 하면 버튼 묶음이 확인 버튼으로 바뀐다.
	AddToColumn(Page, NewLine(WidgetBlueprint, TEXT("Img_FooterLine"), false), HAlign_Fill, FMargin(0.f, 20.f, 0.f, 0.f));
	UHorizontalBox* const Footer = NewWidget<UHorizontalBox>(WidgetBlueprint, TEXT("HorizontalBox_Footer"));
	// 포커스가 분류에 있을 때와 항목에 있을 때 안내가 다르다. C++가 둘 중 하나만 보이게 한다.
	UOverlay* const Hints = NewWidget<UOverlay>(WidgetBlueprint, TEXT("Overlay_Hints"));
	AddToOverlay(Hints, NewKeyHints(WidgetBlueprint, TEXT("Panel_HintsTabs"), CaptionStyle, {
		{ { INVTEXT("↑"), INVTEXT("↓") }, NSLOCTEXT("knightUI", "HintCategory", "분류") },
		{ { INVTEXT("Enter") }, NSLOCTEXT("knightUI", "HintSelect", "선택") },
		{ { INVTEXT("Esc") }, NSLOCTEXT("knightUI", "HintClose", "닫기") } }), HAlign_Left, VAlign_Center);
	AddToOverlay(Hints, NewKeyHints(WidgetBlueprint, TEXT("Panel_HintsRows"), CaptionStyle, {
		{ { INVTEXT("↑"), INVTEXT("↓") }, NSLOCTEXT("knightUI", "HintItem", "항목") },
		{ { INVTEXT("←"), INVTEXT("→") }, NSLOCTEXT("knightUI", "HintValue", "값") },
		{ { INVTEXT("Q"), INVTEXT("E") }, NSLOCTEXT("knightUI", "HintCategory", "분류") },
		{ { INVTEXT("R") }, NSLOCTEXT("knightUI", "HintDefaults", "기본값") },
		{ { INVTEXT("F") }, NSLOCTEXT("knightUI", "HintApply", "적용") },
		{ { INVTEXT("Esc") }, NSLOCTEXT("knightUI", "HintBack", "뒤로") } }), HAlign_Left, VAlign_Center);
	AddCentered(Footer, Hints, false);
	AddCentered(Footer, NewText(WidgetBlueprint, TEXT("Txt_Status"), BodyDimStyle, FText::GetEmpty(), true), true, VAlign_Center, FMargin(32.f, 0.f, 24.f, 0.f));

	const auto NewFooterButton = [&](const TCHAR* Name, const FText& Label, bool bSecondary)
	{
		UknightButtonBase* const Button = NewWidget<UknightButtonBase>(WidgetBlueprint, TSubclassOf<UknightButtonBase>(ButtonClass), Name, true);
		Button->SetButtonText(Label);
		if (bSecondary && SecondaryButtonStyle)
		{
			Button->SetStyle(SecondaryButtonStyle);
		}
		return Button;
	};
	UHorizontalBox* const Buttons = NewWidget<UHorizontalBox>(WidgetBlueprint, TEXT("Panel_Footer"), true);
	AddCentered(Buttons, NewFooterButton(TEXT("Btn_Defaults"), NSLOCTEXT("knightUI", "SettingsDefaults", "기본값"), true), false);
	AddCentered(Buttons, NewFooterButton(TEXT("Btn_Revert"), NSLOCTEXT("knightUI", "SettingsRevert", "되돌리기"), true), false, VAlign_Center, FMargin(12.f, 0.f, 0.f, 0.f));
	AddCentered(Buttons, NewFooterButton(TEXT("Btn_Apply"), NSLOCTEXT("knightUI", "SettingsApply", "적용"), false), false, VAlign_Center, FMargin(12.f, 0.f, 0.f, 0.f));
	AddCentered(Footer, Buttons, false);
	UHorizontalBox* const Prompt = NewWidget<UHorizontalBox>(WidgetBlueprint, TEXT("Panel_Prompt"), true);
	AddCentered(Prompt, NewFooterButton(TEXT("Btn_PromptContinue"), NSLOCTEXT("knightUI", "SettingsPromptContinue", "계속 편집"), true), false);
	AddCentered(Prompt, NewFooterButton(TEXT("Btn_PromptDiscard"), NSLOCTEXT("knightUI", "SettingsPromptDiscard", "버리고 닫기"), true), false, VAlign_Center, FMargin(12.f, 0.f, 0.f, 0.f));
	AddCentered(Prompt, NewFooterButton(TEXT("Btn_PromptApply"), NSLOCTEXT("knightUI", "SettingsPromptApply", "적용하고 닫기"), false), false, VAlign_Center, FMargin(12.f, 0.f, 0.f, 0.f));
	AddCentered(Footer, Prompt, false);
	AddToColumn(Page, Footer, HAlign_Fill, FMargin(0.f, 14.f, 0.f, 0.f));

	EndBuild(WidgetBlueprint);
	return true;
}
