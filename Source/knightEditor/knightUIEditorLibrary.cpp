#include "knightUIEditorLibrary.h"

#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "WidgetBlueprint.h"
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
	FSlateBrush FrameBrush()
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor(Navy900.R, Navy900.G, Navy900.B, 0.8f));
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
