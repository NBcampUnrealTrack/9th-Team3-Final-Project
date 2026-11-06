#include "Settings/KnightSettingsExitTransition.h"
#include "Settings/KnightSettingsWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace KnightSettingsMotion
{
	float Smooth(float T) { T=FMath::Clamp(T,0.f,1.f); return T*T*(3.f-2.f*T); }
	float Out(float T) { T=FMath::Clamp(T,0.f,1.f); return 1.f-FMath::Pow(1.f-T,3.f); }
	float InOut(float T) { return T<.5f ? 4.f*T*T*T : 1.f-FMath::Pow(-2.f*T+2.f,3.f)*.5f; }
}

// Observe held input from Open, before the close-initiating event reaches UMG. During
// departure consume input; afterwards consume only repeats/releases belonging to that
// departure. A newly pressed key works immediately. No world pause/timer dependency.
class FKnightSettingsExitInput : public IInputProcessor, public TSharedFromThis<FKnightSettingsExitInput>
{
public:
	bool bBlocking=false, bReleased=false;
	TSet<FKey> Held;
	void Release() { bBlocking=false; bReleased=true; }
	void Detach() { if(FSlateApplication::IsInitialized())FSlateApplication::Get().UnregisterInputPreProcessor(AsShared()); }
	virtual void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override { if(bReleased&&Held.IsEmpty())Detach(); }
	virtual bool HandleKeyDownEvent(FSlateApplication&,const FKeyEvent& E) override
	{
		if(bReleased)
		{
			if(!E.IsRepeat()) { Held.Remove(E.GetKey()); return false; }
			return Held.Contains(E.GetKey());
		}
		Held.Add(E.GetKey()); return bBlocking;
	}
	virtual bool HandleKeyUpEvent(FSlateApplication&,const FKeyEvent& E) override
	{ const bool WasHeld=Held.Remove(E.GetKey())>0; return bBlocking||(bReleased&&WasHeld); }
	virtual bool HandleMouseButtonDownEvent(FSlateApplication&,const FPointerEvent& E) override
	{ if(bReleased) { Held.Remove(E.GetEffectingButton());return false; } Held.Add(E.GetEffectingButton());return bBlocking; }
	virtual bool HandleMouseButtonUpEvent(FSlateApplication&,const FPointerEvent& E) override
	{ const bool WasHeld=Held.Remove(E.GetEffectingButton())>0;return bBlocking||(bReleased&&WasHeld); }
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& App,const FPointerEvent& E) override { return HandleMouseButtonDownEvent(App,E); }
	virtual bool HandleMouseMoveEvent(FSlateApplication&,const FPointerEvent&) override { return bBlocking; }
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication&,const FPointerEvent&,const FPointerEvent*) override { return bBlocking; }
	virtual bool HandleAnalogInputEvent(FSlateApplication&,const FAnalogInputEvent&) override { return bBlocking; }
};

FKnightSettingsExitTransition::FKnightSettingsExitTransition(UKnightSettingsWidget* InOwner) : Owner(InOwner)
{
	Input=MakeShared<FKnightSettingsExitInput>();
	if(FSlateApplication::IsInitialized())FSlateApplication::Get().RegisterInputPreProcessor(Input,0);
}
FKnightSettingsExitTransition::~FKnightSettingsExitTransition() { Cancel(); }

void FKnightSettingsExitTransition::Start()
{
	UKnightSettingsWidget* Screen=Owner.Get();
	if(bActive||!Screen)return;
	bActive=true; Elapsed=0.f;
	ScreenVisibility=Screen->GetVisibility();
	Screen->SetVisibility(ESlateVisibility::HitTestInvisible);
	// No disabled tint, row rebuild, or focus transfer to the caller while departing.
	if(Input)Input->bBlocking=true;
	auto Track=[&](const TCHAR* Name,float Direction,bool bChrome)
	{
		if(UWidget* W=Screen->GetWidgetFromName(Name))
			Tracks.Add({W,W->GetRenderTransform(),W->GetRenderOpacity(),Direction,bChrome});
	};
	Track(TEXT("Scroll_Rows"),-1,false); Track(TEXT("Size_Detail"),1,false);
	Track(TEXT("Row_Header"),0,true); Track(TEXT("Rule_Header"),0,true);
	Track(TEXT("Rule_Detail"),1,false); Track(TEXT("Rule_Footer"),0,true);
	Track(TEXT("Row_Status"),0,true); Track(TEXT("Panel_Footer"),0,true);
	Track(TEXT("Panel_Prompt"),0,true); Track(TEXT("Img_Backdrop"),0,false);
	ReturnPage=Screen->ReturnMenu;
	if(UWidget* Page=ReturnPage.Get())
	{
		PageOpacity=Page->GetRenderOpacity(); PageVisibility=Page->GetVisibility();
		Page->SetRenderOpacity(0.f); Page->SetVisibility(ESlateVisibility::HitTestInvisible);
		if(auto* Pages=Cast<UWidgetSwitcher>(Page->GetParent()))
		{
			Switcher=Pages; PreviousPage=Pages->GetActiveWidget(); Pages->SetActiveWidget(Page);
		}
	}
	const TWeakPtr<FKnightSettingsExitTransition> Weak=AsShared();
	Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float Dt)
	{
		const auto Self=Weak.Pin(); if(!Self||!Self->bActive)return false;
		Self->Advance(Dt); return Self->bActive;
	}));
}

void FKnightSettingsExitTransition::Advance(float DeltaTime)
{
	UKnightSettingsWidget* Screen=Owner.Get();
	if(!bActive||!Screen)return;
	Elapsed+=FMath::Max(0.f,DeltaTime);
	const float Duration=FMath::Clamp(Screen->ExitDuration,.28f,.34f);
	const float T=FMath::Clamp(Elapsed/Duration,0.f,1.f);
	const float Move=KnightSettingsMotion::InOut(T);
	for(const FTrack& Track:Tracks)if(UWidget* W=Track.Widget.Get())
	{
		FWidgetTransform Transform=Track.Transform;
		Transform.Translation.X+=Screen->ExitSlideDistance*Move*Track.Direction;
		W->SetRenderTransform(Transform);
		// Finish fading before removal. All siblings share group opacity, not per-row tracks.
		const float FadeTime=Track.bChrome ? Screen->ExitChromeFadeDuration : Duration*.94f;
		W->SetRenderOpacity(Track.Opacity*(1.f-KnightSettingsMotion::Smooth(Elapsed/FMath::Max(.01f,FadeTime))));
	}
	Screen->HandleExitChromeOpacity(1.f-KnightSettingsMotion::Smooth(Elapsed/FMath::Max(.01f,Screen->ExitChromeFadeDuration)));
	if(UWidget* Page=ReturnPage.Get())
	{
		const float Delay=Duration*FMath::Clamp(Screen->ReturnMenuFadeDelayFraction,0.f,.9f);
		Page->SetRenderOpacity(PageOpacity*KnightSettingsMotion::Out((Elapsed-Delay)/(Duration-Delay)));
	}
	if(T>=1.f)
	{
		// Hide first, then reset. OnClosed may remove us or synchronously open a new screen.
		Screen->SetVisibility(ESlateVisibility::Collapsed);
		bActive=false; Ticker.Reset();
		Restore(true);
		if(Input) { Input->Release(); Input.Reset(); }
		Screen->CompleteExit();
	}
}

void FKnightSettingsExitTransition::Restore(bool bCompleted)
{
	for(const FTrack& Track:Tracks)if(UWidget* W=Track.Widget.Get())
	{ W->SetRenderTransform(Track.Transform); W->SetRenderOpacity(Track.Opacity); }
	Tracks.Reset();
	if(Owner.IsValid())Owner->HandleExitChromeOpacity(1.f);
	// Restore the switcher's original state before the caller's existing navigation runs.
	if(Switcher.IsValid()&&PreviousPage.IsValid()&&Switcher->GetActiveWidget()==ReturnPage.Get())
		Switcher->SetActiveWidget(PreviousPage.Get());
	if(UWidget* Page=ReturnPage.Get()) { Page->SetRenderOpacity(PageOpacity);Page->SetVisibility(PageVisibility); }
	Switcher.Reset();PreviousPage.Reset();ReturnPage.Reset();
	if(!bCompleted&&Owner.IsValid())Owner->SetVisibility(ScreenVisibility);
}

void FKnightSettingsExitTransition::Cancel()
{
	if(Ticker.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(Ticker); Ticker.Reset(); }
	if(bActive)
	{
		if(Owner.IsValid())Owner->SetVisibility(ESlateVisibility::Collapsed);
		bActive=false; Restore(false);
	}
	if(Input) { Input->Detach(); Input.Reset(); }
}

int32 FKnightSettingsExitTransition::Paint(const FGeometry& G,FSlateWindowElementList& Out,int32 Layer) const
{
	const UKnightSettingsWidget* Screen=Owner.Get();
	if(!bActive||!Screen)return Layer;
	const float Fade=1.f-KnightSettingsMotion::Smooth(Elapsed/(Screen->ExitDuration*.78f));
	const float Pulse=KnightSettingsMotion::Smooth(Elapsed/.045f)*Fade*Screen->ExitEdgeIntensity;
	if(Pulse<=0.f)return Layer;
	for(const TCHAR* Name:{TEXT("Scroll_Rows"),TEXT("Size_Detail")})
	{
		const UWidget* W=Screen->GetWidgetFromName(Name);if(!W)continue;
		const auto& WG=W->GetCachedGeometry();const FVector2D Size=WG.GetLocalSize();
		if(Size.IsNearlyZero())continue;
		const bool bList=FCString::Strcmp(Name,TEXT("Scroll_Rows"))==0;
		const FVector2D Origin=G.AbsoluteToLocal(WG.LocalToAbsolute(FVector2D(bList?Size.X:0.f,0.f)));
		const float Height=G.AbsoluteToLocal(WG.LocalToAbsolute(FVector2D(0.f,Size.Y))).Y-Origin.Y;
		// Soft ends and a narrow halo, entirely inside the screen margins. No clip mask/RT.
		for(int32 Y=0;Y<32;++Y)
		{
			const float V=(Y+.5f)/32.f;
			const float Feather=KnightSettingsMotion::Smooth(FMath::Min(V,1.f-V)/.16f);
			for(int32 Band=0;Band<3;++Band)
			{
				const float Width=Band==0?8.f:Band==1?4.f:1.f;
				FLinearColor Ink(FColor(86,186,221)); Ink.A=Pulse*Feather*(Band==2?1.f:.12f);
				FSlateDrawElement::MakeBox(Out,Layer+1,G.ToPaintGeometry(FVector2D(Width,Height/32.f),FSlateLayoutTransform(Origin+FVector2D(-Width*.5f,Y*Height/32.f))),
					FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Ink);
			}
		}
	}
	return Layer+1;
}
