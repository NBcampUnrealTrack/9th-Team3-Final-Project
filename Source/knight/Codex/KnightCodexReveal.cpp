#include "Codex/KnightCodexReveal.h"
#include "Components/PanelSlot.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"

class SKnightCodexReveal : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKnightCodexReveal) {} SLATE_END_ARGS()
	void Construct(const FArguments&, UKnightCodexReveal* InOwner)
	{ Owner=InOwner; SetCanTick(false); }
	void SetContent(const TSharedRef<SWidget>& Content) { ChildSlot[Content]; }
	virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Cull,FSlateWindowElementList& Out,
		int32 Layer,const FWidgetStyle& Style,bool Enabled) const override
	{
		if(!Owner.IsValid() || !Owner->bActive) return SCompoundWidget::OnPaint(Args,G,Cull,Out,Layer,Style,Enabled);
		if(Owner->Alpha<=0.f || Owner->Progress<=0.f) return Layer;
		const FVector2D Size=G.GetLocalSize();
		FVector2D Min=FVector2D::ZeroVector,Max=Size;
		if(UWidget* Target=Owner->Target.Get(); Target && Target->IsVisible() && !Target->GetCachedGeometry().GetLocalSize().IsNearlyZero())
		{
			const FGeometry& TG=Target->GetCachedGeometry();
			Min=G.AbsoluteToLocal(TG.LocalToAbsolute(FVector2D::ZeroVector));
			Max=G.AbsoluteToLocal(TG.LocalToAbsolute(TG.GetLocalSize()));
		}
		const float Feather=FMath::Max(4.f,Owner->Feather);
		const float Edge=FMath::Lerp(Min.X-Feather,Max.X+Feather,Owner->Progress);
		int32 Highest=Layer;
		// Adjacent, non-overlapping clips produce a feather without a render target,
		// scaling, UV modification, or repainting the source texture. No retained stale data.
		auto PaintSlice=[&](float Left,float Right,float Opacity)
		{
			Left=FMath::Clamp(Left,0.f,float(Size.X)); Right=FMath::Clamp(Right,0.f,float(Size.X));
			if(Right<=Left || Opacity<=0.f) return;
			Out.PushClip(FSlateClippingZone(G.MakeChild(FVector2D(Right-Left,Size.Y),FSlateLayoutTransform(FVector2D(Left,0)))));
			FWidgetStyle SliceStyle=Style; SliceStyle.BlendOpacity(Owner->Alpha*Opacity);
			Highest=FMath::Max(Highest,SCompoundWidget::OnPaint(Args,G,Cull,Out,Layer,SliceStyle,Enabled));
			Out.PopClip();
		};
		PaintSlice(0,Edge-Feather,1.f);
		constexpr int32 Slices=24;
		for(int32 I=0;I<Slices;++I)
		{
			const float T=(I+.5f)/Slices;
			PaintSlice(Edge-Feather+Feather*I/Slices,Edge-Feather+Feather*(I+1)/Slices,1.f-T*T*(3.f-2.f*T));
		}
		if(Owner->Glow>0.f && Max.Y>Min.Y)
		{
			Out.PushClip(FSlateClippingZone(G.MakeChild(Max-Min,FSlateLayoutTransform(Min))));
			auto Band=[&](float Left,float Width,float Opacity)
			{
				FLinearColor Ink(FColor(0x63,0xCF,0xF5)); Ink.A=Opacity*Owner->Glow*Owner->Alpha;
				FSlateDrawElement::MakeBox(Out,Highest+1,G.ToPaintGeometry(FVector2D(Width,Max.Y-Min.Y),FSlateLayoutTransform(FVector2D(Left,Min.Y))),
					FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Ink*Style.GetColorAndOpacityTint());
			};
			for(int32 I=0;I<12;++I) { const float D=I/12.f; Band(Edge-22.f+I*2.f,2.f,.09f*(1.f-D)); }
			Band(Edge-4.f,8.f,.08f); Band(Edge-1.f,2.f,.8f);
			Out.PopClip(); ++Highest;
		}
		return Highest;
	}
private:
	TWeakObjectPtr<UKnightCodexReveal> Owner;
};

void UKnightCodexReveal::SetReveal(float InProgress,float InAlpha,float InGlow,float InFeather,bool bInActive,UWidget* InTarget)
{
	Progress=FMath::Clamp(InProgress,0.f,1.f); Alpha=FMath::Clamp(InAlpha,0.f,1.f);
	Glow=InGlow; Feather=InFeather; bActive=bInActive; Target=InTarget;
	if(RevealSlate) RevealSlate->Invalidate(EInvalidateWidgetReason::Paint);
}
TSharedRef<SWidget> UKnightCodexReveal::RebuildWidget()
{
	RevealSlate=SNew(SKnightCodexReveal,this);
	if(GetContent()) RevealSlate->SetContent(GetContent()->TakeWidget());
	return RevealSlate.ToSharedRef();
}
void UKnightCodexReveal::ReleaseSlateResources(bool bChildren) { Super::ReleaseSlateResources(bChildren); RevealSlate.Reset(); }
void UKnightCodexReveal::OnSlotAdded(UPanelSlot* InSlot) { if(RevealSlate) RevealSlate->SetContent(InSlot->Content->TakeWidget()); }
void UKnightCodexReveal::OnSlotRemoved(UPanelSlot*) { if(RevealSlate) RevealSlate->SetContent(SNullWidget::NullWidget); }

class SKnightCodexMotionShield : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SKnightCodexMotionShield) {} SLATE_END_ARGS()
	void Construct(const FArguments&) { SetCanTick(false); }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
	virtual int32 OnPaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32 Layer,const FWidgetStyle&,bool) const override { return Layer; }
	virtual FReply OnMouseButtonDown(const FGeometry&,const FPointerEvent&) override { return FReply::Handled(); }
	virtual FReply OnMouseButtonUp(const FGeometry&,const FPointerEvent&) override { return FReply::Handled(); }
	virtual FReply OnMouseButtonDoubleClick(const FGeometry&,const FPointerEvent&) override { return FReply::Handled(); }
	virtual FReply OnMouseWheel(const FGeometry&,const FPointerEvent&) override { return FReply::Handled(); }
	virtual FReply OnTouchStarted(const FGeometry&,const FPointerEvent&) override { return FReply::Handled(); }
	virtual FReply OnTouchEnded(const FGeometry&,const FPointerEvent&) override { return FReply::Handled(); }
};
TSharedRef<SWidget> UKnightCodexMotionShield::RebuildWidget() { return SNew(SKnightCodexMotionShield); }
