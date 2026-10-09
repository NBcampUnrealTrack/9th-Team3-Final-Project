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
#include "Components/Spacer.h"
#include "Components/Slider.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Texture2D.h"
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

bool UknightUIEditorLibrary::BuildHUDPlayerStatus(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> CaptionStyle, TSubclassOf<UCommonTextStyle> WarningStyle,
	TSubclassOf<UCommonTextStyle> NumberStyle)
{
	if (!BeginBuild(WidgetBlueprint, TEXT("BuildHUDPlayerStatus")))
	{
		return false;
	}

	// 화면 전체를 덮는 캔버스: 체력은 왼쪽 아래, 부스트는 아래 가운데. 두 패널 모두 세이프존 아래 끝에 맞춘다.
	UCanvasPanel* const Root = NewWidget<UCanvasPanel>(WidgetBlueprint, TEXT("Canvas_Root"));
	WidgetBlueprint->WidgetTree->RootWidget = Root;
	auto PlaceBottom = [Root](UWidget* Panel, float AnchorX, float X)
	{
		UCanvasPanelSlot* const Slot = Root->AddChildToCanvas(Panel);
		Slot->SetAnchors(FAnchors(AnchorX, 1.f));
		Slot->SetAlignment(FVector2D(AnchorX, 1.f));
		Slot->SetPosition(FVector2D(X, -HUDMargin.Y));
		Slot->SetAutoSize(true);
	};

	// 체력: 캡션 + 큰 현재값 + 작은 최대값, 아래에 고스트 바.
	USizeBox* const HealthPanel = NewSizeBox(WidgetBlueprint, TEXT("SizeBox_HealthPanel"), 360.f, 0.f);
	UVerticalBox* const HealthColumn = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("VerticalBox_Health"));
	HealthPanel->AddChild(HealthColumn);
	UHorizontalBox* const HealthRow = NewWidget<UHorizontalBox>(WidgetBlueprint, TEXT("HorizontalBox_HealthLabels"));
	AddToRow(HealthRow, NewText(WidgetBlueprint, TEXT("Txt_HealthLabel"), CaptionStyle, INVTEXT("HP"), false), true, HAlign_Left, FMargin(0.f, 0.f, 0.f, 8.f));
	AddToRow(HealthRow, NewText(WidgetBlueprint, TEXT("Txt_Health"), NumberStyle, INVTEXT("100"), true), false, HAlign_Right);
	AddToRow(HealthRow, NewText(WidgetBlueprint, TEXT("Txt_HealthMax"), CaptionStyle, INVTEXT("/100"), true), false, HAlign_Right, FMargin(6.f, 0.f, 0.f, 8.f));
	AddToColumn(HealthColumn, HealthRow, HAlign_Fill);
	USizeBox* const HealthBox = NewSizeBox(WidgetBlueprint, TEXT("SizeBox_Health"), 0.f, 8.f);
	UOverlay* const HealthBars = NewWidget<UOverlay>(WidgetBlueprint, TEXT("Overlay_HealthBars"));
	HealthBox->AddChild(HealthBars);
	// 고스트 바가 테두리와 바탕을 그리고, 그 위에 실제 체력 바를 겹친다.
	AddToOverlay(HealthBars, NewBar(WidgetBlueprint, TEXT("Bar_HealthGhost"), true, 1.f), HAlign_Fill, VAlign_Fill);
	AddToOverlay(HealthBars, NewBar(WidgetBlueprint, TEXT("Bar_Health"), false, 1.f), HAlign_Fill, VAlign_Fill);
	AddToColumn(HealthColumn, HealthBox, HAlign_Fill, FMargin(0.f, 2.f, 0.f, 0.f));
	PlaceBottom(HealthPanel, 0.f, HUDMargin.X);

	// 부스트: 캡션 줄 + 3등분 눈금이 있는 긴 바. 낮을 때 깜빡이는 빨간 테두리가 바를 감싼다.
	USizeBox* const BoostPanel = NewSizeBox(WidgetBlueprint, TEXT("SizeBox_BoostPanel"), 640.f, 0.f);
	UVerticalBox* const BoostColumn = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("VerticalBox_Boost"));
	BoostPanel->AddChild(BoostColumn);
	UHorizontalBox* const BoostRow = NewWidget<UHorizontalBox>(WidgetBlueprint, TEXT("HorizontalBox_BoostLabels"));
	AddToRow(BoostRow, NewText(WidgetBlueprint, TEXT("Txt_BoostLabel"), CaptionStyle, INVTEXT("BOOST"), false), false, HAlign_Left);
	AddToRow(BoostRow, NewText(WidgetBlueprint, TEXT("Txt_Overheat"), WarningStyle, INVTEXT("OVERHEAT"), true), false, HAlign_Left, FMargin(12.f, 0.f, 0.f, 0.f));
	// 수치가 남은 폭을 채우고 오른쪽에 붙는다. OVERHEAT가 접혀도 위치가 그대로다.
	AddToRow(BoostRow, NewText(WidgetBlueprint, TEXT("Txt_Boost"), CaptionStyle, INVTEXT("100%"), true), true, HAlign_Right);
	AddToColumn(BoostColumn, BoostRow, HAlign_Fill);

	USizeBox* const BoostBox = NewSizeBox(WidgetBlueprint, TEXT("SizeBox_Boost"), 0.f, 8.f);
	UOverlay* const BoostLayers = NewWidget<UOverlay>(WidgetBlueprint, TEXT("Overlay_Boost"));
	BoostBox->AddChild(BoostLayers);
	AddToOverlay(BoostLayers, NewBar(WidgetBlueprint, TEXT("Bar_Boost"), true, 1.f), HAlign_Fill, VAlign_Fill);
	UHorizontalBox* const Ticks = NewWidget<UHorizontalBox>(WidgetBlueprint, TEXT("HorizontalBox_BoostTicks"));
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (Index > 0)
		{
			UHorizontalBoxSlot* const TickSlot = Ticks->AddChildToHorizontalBox(NewHUDImage(WidgetBlueprint, *FString::Printf(TEXT("Img_BoostTick%d"), Index), Navy900, FVector2D(2.f, 8.f)));
			TickSlot->SetVerticalAlignment(VAlign_Fill);
		}
		Ticks->AddChildToHorizontalBox(NewWidget<USpacer>(WidgetBlueprint, *FString::Printf(TEXT("Spacer_Boost%d"), Index)))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	AddToOverlay(BoostLayers, Ticks, HAlign_Fill, VAlign_Fill);
	UImage* const Alert = NewWidget<UImage>(WidgetBlueprint, TEXT("Img_BoostAlert"), true);
	FSlateBrush AlertBrush;
	AlertBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	AlertBrush.TintColor = FSlateColor(FLinearColor::Transparent);
	AlertBrush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(0.f, 0.f, 0.f, 0.f), FSlateColor(FLinearColor(FColor(0xE5, 0x48, 0x4D))), 2.f);
	AlertBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	Alert->SetBrush(AlertBrush);
	Alert->SetVisibility(ESlateVisibility::Hidden);
	Alert->SetPixelSnapping(EWidgetPixelSnapping::Disabled);
	// 바 바깥으로 3px 나와서 바의 테두리와 겹치지 않는다.
	AddToOverlay(BoostLayers, Alert, HAlign_Fill, VAlign_Fill, FMargin(-3.f));
	AddToColumn(BoostColumn, BoostBox, HAlign_Fill, FMargin(0.f, 6.f, 0.f, 0.f));
	PlaceBottom(BoostPanel, 0.5f, 0.f);

	EndBuild(WidgetBlueprint);
	return true;
}

bool UknightUIEditorLibrary::BuildHUDRaidTimer(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> TimeStyle, TSubclassOf<UCommonTextStyle> CaptionStyle,
	TSubclassOf<UCommonTextStyle> ObjectiveStyle)
{
	if (!BeginBuild(WidgetBlueprint, TEXT("BuildHUDRaidTimer")))
	{
		return false;
	}

	// 목표 한 줄: 캡션, 목표 + 남은 시간, 밑줄(왼쪽에 짧은 강조).
	USizeBox* const Root = NewSizeBox(WidgetBlueprint, TEXT("SizeBox_Root"), 380.f, 0.f);
	WidgetBlueprint->WidgetTree->RootWidget = Root;
	UVerticalBox* const Column = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("VerticalBox_Root"));
	Root->AddChild(Column);
	AddToColumn(Column, NewText(WidgetBlueprint, TEXT("Txt_Caption"), CaptionStyle, INVTEXT("OBJECTIVE"), true), HAlign_Left);
	UHorizontalBox* const Row = NewWidget<UHorizontalBox>(WidgetBlueprint, TEXT("HorizontalBox_Objective"));
	AddToRow(Row, NewText(WidgetBlueprint, TEXT("Txt_Objective"), ObjectiveStyle, NSLOCTEXT("knightUI", "ObjectivePlaceholder", "잔열 토벌"), true), true, HAlign_Left);
	AddToRow(Row, NewText(WidgetBlueprint, TEXT("Txt_Time"), TimeStyle, INVTEXT("12:48"), true), false, HAlign_Right, FMargin(24.f, 0.f, 0.f, 0.f));
	AddToColumn(Column, Row, HAlign_Fill, FMargin(0.f, 4.f, 0.f, 0.f));
	UOverlay* const Underline = NewWidget<UOverlay>(WidgetBlueprint, TEXT("Overlay_Underline"));
	AddToOverlay(Underline, NewHUDImage(WidgetBlueprint, TEXT("Img_Underline"), FLinearColor(Frost.R, Frost.G, Frost.B, 0.6f), FVector2D(8.f, 1.f)), HAlign_Fill, VAlign_Center);
	AddToOverlay(Underline, NewHUDImage(WidgetBlueprint, TEXT("Img_UnderlineAccent"), Frost, FVector2D(32.f, 3.f)), HAlign_Left, VAlign_Center);
	AddToColumn(Column, Underline, HAlign_Fill, FMargin(0.f, 6.f, 0.f, 0.f));

	EndBuild(WidgetBlueprint);
	return true;
}

bool UknightUIEditorLibrary::BuildHUDLockOnMarker(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> CaptionStyle, TSubclassOf<UCommonTextStyle> NameStyle,
	UTexture2D* PartFrameTexture, TSubclassOf<UCommonTextStyle> StaggerCaptionStyle)
{
	if (!BeginBuild(WidgetBlueprint, TEXT("BuildHUDLockOnMarker")))
	{
		return false;
	}

	UCanvasPanel* const Root = NewWidget<UCanvasPanel>(WidgetBlueprint, TEXT("Canvas_Root"));
	WidgetBlueprint->WidgetTree->RootWidget = Root;
	// 디자이너에서 보이도록 놓는 자리. 실행 중에는 C++가 대상 위치로 옮긴다.
	const FVector2D Preview(400.f, 300.f);

	// 부위 틀: 박스 뒤에 깔린다. 색은 C++가 부위 상태로 정하고, 흰 PartFlash는 상태가 바뀔 때만 잠깐 보인다.
	if (PartFrameTexture)
	{
		for (const TCHAR* const Name : { TEXT("PartFrame"), TEXT("PartFlash") })
		{
			UImage* const Image = NewHUDImage(WidgetBlueprint, Name, FLinearColor::White, FVector2D(104.f, 104.f), true);
			FSlateBrush Brush;
			Brush.SetResourceObject(PartFrameTexture);
			Brush.ImageSize = FVector2D(104.f, 104.f);
			Image->SetBrush(Brush);
			UCanvasPanelSlot* const FrameSlot = Root->AddChildToCanvas(Image);
			FrameSlot->SetAutoSize(true);
			FrameSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			FrameSlot->SetPosition(Preview);
		}
	}

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
	// 부위 이름: 눈금(4) + 간격(8)만큼 들여서 몬스터 이름과 맞춘다. 부위가 없으면 C++가 접는다.
	UCommonTextBlock* const PartName = NewText(WidgetBlueprint, TEXT("Txt_PartName"), CaptionStyle, INVTEXT("PART"), true);
	AddToColumn(Plate, PartName, HAlign_Left);
	if (UVerticalBoxSlot* const PartSlot = Cast<UVerticalBoxSlot>(PartName->Slot))
	{
		PartSlot->SetPadding(FMargin(12.f, 2.f, 0.f, 0.f));
	}
	UCanvasPanelSlot* const PlateSlot = Root->AddChildToCanvas(Plate);
	PlateSlot->SetAutoSize(true);
	PlateSlot->SetAlignment(FVector2D(0.f, 1.f));
	PlateSlot->SetPosition(Preview + FVector2D(64.f, -78.f));

	// 박스 아래에 경직 게이지, 그 아래 거리. 가지선이 위로 나가므로 겹치지 않는다. 채움 색은 C++가 칠한다.
	UVerticalBox* const Stagger = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("Stagger"), true);
	Stagger->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddToColumn(Stagger, NewText(WidgetBlueprint, TEXT("Txt_StaggerCaption"), StaggerCaptionStyle ? StaggerCaptionStyle : CaptionStyle, INVTEXT("STAGGER"), false), HAlign_Center);
	USizeBox* const StaggerBox = NewSizeBox(WidgetBlueprint, TEXT("SizeBox_Stagger"), 96.f, 5.f);
	StaggerBox->AddChild(NewBar(WidgetBlueprint, TEXT("Bar_Stagger"), true, 0.6f));
	AddToColumn(Stagger, StaggerBox, HAlign_Center, FMargin(0.f, 2.f, 0.f, 0.f));
	UCanvasPanelSlot* const StaggerSlot = Root->AddChildToCanvas(Stagger);
	StaggerSlot->SetAutoSize(true);
	StaggerSlot->SetAlignment(FVector2D(0.5f, 0.f));
	StaggerSlot->SetPosition(Preview + FVector2D(0.f, 58.f));

	UCommonTextBlock* const Distance = NewText(WidgetBlueprint, TEXT("Txt_Distance"), CaptionStyle, INVTEXT("[ 42M ]"), true);
	Distance->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* const DistanceSlot = Root->AddChildToCanvas(Distance);
	DistanceSlot->SetAutoSize(true);
	DistanceSlot->SetAlignment(FVector2D(0.5f, 0.f));
	DistanceSlot->SetPosition(Preview + FVector2D(0.f, 90.f));

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

	// 플레이어 패널은 화면 전체에 깔고 그 안에서 체력(왼쪽 아래)과 부스트(아래 가운데)를 놓는다.
	UCanvasPanelSlot* const PlayerSlot = Root->AddChildToCanvas(NewWidget<UUserWidget>(WidgetBlueprint, PlayerStatusClass, TEXT("PlayerStatus"), true));
	PlayerSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	PlayerSlot->SetOffsets(FMargin(0.f));

	// 목표 + 남은 시간: 왼쪽 위.
	UCanvasPanelSlot* const TimerSlot = Root->AddChildToCanvas(NewWidget<UUserWidget>(WidgetBlueprint, RaidTimerClass, TEXT("RaidTimer"), true));
	TimerSlot->SetAnchors(FAnchors(0.f, 0.f));
	TimerSlot->SetAlignment(FVector2D(0.f, 0.f));
	TimerSlot->SetPosition(HUDMargin);
	TimerSlot->SetAutoSize(true);

	EndBuild(WidgetBlueprint);
	return true;
}

bool UknightUIEditorLibrary::RemoveWidget(UWidgetBlueprint* WidgetBlueprint, FName WidgetName)
{
	UWidget* const Widget = WidgetBlueprint && WidgetBlueprint->WidgetTree ? WidgetBlueprint->WidgetTree->FindWidget(WidgetName) : nullptr;
	if (!Widget)
	{
		return false;
	}
	WidgetBlueprint->Modify();
	WidgetBlueprint->WidgetTree->Modify();

	// 자식까지 함께 지운다. GUID 표에 이름이 남으면 컴파일러가 없는 위젯을 찾으므로 같이 뺀다.
	TArray<UWidget*> Removed = { Widget };
	UWidgetTree::GetChildWidgets(Widget, Removed);
	WidgetBlueprint->WidgetTree->RemoveWidget(Widget);
	for (UWidget* const Each : Removed)
	{
		WidgetBlueprint->OnVariableRemoved(Each->GetFName());
		// Frees the name, so a rebuilt tree can reuse it.
		Each->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
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

bool UknightUIEditorLibrary::BuildRaidResult(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> ButtonClass, TSubclassOf<UCommonTextStyle> DisplayStyle,
	TSubclassOf<UCommonTextStyle> TitleStyle, TSubclassOf<UCommonTextStyle> SubheadStyle, TSubclassOf<UCommonTextStyle> BodyStyle,
	TSubclassOf<UCommonTextStyle> BodyDimStyle, TSubclassOf<UCommonTextStyle> NumberStyle, TSubclassOf<UCommonTextStyle> CaptionStyle)
{
	if (!ButtonClass || !ButtonClass->IsChildOf<UknightButtonBase>() || !BeginBuild(WidgetBlueprint, TEXT("BuildRaidResult")))
	{
		return false;
	}

	UOverlay* const Root = NewWidget<UOverlay>(WidgetBlueprint, TEXT("Overlay_Root"));
	WidgetBlueprint->WidgetTree->RootWidget = Root;

	// C++가 투명도를 0에서 1로 올린다.
	UImage* const Dim = NewWidget<UImage>(WidgetBlueprint, TEXT("Img_Dim"), true);
	Dim->SetBrush(SolidBrush(FLinearColor::White));
	Dim->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	AddToOverlay(Root, Dim, HAlign_Fill, VAlign_Fill);

	// 1단계: 화면을 가로지르는 배너. 강조색(성공 주황 / 실패 빨강)은 C++가 칠한다.
	UBorder* const Banner = NewWidget<UBorder>(WidgetBlueprint, TEXT("Banner"), true);
	Banner->SetBrush(FrameBrush(0.88f));
	Banner->SetPadding(FMargin(0.f, 28.f));
	Banner->SetHorizontalAlignment(HAlign_Center);
	Banner->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	AddToOverlay(Root, Banner, HAlign_Fill, VAlign_Center);
	UVerticalBox* const BannerColumn = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("VerticalBox_Banner"));
	Banner->SetContent(BannerColumn);
	AddToColumn(BannerColumn, NewText(WidgetBlueprint, TEXT("Txt_BannerCaption"), CaptionStyle, INVTEXT("MISSION COMPLETE"), true), HAlign_Center);
	AddToColumn(BannerColumn, NewHUDImage(WidgetBlueprint, TEXT("Img_Accent"), Ember, FVector2D(48.f, 2.f), true), HAlign_Center, FMargin(0.f, 10.f, 0.f, 12.f));
	AddToColumn(BannerColumn, NewText(WidgetBlueprint, TEXT("Txt_BannerTitle"), DisplayStyle, NSLOCTEXT("knightUI", "ResultSuccess", "토벌 완료"), true), HAlign_Center);
	AddToColumn(BannerColumn, NewText(WidgetBlueprint, TEXT("Txt_BannerReason"), SubheadStyle, NSLOCTEXT("knightUI", "FailKnightDestroyed", "기체 대파"), true), HAlign_Center, FMargin(0.f, 6.f, 0.f, 0.f));

	// 2단계: 보고 패널.
	USizeBox* const Report = NewSizeBox(WidgetBlueprint, TEXT("Report"), 640.f, 0.f);
	Report->bIsVariable = true;
	AddToOverlay(Root, Report, HAlign_Center, VAlign_Center);
	UBorder* const Frame = NewWidget<UBorder>(WidgetBlueprint, TEXT("Border_Frame"));
	Frame->SetBrush(FrameBrush(0.94f));
	Frame->SetPadding(FMargin(40.f, 32.f, 40.f, 28.f));
	Report->AddChild(Frame);
	UVerticalBox* const Column = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("VerticalBox_Report"));
	Frame->SetContent(Column);

	AddToColumn(Column, NewText(WidgetBlueprint, TEXT("Txt_ReportCaption"), CaptionStyle, INVTEXT("RAID REPORT"), false), HAlign_Left);
	AddToColumn(Column, NewHUDImage(WidgetBlueprint, TEXT("Img_ReportAccent"), Frost, FVector2D(32.f, 2.f)), HAlign_Left, FMargin(0.f, 10.f, 0.f, 14.f));
	AddToColumn(Column, NewText(WidgetBlueprint, TEXT("Txt_StageName"), TitleStyle, NSLOCTEXT("knightUI", "ResultStagePlaceholder", "잔열 토벌"), true), HAlign_Left);

	// 왼쪽 항목 이름, 오른쪽 값. 첫 줄만 위 여백을 크게.
	int32 RowIndex = 0;
	const auto AddStatRow = [&](const TCHAR* RowName, const FText& Label, std::initializer_list<UWidget*> Values)
	{
		UHorizontalBox* const Row = NewWidget<UHorizontalBox>(WidgetBlueprint, RowName);
		UHorizontalBoxSlot* const LabelSlot = Row->AddChildToHorizontalBox(NewText(WidgetBlueprint, *FString::Printf(TEXT("Txt_%sLabel"), RowName + 14), BodyDimStyle, Label, false));
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		bool bFirstValue = true;
		for (UWidget* const Value : Values)
		{
			UHorizontalBoxSlot* const ValueSlot = Row->AddChildToHorizontalBox(Value);
			ValueSlot->SetVerticalAlignment(VAlign_Center);
			ValueSlot->SetPadding(FMargin(bFirstValue ? 0.f : 12.f, 0.f, 0.f, 0.f));
			bFirstValue = false;
		}
		AddToColumn(Column, Row, HAlign_Fill, FMargin(0.f, RowIndex++ == 0 ? 28.f : 10.f, 0.f, 0.f));
	};
	AddStatRow(TEXT("HorizontalBox_ClearTime"), NSLOCTEXT("knightUI", "ResultClearTime", "작전 시간"),
		{ NewText(WidgetBlueprint, TEXT("Txt_ClearTime"), NumberStyle, INVTEXT("03:42.15"), true) });
	UCommonTextBlock* const NewRecord = NewText(WidgetBlueprint, TEXT("Txt_NewRecord"), CaptionStyle, INVTEXT("NEW RECORD"), true);
	NewRecord->SetColorAndOpacity(Ember);
	AddStatRow(TEXT("HorizontalBox_BestTime"), NSLOCTEXT("knightUI", "ResultBestTime", "최고 기록"),
		{ NewRecord, NewText(WidgetBlueprint, TEXT("Txt_BestTime"), NumberStyle, INVTEXT("03:42.15"), true) });
	AddStatRow(TEXT("HorizontalBox_BrokenParts"), NSLOCTEXT("knightUI", "ResultBrokenParts", "부위 파괴"),
		{ NewText(WidgetBlueprint, TEXT("Txt_BrokenParts"), BodyStyle, NSLOCTEXT("knightUI", "ResultBrokenPartsPlaceholder", "2곳"), true) });

	AddToColumn(Column, NewHUDImage(WidgetBlueprint, TEXT("Img_Divider"), FrostDim, FVector2D(32.f, 1.f)), HAlign_Fill, FMargin(0.f, 20.f, 0.f, 16.f));
	AddToColumn(Column, NewText(WidgetBlueprint, TEXT("Txt_RewardsLabel"), BodyDimStyle, NSLOCTEXT("knightUI", "ResultRewards", "획득 보상"), false), HAlign_Left);
	// 보상 줄은 C++가 런타임에 채운다 (WBP의 RewardTextStyle 사용).
	AddToColumn(Column, NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("Box_Rewards"), true), HAlign_Fill, FMargin(0.f, 10.f, 0.f, 0.f));
	AddToColumn(Column, NewText(WidgetBlueprint, TEXT("Txt_NoRewards"), BodyDimStyle, NSLOCTEXT("knightUI", "ResultNoRewards", "획득한 보상이 없습니다."), true), HAlign_Left, FMargin(0.f, 10.f, 0.f, 0.f));

	// 아래: 카운트다운 + 지금 귀환.
	UHorizontalBox* const Footer = NewWidget<UHorizontalBox>(WidgetBlueprint, TEXT("HorizontalBox_Footer"));
	UHorizontalBoxSlot* const CountdownSlot = Footer->AddChildToHorizontalBox(NewText(WidgetBlueprint, TEXT("Txt_Countdown"), CaptionStyle,
		NSLOCTEXT("knightUI", "ResultCountdownPlaceholder", "20초 후 기지로 귀환"), true));
	CountdownSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CountdownSlot->SetVerticalAlignment(VAlign_Center);
	UknightButtonBase* const Return = NewWidget<UknightButtonBase>(WidgetBlueprint, TSubclassOf<UknightButtonBase>(ButtonClass), TEXT("Btn_Return"), true);
	Return->SetButtonText(NSLOCTEXT("knightUI", "ResultReturn", "지금 귀환"));
	Footer->AddChildToHorizontalBox(Return)->SetVerticalAlignment(VAlign_Center);
	AddToColumn(Column, Footer, HAlign_Fill, FMargin(0.f, 32.f, 0.f, 0.f));

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

namespace
{
	/** Offsets mean position + size for point anchors, and edge distances for stretched anchors. */
	UCanvasPanelSlot* AddToCanvas(UCanvasPanel* Canvas, UWidget* Widget, const FAnchors& Anchors, const FVector2D& Alignment, const FMargin& Offsets, bool bAutoSize = false)
	{
		UCanvasPanelSlot* const Slot = Canvas->AddChildToCanvas(Widget);
		Slot->SetAnchors(Anchors);
		Slot->SetAlignment(Alignment);
		Slot->SetOffsets(Offsets);
		Slot->SetAutoSize(bAutoSize);
		return Slot;
	}

	void AddFill(UCanvasPanel* Canvas, UWidget* Widget)
	{
		AddToCanvas(Canvas, Widget, FAnchors(0.f, 0.f, 1.f, 1.f), FVector2D::ZeroVector, FMargin(0.f));
	}

	UImage* NewTextureImage(UWidgetBlueprint* WidgetBlueprint, const FName Name, UTexture2D* Texture, const FLinearColor& Tint, const FVector2D& Size)
	{
		UImage* const Image = NewWidget<UImage>(WidgetBlueprint, Name);
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = Size;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Image->SetBrush(Brush);
		Image->SetColorAndOpacity(Tint);
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Image;
	}

	/** Navy fade from the left edge so the menu text reads over the 3D scene, which stays visible on the right. */
	UImage* NewLeftFade(UWidgetBlueprint* WidgetBlueprint, const FName Name, UTexture2D* Gradient)
	{
		return NewTextureImage(WidgetBlueprint, Name, Gradient, FLinearColor(Navy900.R, Navy900.G, Navy900.B, 0.85f), FVector2D(512.f, 8.f));
	}

	UVerticalBox* NewTextMenu(UWidgetBlueprint* WidgetBlueprint, const FName Name, TSubclassOf<UUserWidget> ItemClass, std::initializer_list<TPair<const TCHAR*, FText>> Items)
	{
		UVerticalBox* const Menu = NewWidget<UVerticalBox>(WidgetBlueprint, Name);
		for (const TPair<const TCHAR*, FText>& Item : Items)
		{
			UknightButtonBase* const Button = NewWidget<UknightButtonBase>(WidgetBlueprint, TSubclassOf<UknightButtonBase>(ItemClass), Item.Key, true);
			Button->SetButtonText(Item.Value);
			// Grow from the left edge, like the text itself.
			Button->SetRenderTransformPivot(FVector2D(0.f, 0.5f));
			AddToColumn(Menu, Button, HAlign_Left, FMargin(0.f, 4.f));
		}
		return Menu;
	}

	/** Caption above a value, for the info panels. */
	void AddCaptionedValue(UWidgetBlueprint* WidgetBlueprint, UVerticalBox* Column, const TCHAR* ValueName, const FText& Caption, const FText& Placeholder,
		TSubclassOf<UCommonTextStyle> CaptionStyle, TSubclassOf<UCommonTextStyle> ValueStyle, float Top)
	{
		AddToColumn(Column, NewText(WidgetBlueprint, *FString::Printf(TEXT("%s_Caption"), ValueName), CaptionStyle, Caption, false), HAlign_Left, FMargin(0.f, Top, 0.f, 0.f));
		AddToColumn(Column, NewText(WidgetBlueprint, ValueName, ValueStyle, Placeholder, true), HAlign_Left, FMargin(0.f, 4.f, 0.f, 0.f));
	}

	/** Back button shared by both menus: bottom right, outside the page switcher. */
	void AddBackAndStatus(UWidgetBlueprint* WidgetBlueprint, UCanvasPanel* Canvas, TSubclassOf<UUserWidget> ButtonClass, TSubclassOf<UCommonButtonStyle> SecondaryStyle,
		TSubclassOf<UCommonTextStyle> StatusStyle)
	{
		UknightButtonBase* const Back = NewWidget<UknightButtonBase>(WidgetBlueprint, TSubclassOf<UknightButtonBase>(ButtonClass), TEXT("Button_Back"), true);
		Back->SetButtonText(NSLOCTEXT("knightUI", "MenuBack", "ESC   돌아가기"));
		if (SecondaryStyle)
		{
			Back->SetStyle(SecondaryStyle);
		}
		AddToCanvas(Canvas, Back, FAnchors(1.f, 1.f), FVector2D(1.f, 1.f), FMargin(-HUDMargin.X, -48.f, 0.f, 0.f), true);
		AddToCanvas(Canvas, NewText(WidgetBlueprint, TEXT("Text_Status"), StatusStyle, FText::GetEmpty(), true),
			FAnchors(0.f, 1.f), FVector2D(0.f, 1.f), FMargin(HUDMargin.X, -56.f, 0.f, 0.f), true);
	}
}

bool UknightUIEditorLibrary::BuildKnightMenuItem(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> LabelStyle)
{
	if (!BeginBuild(WidgetBlueprint, TEXT("BuildKnightMenuItem")))
	{
		return false;
	}
	UHorizontalBox* const Root = NewWidget<UHorizontalBox>(WidgetBlueprint, TEXT("HorizontalBox_Root"));
	WidgetBlueprint->WidgetTree->RootWidget = Root;

	// Orange bar in front of the focused item. The button base fades and opens it like the button brackets.
	AddCentered(Root, NewHUDImage(WidgetBlueprint, TEXT("Bracket_L"), Ember, FVector2D(3.f, 26.f), true), false, VAlign_Center);
	AddCentered(Root, NewText(WidgetBlueprint, TEXT("Txt_Label"), LabelStyle, NSLOCTEXT("knightUI", "MenuItemPlaceholder", "메뉴 항목"), true),
		false, VAlign_Center, FMargin(18.f, 0.f, 0.f, 0.f));

	EndBuild(WidgetBlueprint);
	return true;
}

bool UknightUIEditorLibrary::BuildSaveSlotCard(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> CaptionStyle, TSubclassOf<UCommonTextStyle> TitleStyle,
	TSubclassOf<UCommonTextStyle> SubheadStyle, TSubclassOf<UCommonTextStyle> BodyDimStyle, TSubclassOf<UCommonTextStyle> NumberStyle)
{
	if (!BeginBuild(WidgetBlueprint, TEXT("BuildSaveSlotCard")))
	{
		return false;
	}
	USizeBox* const Root = NewSizeBox(WidgetBlueprint, TEXT("SizeBox_Card"), 420.f, 600.f);
	WidgetBlueprint->WidgetTree->RootWidget = Root;
	UOverlay* const Layers = NewWidget<UOverlay>(WidgetBlueprint, TEXT("Overlay_Card"));
	Root->AddChild(Layers);

	UImage* const Background = NewWidget<UImage>(WidgetBlueprint, TEXT("Img_Background"));
	Background->SetBrush(FrameBrush(0.92f));
	Background->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddToOverlay(Layers, Background, HAlign_Fill, VAlign_Fill);

	// Orange 2px frame for hover / focus; C++ shows it.
	FSlateBrush FocusBrush;
	FocusBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	FocusBrush.TintColor = FSlateColor(FLinearColor::Transparent);
	FocusBrush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(0.f, 0.f, 0.f, 0.f), FSlateColor(Ember), 2.f);
	FocusBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	UImage* const Focus = NewWidget<UImage>(WidgetBlueprint, TEXT("Img_FocusFrame"), true);
	Focus->SetBrush(FocusBrush);
	Focus->SetVisibility(ESlateVisibility::Collapsed);
	AddToOverlay(Layers, Focus, HAlign_Fill, VAlign_Fill);

	UVerticalBox* const Column = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("VerticalBox_Content"));
	AddToOverlay(Layers, Column, HAlign_Fill, VAlign_Fill, FMargin(36.f, 32.f));

	// Header: caption + name on the left, "최근 플레이" tag on the right.
	UHorizontalBox* const Header = NewWidget<UHorizontalBox>(WidgetBlueprint, TEXT("HorizontalBox_Header"));
	UVerticalBox* const Names = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("VerticalBox_Names"));
	AddToColumn(Names, NewText(WidgetBlueprint, TEXT("Txt_SlotCaption"), CaptionStyle, INVTEXT("SLOT 01"), true), HAlign_Left);
	AddToColumn(Names, NewText(WidgetBlueprint, TEXT("Txt_SlotName"), TitleStyle, NSLOCTEXT("knightUI", "SlotNamePlaceholder", "저장 1"), true), HAlign_Left, FMargin(0.f, 4.f, 0.f, 0.f));
	AddCentered(Header, Names, true, VAlign_Top);
	UBorder* const Active = NewWidget<UBorder>(WidgetBlueprint, TEXT("Panel_Active"), true);
	FSlateBrush ActiveBrush = FrameBrush(0.f);
	ActiveBrush.OutlineSettings.Color = FSlateColor(Ember);
	Active->SetBrush(ActiveBrush);
	Active->SetPadding(FMargin(10.f, 4.f));
	UCommonTextBlock* const ActiveText = NewText(WidgetBlueprint, TEXT("Txt_Active"), CaptionStyle, NSLOCTEXT("knightUI", "SlotRecent", "최근 플레이"), false);
	ActiveText->SetColorAndOpacity(Ember);
	Active->SetContent(ActiveText);
	AddCentered(Header, Active, false, VAlign_Top);
	AddToColumn(Column, Header, HAlign_Fill);
	AddToColumn(Column, NewHUDImage(WidgetBlueprint, TEXT("Img_Accent"), Frost, FVector2D(32.f, 2.f)), HAlign_Left, FMargin(0.f, 16.f, 0.f, 0.f));

	// Filled slot: four records, caption above value.
	UVerticalBox* const Filled = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("Panel_Filled"), true);
	AddCaptionedValue(WidgetBlueprint, Filled, TEXT("Txt_PlayTime"), NSLOCTEXT("knightUI", "SlotPlayTime", "총 플레이 시간"), INVTEXT("00:00:00"), BodyDimStyle, NumberStyle, 32.f);
	AddCaptionedValue(WidgetBlueprint, Filled, TEXT("Txt_LastPlayed"), NSLOCTEXT("knightUI", "SlotLastPlayed", "마지막 플레이"), INVTEXT("2026-10-09  16:45"), BodyDimStyle, NumberStyle, 20.f);
	AddToColumn(Filled, NewLine(WidgetBlueprint, TEXT("Img_Divider"), false), HAlign_Fill, FMargin(0.f, 28.f, 0.f, 0.f));
	AddCaptionedValue(WidgetBlueprint, Filled, TEXT("Txt_ClearedStages"), NSLOCTEXT("knightUI", "SlotClearedStages", "토벌한 지역"), NSLOCTEXT("knightUI", "SlotClearedPlaceholder", "0곳"), BodyDimStyle, SubheadStyle, 28.f);
	AddCaptionedValue(WidgetBlueprint, Filled, TEXT("Txt_TotalClears"), NSLOCTEXT("knightUI", "SlotTotalClears", "총 토벌 횟수"), NSLOCTEXT("knightUI", "SlotTotalPlaceholder", "0회"), BodyDimStyle, SubheadStyle, 20.f);
	AddToColumn(Column, Filled, HAlign_Fill);

	// Empty or unreadable slot.
	UVerticalBox* const Empty = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("Panel_Empty"), true);
	AddToColumn(Empty, NewText(WidgetBlueprint, TEXT("Txt_EmptyTitle"), SubheadStyle, NSLOCTEXT("knightUI", "SlotEmptyPlaceholder", "빈 슬롯"), true), HAlign_Left, FMargin(0.f, 32.f, 0.f, 0.f));
	UCommonTextBlock* const EmptyBody = NewText(WidgetBlueprint, TEXT("Txt_EmptyBody"), BodyDimStyle, NSLOCTEXT("knightUI", "SlotEmptyBodyPlaceholder", "선택하면 새 게임을 시작합니다."), true);
	EmptyBody->SetAutoWrapText(true);
	AddToColumn(Empty, EmptyBody, HAlign_Fill, FMargin(0.f, 10.f, 0.f, 0.f));
	AddToColumn(Column, Empty, HAlign_Fill);

	EndBuild(WidgetBlueprint);
	return true;
}

bool UknightUIEditorLibrary::BuildTitleMenu(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> MenuItemClass, TSubclassOf<UUserWidget> ButtonClass,
	TSubclassOf<UCommonButtonStyle> SecondaryButtonStyle, TSubclassOf<UUserWidget> SaveCardClass, UTexture2D* Logo, UTexture2D* Gradient,
	TSubclassOf<UCommonTextStyle> TitleStyle, TSubclassOf<UCommonTextStyle> CaptionStyle, TSubclassOf<UCommonTextStyle> BodyDimStyle)
{
	if (!MenuItemClass || !MenuItemClass->IsChildOf<UknightButtonBase>() || !ButtonClass || !ButtonClass->IsChildOf<UknightButtonBase>() || !SaveCardClass
		|| !BeginBuild(WidgetBlueprint, TEXT("BuildTitleMenu")))
	{
		return false;
	}
	UCanvasPanel* const Root = NewWidget<UCanvasPanel>(WidgetBlueprint, TEXT("Canvas_Root"));
	WidgetBlueprint->WidgetTree->RootWidget = Root;

	UWidgetSwitcher* const Pages = NewWidget<UWidgetSwitcher>(WidgetBlueprint, TEXT("Switcher_Pages"), true);
	AddFill(Root, Pages);

	// Home: left column over a fade; the right side is left clear for the 3D scene.
	UCanvasPanel* const Home = NewWidget<UCanvasPanel>(WidgetBlueprint, TEXT("Panel_Home"), true);
	Pages->AddChild(Home);
	AddToCanvas(Home, NewLeftFade(WidgetBlueprint, TEXT("Img_Fade"), Gradient), FAnchors(0.f, 0.f, 0.f, 1.f), FVector2D::ZeroVector, FMargin(0.f, 0.f, 960.f, 0.f));
	AddToCanvas(Home, NewTextureImage(WidgetBlueprint, TEXT("Img_Logo"), Logo, FLinearColor::White, FVector2D(776.f, 150.f)),
		FAnchors(0.f, 0.5f), FVector2D(0.f, 1.f), FMargin(HUDMargin.X - 30.f, -40.f, 776.f, 150.f));
	AddToCanvas(Home, NewTextMenu(WidgetBlueprint, TEXT("VerticalBox_Menu"), MenuItemClass, {
		{ TEXT("Button_StartGame"), NSLOCTEXT("knightUI", "TitleStart", "게임 시작") },
		{ TEXT("Button_SaveSelect"), NSLOCTEXT("knightUI", "TitleSaveSelect", "저장 파일 선택") },
		{ TEXT("Button_Settings"), NSLOCTEXT("knightUI", "TitleSettings", "설정") },
		{ TEXT("Button_Quit"), NSLOCTEXT("knightUI", "TitleQuit", "게임 종료") } }),
		FAnchors(0.f, 0.5f), FVector2D(0.f, 0.f), FMargin(HUDMargin.X, 20.f, 0.f, 0.f), true);
	AddToCanvas(Home, NewText(WidgetBlueprint, TEXT("Txt_Build"), CaptionStyle, INVTEXT("PROJECT KNIGHT  /  ALPHA BUILD"), false),
		FAnchors(0.5f, 1.f), FVector2D(0.5f, 1.f), FMargin(0.f, -40.f, 0.f, 0.f), true);

	// Save select: header band, three cards, footer band with key hints.
	UOverlay* const SaveSelect = NewWidget<UOverlay>(WidgetBlueprint, TEXT("Panel_SaveSelect"), true);
	Pages->AddChild(SaveSelect);
	UImage* const Dim = NewWidget<UImage>(WidgetBlueprint, TEXT("Img_SaveDim"));
	Dim->SetBrush(SolidBrush(FLinearColor::White));
	Dim->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	AddToOverlay(SaveSelect, Dim, HAlign_Fill, VAlign_Fill);
	UVerticalBox* const SaveColumn = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("VerticalBox_Save"));
	AddToOverlay(SaveSelect, SaveColumn, HAlign_Fill, VAlign_Fill);

	UBorder* const Header = NewWidget<UBorder>(WidgetBlueprint, TEXT("Border_SaveHeader"));
	Header->SetBrush(SolidBrush(FLinearColor(Navy900.R, Navy900.G, Navy900.B, 0.92f)));
	Header->SetPadding(FMargin(0.f, 22.f, 0.f, 20.f));
	Header->SetHorizontalAlignment(HAlign_Center);
	UVerticalBox* const HeaderColumn = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("VerticalBox_SaveHeader"));
	AddToColumn(HeaderColumn, NewText(WidgetBlueprint, TEXT("Txt_SaveCaption"), CaptionStyle, INVTEXT("SELECT SAVE DATA"), false), HAlign_Center);
	AddToColumn(HeaderColumn, NewText(WidgetBlueprint, TEXT("Txt_SaveTitle"), TitleStyle, NSLOCTEXT("knightUI", "SaveSelectTitle", "저장 파일 선택"), false), HAlign_Center, FMargin(0.f, 6.f, 0.f, 0.f));
	Header->SetContent(HeaderColumn);
	AddToColumn(SaveColumn, Header, HAlign_Fill);
	AddToColumn(SaveColumn, NewLine(WidgetBlueprint, TEXT("Img_SaveHeaderLine"), false), HAlign_Fill);

	UHorizontalBox* const Cards = NewWidget<UHorizontalBox>(WidgetBlueprint, TEXT("HorizontalBox_Cards"));
	for (int32 Number = 1; Number <= 3; ++Number)
	{
		UUserWidget* const Card = NewWidget<UUserWidget>(WidgetBlueprint, SaveCardClass, *FString::Printf(TEXT("Button_Slot%d"), Number), true);
		AddCentered(Cards, Card, false, VAlign_Center, FMargin(Number > 1 ? 36.f : 0.f, 0.f, 0.f, 0.f));
	}
	UVerticalBoxSlot* const CardsSlot = SaveColumn->AddChildToVerticalBox(Cards);
	CardsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CardsSlot->SetHorizontalAlignment(HAlign_Center);
	CardsSlot->SetVerticalAlignment(VAlign_Center);

	AddToColumn(SaveColumn, NewLine(WidgetBlueprint, TEXT("Img_SaveFooterLine"), false), HAlign_Fill);
	UBorder* const Footer = NewWidget<UBorder>(WidgetBlueprint, TEXT("Border_SaveFooter"));
	Footer->SetBrush(SolidBrush(FLinearColor(Navy900.R, Navy900.G, Navy900.B, 0.92f)));
	Footer->SetPadding(FMargin(HUDMargin.X, 0.f));
	Footer->SetVerticalAlignment(VAlign_Center);
	Footer->SetHorizontalAlignment(HAlign_Left);
	USizeBox* const FooterHeight = NewSizeBox(WidgetBlueprint, TEXT("SizeBox_SaveFooter"), 0.f, 96.f);
	FooterHeight->AddChild(NewKeyHints(WidgetBlueprint, TEXT("Panel_SaveHints"), CaptionStyle, {
		{ { INVTEXT("←"), INVTEXT("→") }, NSLOCTEXT("knightUI", "HintSlot", "슬롯") },
		{ { INVTEXT("Enter") }, NSLOCTEXT("knightUI", "HintSelect", "선택") } }));
	Footer->SetContent(FooterHeight);
	AddToColumn(SaveColumn, Footer, HAlign_Fill);

	// Settings opens as its own full-screen widget; this page only marks the state.
	Pages->AddChild(NewWidget<UOverlay>(WidgetBlueprint, TEXT("Panel_Settings"), true));

	AddBackAndStatus(WidgetBlueprint, Root, ButtonClass, SecondaryButtonStyle, BodyDimStyle);
	EndBuild(WidgetBlueprint);
	return true;
}

bool UknightUIEditorLibrary::BuildMainMenu(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> MenuItemClass, TSubclassOf<UUserWidget> ButtonClass,
	TSubclassOf<UCommonButtonStyle> SecondaryButtonStyle, TSubclassOf<UUserWidget> GlobeClass, UTexture2D* Gradient,
	TSubclassOf<UCommonTextStyle> TitleStyle, TSubclassOf<UCommonTextStyle> BodyStyle, TSubclassOf<UCommonTextStyle> BodyDimStyle,
	TSubclassOf<UCommonTextStyle> CaptionStyle, TSubclassOf<UCommonTextStyle> WarningStyle, TSubclassOf<UCommonTextStyle> NumberStyle)
{
	if (!MenuItemClass || !MenuItemClass->IsChildOf<UknightButtonBase>() || !ButtonClass || !ButtonClass->IsChildOf<UknightButtonBase>() || !GlobeClass
		|| !BeginBuild(WidgetBlueprint, TEXT("BuildMainMenu")))
	{
		return false;
	}
	UCanvasPanel* const Root = NewWidget<UCanvasPanel>(WidgetBlueprint, TEXT("Canvas_Root"));
	WidgetBlueprint->WidgetTree->RootWidget = Root;
	UWidgetSwitcher* const Pages = NewWidget<UWidgetSwitcher>(WidgetBlueprint, TEXT("Switcher_Pages"), true);
	AddFill(Root, Pages);

	// Home (hangar): same text menu as the title.
	UCanvasPanel* const Home = NewWidget<UCanvasPanel>(WidgetBlueprint, TEXT("Panel_Home"), true);
	Pages->AddChild(Home);
	AddToCanvas(Home, NewLeftFade(WidgetBlueprint, TEXT("Img_Fade"), Gradient), FAnchors(0.f, 0.f, 0.f, 1.f), FVector2D::ZeroVector, FMargin(0.f, 0.f, 960.f, 0.f));
	UVerticalBox* const HomeHeader = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("VerticalBox_HomeHeader"));
	AddToColumn(HomeHeader, NewText(WidgetBlueprint, TEXT("Txt_HomeCaption"), CaptionStyle, INVTEXT("HANGAR"), false), HAlign_Left);
	AddToColumn(HomeHeader, NewHUDImage(WidgetBlueprint, TEXT("Img_HomeAccent"), Frost, FVector2D(32.f, 2.f)), HAlign_Left, FMargin(0.f, 10.f, 0.f, 12.f));
	AddToColumn(HomeHeader, NewText(WidgetBlueprint, TEXT("Text_ActiveProfile"), BodyDimStyle, NSLOCTEXT("knightUI", "ActiveProfilePlaceholder", "저장 1"), true), HAlign_Left);
	AddToCanvas(Home, HomeHeader, FAnchors(0.f, 0.5f), FVector2D(0.f, 1.f), FMargin(HUDMargin.X, -16.f, 0.f, 0.f), true);
	AddToCanvas(Home, NewTextMenu(WidgetBlueprint, TEXT("VerticalBox_Menu"), MenuItemClass, {
		{ TEXT("Button_Stage"), NSLOCTEXT("knightUI", "LobbySortie", "출격") },
		{ TEXT("Button_Maintenance"), NSLOCTEXT("knightUI", "LobbyMaintenance", "정비") },
		{ TEXT("Button_Codex"), NSLOCTEXT("knightUI", "LobbyCodex", "도감") },
		{ TEXT("Button_Settings"), NSLOCTEXT("knightUI", "LobbySettings", "설정") },
		{ TEXT("Button_ReturnToTitle"), NSLOCTEXT("knightUI", "LobbyTitle", "타이틀로") } }),
		FAnchors(0.f, 0.5f), FVector2D(0.f, 0.f), FMargin(HUDMargin.X, 8.f, 0.f, 0.f), true);

	// Stage: globe in the middle, info panel beside it (C++ picks the side).
	UCanvasPanel* const Stage = NewWidget<UCanvasPanel>(WidgetBlueprint, TEXT("Panel_Stage"), true);
	Pages->AddChild(Stage);
	UImage* const StageDim = NewWidget<UImage>(WidgetBlueprint, TEXT("Img_StageDim"));
	StageDim->SetBrush(SolidBrush(FLinearColor::White));
	StageDim->SetColorAndOpacity(FLinearColor(Navy900.R * 0.6f, Navy900.G * 0.6f, Navy900.B * 0.6f, 0.82f));
	AddFill(Stage, StageDim);
	UVerticalBox* const StageHeader = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("VerticalBox_StageHeader"));
	AddToColumn(StageHeader, NewText(WidgetBlueprint, TEXT("Txt_StageCaption"), CaptionStyle, INVTEXT("OPERATION MAP"), false), HAlign_Left);
	AddToColumn(StageHeader, NewHUDImage(WidgetBlueprint, TEXT("Img_StageAccent"), Frost, FVector2D(32.f, 2.f)), HAlign_Left, FMargin(0.f, 10.f, 0.f, 12.f));
	AddToColumn(StageHeader, NewText(WidgetBlueprint, TEXT("Txt_StageHeading"), TitleStyle, NSLOCTEXT("knightUI", "StageHeading", "작전 지역 선택"), false), HAlign_Left);
	AddToCanvas(Stage, StageHeader, FAnchors(0.f, 0.f), FVector2D::ZeroVector, FMargin(HUDMargin.X, HUDMargin.Y, 0.f, 0.f), true);

	UUserWidget* const Globe = NewWidget<UUserWidget>(WidgetBlueprint, GlobeClass, TEXT("Globe_Stages"), true);
	AddToCanvas(Stage, Globe, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FMargin(0.f, 10.f, 700.f, 700.f));

	USizeBox* const Info = NewSizeBox(WidgetBlueprint, TEXT("Panel_StageInfo"), 420.f, 0.f);
	Info->bIsVariable = true;
	AddToCanvas(Stage, Info, FAnchors(0.5f, 0.5f), FVector2D(0.f, 0.5f), FMargin(414.f, 10.f, 0.f, 0.f), true);
	UBorder* const InfoFrame = NewWidget<UBorder>(WidgetBlueprint, TEXT("Border_StageInfo"));
	InfoFrame->SetBrush(FrameBrush(0.92f));
	InfoFrame->SetPadding(FMargin(32.f, 28.f, 32.f, 28.f));
	Info->AddChild(InfoFrame);
	UVerticalBox* const InfoColumn = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("VerticalBox_StageInfo"));
	InfoFrame->SetContent(InfoColumn);
	AddToColumn(InfoColumn, NewText(WidgetBlueprint, TEXT("Txt_InfoCaption"), CaptionStyle, INVTEXT("MISSION INFO"), false), HAlign_Left);
	AddToColumn(InfoColumn, NewHUDImage(WidgetBlueprint, TEXT("Img_InfoAccent"), Ember, FVector2D(32.f, 2.f)), HAlign_Left, FMargin(0.f, 10.f, 0.f, 12.f));
	UCommonTextBlock* const StageTitle = NewText(WidgetBlueprint, TEXT("Text_StageTitle"), TitleStyle, NSLOCTEXT("knightUI", "StageTitlePlaceholder", "잔열 방어전"), true);
	StageTitle->SetAutoWrapText(true);
	AddToColumn(InfoColumn, StageTitle, HAlign_Fill);
	UImage* const Thumbnail = NewWidget<UImage>(WidgetBlueprint, TEXT("Img_StageThumbnail"), true);
	Thumbnail->SetBrush(SolidBrush(FrostDim, FVector2D(356.f, 180.f)));
	Thumbnail->SetVisibility(ESlateVisibility::Collapsed);
	AddToColumn(InfoColumn, Thumbnail, HAlign_Fill, FMargin(0.f, 16.f, 0.f, 0.f));
	AddCaptionedValue(WidgetBlueprint, InfoColumn, TEXT("Text_StageRegion"), INVTEXT("COMBAT ZONE"), NSLOCTEXT("knightUI", "StageRegionPlaceholder", "북부 폐허 도시"), CaptionStyle, BodyStyle, 20.f);
	AddCaptionedValue(WidgetBlueprint, InfoColumn, TEXT("Text_StageTarget"), INVTEXT("TARGET"), NSLOCTEXT("knightUI", "StageTargetPlaceholder", "잔열"), CaptionStyle, BodyStyle, 14.f);
	UHorizontalBox* const Numbers = NewWidget<UHorizontalBox>(WidgetBlueprint, TEXT("HorizontalBox_StageNumbers"));
	UVerticalBox* const TimeColumn = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("VerticalBox_StageTime"));
	AddCaptionedValue(WidgetBlueprint, TimeColumn, TEXT("Text_StageTimeLimit"), INVTEXT("TIME LIMIT"), INVTEXT("15:00"), CaptionStyle, NumberStyle, 0.f);
	AddCentered(Numbers, TimeColumn, true, VAlign_Top);
	UVerticalBox* const RecordColumn = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("VerticalBox_StageRecord"));
	AddCaptionedValue(WidgetBlueprint, RecordColumn, TEXT("Text_StageRecord"), INVTEXT("BEST RECORD"), INVTEXT("03:42.15"), CaptionStyle, NumberStyle, 0.f);
	AddCentered(Numbers, RecordColumn, true, VAlign_Top);
	AddToColumn(InfoColumn, Numbers, HAlign_Fill, FMargin(0.f, 14.f, 0.f, 0.f));
	AddToColumn(InfoColumn, NewLine(WidgetBlueprint, TEXT("Img_InfoDivider"), false), HAlign_Fill, FMargin(0.f, 18.f, 0.f, 0.f));
	AddToColumn(InfoColumn, NewText(WidgetBlueprint, TEXT("Txt_DetailCaption"), CaptionStyle, INVTEXT("DETAIL"), false), HAlign_Left, FMargin(0.f, 16.f, 0.f, 0.f));
	UCommonTextBlock* const Description = NewText(WidgetBlueprint, TEXT("Text_StageDescription"), BodyDimStyle, NSLOCTEXT("knightUI", "StageDescriptionPlaceholder", "잔열 방어전의 전투 지역으로 출격합니다."), true);
	Description->SetAutoWrapText(true);
	AddToColumn(InfoColumn, Description, HAlign_Fill, FMargin(0.f, 6.f, 0.f, 0.f));
	UCommonTextBlock* const Lock = NewText(WidgetBlueprint, TEXT("Text_StageLock"), WarningStyle, NSLOCTEXT("knightUI", "StageLockPlaceholder", "이전 지역을 먼저 토벌해야 합니다."), true);
	Lock->SetAutoWrapText(true);
	Lock->SetVisibility(ESlateVisibility::Collapsed);
	AddToColumn(InfoColumn, Lock, HAlign_Fill, FMargin(0.f, 16.f, 0.f, 0.f));
	UknightButtonBase* const Launch = NewWidget<UknightButtonBase>(WidgetBlueprint, TSubclassOf<UknightButtonBase>(ButtonClass), TEXT("Button_LaunchStage"), true);
	Launch->SetButtonText(NSLOCTEXT("knightUI", "StageLaunch", "출격"));
	AddToColumn(InfoColumn, Launch, HAlign_Fill, FMargin(0.f, 24.f, 0.f, 0.f));

	AddToCanvas(Stage, NewKeyHints(WidgetBlueprint, TEXT("Panel_StageHints"), CaptionStyle, {
		{ { NSLOCTEXT("knightUI", "KeyDrag", "드래그") }, NSLOCTEXT("knightUI", "HintRotate", "회전") },
		{ { NSLOCTEXT("knightUI", "KeyClick", "클릭") }, NSLOCTEXT("knightUI", "HintSelectRegion", "지역 선택") },
		{ { INVTEXT("←"), INVTEXT("→") }, NSLOCTEXT("knightUI", "HintCycleRegion", "지역 전환") } }),
		FAnchors(0.f, 1.f), FVector2D(0.f, 1.f), FMargin(HUDMargin.X, -96.f, 0.f, 0.f), true);

	// Placeholder pages; codex fills Panel_Codex at runtime, settings opens over everything.
	struct FPlaceholder { const TCHAR* Name; FText Label; };
	for (const FPlaceholder& Page : {
		FPlaceholder{ TEXT("Panel_Maintenance"), NSLOCTEXT("knightUI", "MaintenanceSoon", "정비 기능 준비 중") },
		FPlaceholder{ TEXT("Panel_Settings"), FText::GetEmpty() },
		FPlaceholder{ TEXT("Panel_Codex"), NSLOCTEXT("knightUI", "CodexSoon", "도감 기능 준비 중") } })
	{
		UVerticalBox* const Panel = NewWidget<UVerticalBox>(WidgetBlueprint, Page.Name, true);
		Pages->AddChild(Panel);
		if (!Page.Label.IsEmpty())
		{
			UVerticalBoxSlot* const LabelSlot = Panel->AddChildToVerticalBox(NewText(WidgetBlueprint, *FString::Printf(TEXT("Txt_%s"), Page.Name + 6), BodyDimStyle, Page.Label, false));
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetHorizontalAlignment(HAlign_Center);
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
	}

	AddBackAndStatus(WidgetBlueprint, Root, ButtonClass, SecondaryButtonStyle, BodyDimStyle);
	EndBuild(WidgetBlueprint);
	return true;
}

bool UknightUIEditorLibrary::BuildLoadingScreen(UWidgetBlueprint* WidgetBlueprint, UTexture2D* Mark, TSubclassOf<UCommonTextStyle> TipStyle)
{
	if (!BeginBuild(WidgetBlueprint, TEXT("BuildLoadingScreen")))
	{
		return false;
	}
	UCanvasPanel* const Root = NewWidget<UCanvasPanel>(WidgetBlueprint, TEXT("Canvas_Root"));
	WidgetBlueprint->WidgetTree->RootWidget = Root;
	AddFill(Root, NewHUDImage(WidgetBlueprint, TEXT("Img_Background"), FLinearColor(FColor(0x05, 0x08, 0x0D)), FVector2D(32.f, 32.f)));

	// Mark and tip, a little below the centre like the reference.
	UVerticalBox* const Center = NewWidget<UVerticalBox>(WidgetBlueprint, TEXT("VerticalBox_Center"));
	AddToCanvas(Root, Center, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.f), FMargin(0.f, -40.f, 0.f, 0.f), true);
	UVerticalBoxSlot* const MarkSlot = Center->AddChildToVerticalBox(NewTextureImage(WidgetBlueprint, TEXT("Img_Mark"), Mark, White, FVector2D(72.f, 96.f)));
	MarkSlot->SetHorizontalAlignment(HAlign_Center);
	UCommonTextBlock* const Tip = NewText(WidgetBlueprint, TEXT("Txt_Tip"), TipStyle, NSLOCTEXT("knightUI", "LoadingTipPlaceholder", "로딩 중에 보여 줄 팁 문구입니다."), true);
	Tip->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* const TipSlot = Center->AddChildToVerticalBox(Tip);
	TipSlot->SetHorizontalAlignment(HAlign_Center);
	TipSlot->SetPadding(FMargin(0.f, 28.f, 0.f, 0.f));

	// Three rounded dots; C++ lights them in turn.
	UHorizontalBox* const Dots = NewWidget<UHorizontalBox>(WidgetBlueprint, TEXT("HorizontalBox_Dots"));
	AddToCanvas(Root, Dots, FAnchors(1.f, 1.f), FVector2D(1.f, 1.f), FMargin(-HUDMargin.X, -HUDMargin.Y, 0.f, 0.f), true);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UImage* const Dot = NewWidget<UImage>(WidgetBlueprint, *FString::Printf(TEXT("Img_Dot_%d"), Index), true);
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.ImageSize = FVector2D(12.f, 12.f);
		Brush.TintColor = FSlateColor(White);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(3.f, 3.f, 3.f, 3.f), FSlateColor(FLinearColor::Transparent), 0.f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Dot->SetBrush(Brush);
		Dot->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddCentered(Dots, Dot, false, VAlign_Center, FMargin(Index > 0 ? 6.f : 0.f, 0.f, 0.f, 0.f));
	}

	EndBuild(WidgetBlueprint);
	return true;
}

bool UknightUIEditorLibrary::AddTitleIntro(UWidgetBlueprint* WidgetBlueprint, UTexture2D* StudioLogo, UTexture2D* EngineLogo, TSubclassOf<UCommonTextStyle> PressKeyStyle)
{
	UWidgetTree* const Tree = WidgetBlueprint ? WidgetBlueprint->WidgetTree.Get() : nullptr;
	UCanvasPanel* const Root = Tree ? Cast<UCanvasPanel>(Tree->RootWidget) : nullptr;
	UCanvasPanel* const Home = Tree ? Cast<UCanvasPanel>(Tree->FindWidget(TEXT("Panel_Home"))) : nullptr;
	if (!Root || !Home || !StudioLogo || !EngineLogo)
	{
		UE_LOG(LogTemp, Warning, TEXT("AddTitleIntro: needs the title menu layout (Canvas_Root, Panel_Home) and both logos."));
		return false;
	}
	RemoveWidget(WidgetBlueprint, TEXT("Panel_Intro"));
	RemoveWidget(WidgetBlueprint, TEXT("Txt_PressKey"));
	WidgetBlueprint->Modify();
	Tree->Modify();

	// Where the text menu starts, lined up with the menu labels (3px bar + 18px gap).
	UCommonTextBlock* const PressKey = NewText(WidgetBlueprint, TEXT("Txt_PressKey"), PressKeyStyle, INVTEXT("PRESS ANY KEY"), true);
	PressKey->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddToCanvas(Home, PressKey, FAnchors(0.f, 0.5f), FVector2D(0.f, 0.f), FMargin(HUDMargin.X + 21.f, 64.f, 0.f, 0.f), true);

	// Last child of the root, so it covers the pages, the back button and the status line. The backdrop also takes the
	// clicks until the menu is shown; C++ hides the whole panel after that.
	UOverlay* const Intro = NewWidget<UOverlay>(WidgetBlueprint, TEXT("Panel_Intro"), true);
	AddFill(Root, Intro);
	UImage* const Backdrop = NewWidget<UImage>(WidgetBlueprint, TEXT("Img_IntroBackdrop"), true);
	Backdrop->SetBrush(SolidBrush(FLinearColor::White));
	Backdrop->SetColorAndOpacity(FLinearColor(FColor(0x05, 0x08, 0x0D)));
	AddToOverlay(Intro, Backdrop, HAlign_Fill, VAlign_Fill);
	// The logos scale slowly; with pixel snapping they would move in whole-pixel steps.
	UImage* const Studio = NewTextureImage(WidgetBlueprint, TEXT("Img_StudioLogo"), StudioLogo, FLinearColor::White, FVector2D(512.f, 512.f));
	Studio->bIsVariable = true;
	Studio->SetPixelSnapping(EWidgetPixelSnapping::Disabled);
	AddToOverlay(Intro, Studio, HAlign_Center, VAlign_Center);
	UImage* const Engine = NewTextureImage(WidgetBlueprint, TEXT("Img_EngineLogo"), EngineLogo, FLinearColor::White, FVector2D(220.f, 220.f));
	Engine->bIsVariable = true;
	Engine->SetPixelSnapping(EWidgetPixelSnapping::Disabled);
	AddToOverlay(Intro, Engine, HAlign_Center, VAlign_Center);

	EndBuild(WidgetBlueprint);
	return true;
}
