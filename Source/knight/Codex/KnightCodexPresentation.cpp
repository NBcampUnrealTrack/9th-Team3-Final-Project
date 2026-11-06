#include "Codex/KnightCodexPresentation.h"

#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SLeafWidget.h"

namespace KnightCodexPresentation
{
	FLinearColor Color(const TCHAR* Hex, float Alpha)
	{
		FLinearColor Result(FColor::FromHex(Hex));
		Result.A = Alpha;
		return Result;
	}

	void TextStyle(UTextBlock* Text, int32 Size, bool bStrong, bool bMuted)
	{
		if (!Text) return;
		UFont* Font = LoadObject<UFont>(nullptr, TEXT("/Game/Knight/UI/Style/Fonts/F_Knight_PlexKR.F_Knight_PlexKR"));
		FSlateFontInfo Info(Font, Size, bStrong ? TEXT("SemiBold") : TEXT("Regular"));
		Text->SetFont(Info);
		Text->SetColorAndOpacity(Color(bMuted ? TEXT("91A6B5") : TEXT("E6EEF4")));
		Text->SetShadowOffset(FVector2D::ZeroVector);
		Text->SetAutoWrapText(true);
		Text->SetJustification(ETextJustify::Left);
		Text->SetLineHeightPercentage(1.22f);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

namespace
{
	using namespace KnightCodexPresentation;
	void Line(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry,
		std::initializer_list<FVector2D> Points, FLinearColor Ink, float Thickness = 1.f)
	{
		TArray<FVector2D> Path(Points);
		FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Path, ESlateDrawEffect::None, Ink, true, Thickness);
	}
	void Fill(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, FLinearColor Ink)
	{
		FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(), FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Ink);
	}

	class SCodexSymbol : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SCodexSymbol) {} SLATE_END_ARGS()
		void Construct(const FArguments&, UKnightCodexSymbolWidget* InOwner) { Owner = InOwner; SetCanTick(false); }
		virtual FVector2D ComputeDesiredSize(float) const override { return Owner.IsValid() ? Owner->SymbolSize : FVector2D(40.f); }
		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out,
			int32 Layer, const FWidgetStyle& Style, bool) const override
		{
			// Keep legacy enum values loadable, but only panel framing is drawn now.
			if (!Owner.IsValid() || Owner->Symbol != EKnightCodexSymbol::Frame) return Layer;
			const FLinearColor Ink = Owner->Ink * Style.GetColorAndOpacityTint();
			const FVector2D Size = G.GetLocalSize();
			if (Owner->Symbol == EKnightCodexSymbol::Frame)
			{
				const float X = Size.X - 1.f, Y = Size.Y - 1.f, C = 15.f;
				Line(Out, Layer, G, {{0,C},{0,0},{C*3,0}}, Ink, 1.f);
				Line(Out, Layer, G, {{X-C*3,0},{X-C,0},{X,C},{X,C*3}}, Ink, 1.f);
				Line(Out, Layer, G, {{X,Y-C*3},{X,Y-C},{X-C,Y},{X-C*3,Y}}, Ink, 1.f);
				Line(Out, Layer, G, {{C*3,Y},{0,Y},{0,Y-C*3}}, Ink, 1.f);
				FLinearColor Edge = Ink; Edge.A *= .34f;
				Line(Out, Layer, G, {{C*3,0},{X-C*3,0}}, Edge);
				Line(Out, Layer, G, {{0,C*3},{0,Y-C*3}}, Edge);
				Line(Out, Layer, G, {{X,C*3},{X,Y-C*3}}, Edge);
				Line(Out, Layer, G, {{C*3,Y},{X-C*3,Y}}, Edge);
				if (Owner->bGrid)
				{
					FLinearColor Grid = Ink; Grid.A *= .10f;
					for (float PX = 34.f; PX < X-20.f; PX += 78.f)
						for (float PY = 34.f; PY < Y-20.f; PY += 78.f)
						{
							Line(Out, Layer, G, {{PX-2,PY},{PX+2,PY}}, Grid);
							Line(Out, Layer, G, {{PX,PY-2},{PX,PY+2}}, Grid);
						}
				}
				return Layer;
			}
			return Layer;
		}
	private:
		TWeakObjectPtr<UKnightCodexSymbolWidget> Owner;
	};

	class SCodexArchiveButton : public SButton
	{
	public:
		void Construct(const SButton::FArguments& Args, UKnightCodexArchiveButton* InOwner)
		{
			Owner = InOwner; SButton::Construct(Args); SetCanTick(false);
		}
		// This control paints focus on its existing outer edge; suppress only its
		// default Slate focus brush, keeping keyboard/controller focus functional.
		virtual const FSlateBrush* GetFocusBrush() const override { return nullptr; }
		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Clip,
			FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& PaintStyle, bool ParentEnabled) const override
		{
			const bool Enabled = ParentEnabled && IsEnabled();
			const bool Locked = Owner.IsValid() && Owner->IsRecordLocked();
			const float Selected = Owner.IsValid() && Owner->IsRecordSelected() ? 1.f : 0.f;
			const float Hover = Enabled && IsHovered() ? 1.f : 0.f;
			const FVector2D S = G.GetLocalSize();
			const float X = S.X - 1.f, Y = S.Y - 1.f, C = 9.f;
			FLinearColor Base = Color(TEXT("071018"), Locked ? .60f : .72f);
			FLinearColor Active = Color(TEXT("163548"), .92f);
			FLinearColor Background = FMath::Lerp(Base, Active, Selected);
			Background = FMath::Lerp(Background, Color(TEXT("163246"), .87f), Hover * (1.f - Selected) * .8f);
			if (!Enabled) Background.A *= .55f;
			Fill(Out, Layer, G, Background * PaintStyle.GetColorAndOpacityTint());
			FLinearColor Edge = FMath::Lerp(Color(TEXT("416176"), .55f + .35f*Hover), Color(TEXT("50BDE9"), .95f), Selected);
			if (HasKeyboardFocus()) Edge = Color(TEXT("C0EBFF"));
			Edge *= PaintStyle.GetColorAndOpacityTint();
			Line(Out, Layer+3, G, {{0,0},{X-C,0},{X,C},{X,Y-C},{X-C,Y},{0,Y},{0,0}}, Edge);
			if (Selected > .01f)
			{
				FLinearColor Marker = Color(TEXT("7ADFFF"), Selected);
				Line(Out, Layer+3, G, {{3,5},{3,Y-5}}, Marker, 3.f);
			}
			return SButton::OnPaint(Args, G, Clip, Out, Layer+4, PaintStyle, ParentEnabled);
		}
	private:
		TWeakObjectPtr<UKnightCodexArchiveButton> Owner;
	};
}

void UKnightCodexSymbolWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren); SymbolSlate.Reset();
}
TSharedRef<SWidget> UKnightCodexSymbolWidget::RebuildWidget()
{
	SymbolSlate = SNew(SCodexSymbol, this); return SymbolSlate.ToSharedRef();
}
UKnightCodexArchiveButton::UKnightCodexArchiveButton(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	FButtonStyle Style;
	Style.SetNormal(FSlateNoResource()).SetHovered(FSlateNoResource()).SetPressed(FSlateNoResource()).SetDisabled(FSlateNoResource());
	Style.SetNormalPadding(FMargin()).SetPressedPadding(FMargin());
	SetStyle(Style); InitIsFocusable(true);
}
void UKnightCodexArchiveButton::SetRecordState(bool bSelected, bool bLocked)
{
	bRecordSelected = bSelected; bRecordLocked = bLocked;
	if (MyButton) MyButton->Invalidate(EInvalidateWidgetReason::Paint);
}
TSharedRef<SWidget> UKnightCodexArchiveButton::RebuildWidget()
{
	MyButton = SNew(SCodexArchiveButton, this)
		.OnClicked(BIND_UOBJECT_DELEGATE(FOnClicked, SlateHandleClicked))
		.OnPressed(BIND_UOBJECT_DELEGATE(FSimpleDelegate, SlateHandlePressed))
		.OnReleased(BIND_UOBJECT_DELEGATE(FSimpleDelegate, SlateHandleReleased))
		.OnHovered_UObject(this, &ThisClass::SlateHandleHovered)
		.OnUnhovered_UObject(this, &ThisClass::SlateHandleUnhovered)
		.OnReceivedFocus_UObject(this, &ThisClass::SlateHandleOnReceivedFocus)
		.OnLostFocus_UObject(this, &ThisClass::SlateHandleOnLostFocus)
		.ButtonStyle(&GetStyle()).ClickMethod(GetClickMethod()).TouchMethod(GetTouchMethod())
		.PressMethod(GetPressMethod()).IsFocusable(GetIsFocusable());
	if (GetChildrenCount() > 0) CastChecked<UButtonSlot>(GetContentSlot())->BuildSlot(MyButton.ToSharedRef());
	return MyButton.ToSharedRef();
}
