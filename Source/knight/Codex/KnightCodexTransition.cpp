#include "Codex/KnightCodexTransition.h"
#include "Codex/KnightCodexReveal.h"
#include "Codex/KnightCodexWidget.h"
#include "Menu/UI/KnightMainMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"

namespace KnightCodexMotion
{
	float Smooth(float T) { T=FMath::Clamp(T,0.f,1.f); return T*T*(3.f-2.f*T); }
	float Out(float T) { T=FMath::Clamp(T,0.f,1.f); return 1.f-FMath::Pow(1.f-T,3.f); }
	float InOut(float T) { return T<.5f ? 4.f*T*T*T : 1.f-FMath::Pow(-2.f*T+2.f,3.f)*.5f; }
}

void UKnightCodexTransition::PrepareHome()
{
	// Same real-child overlap pattern as the operation map. No duplicate menu or screenshot.
	auto* Home=Cast<UCanvasPanel>(Owner->Panel_Home);
	auto* Root=Cast<UCanvasPanel>(Owner->WidgetTree->RootWidget);
	auto* PagesSlot=Home && Home->GetParent()?Cast<UCanvasPanelSlot>(Home->GetParent()->Slot):nullptr;
	if(!Home || !Root || !PagesSlot) return;
	HomeOverlay=Owner->WidgetTree->ConstructWidget<UCanvasPanel>();
	HomeOverlay->SetVisibility(ESlateVisibility::HitTestInvisible); HomeOverlay->SetRenderOpacity(HomeAlpha);
	auto* Slot=Root->AddChildToCanvas(HomeOverlay); Slot->SetLayout(PagesSlot->GetLayout()); Slot->SetZOrder(PagesSlot->GetZOrder()-1);
	for(UWidget* Child:Home->GetAllChildren())
	{
		auto* CS=Cast<UCanvasPanelSlot>(Child->Slot); if(!CS) continue;
		FKnightCodexHomeChild Record; Record.Widget=Child; Record.Layout=CS->GetLayout(); Record.ZOrder=CS->GetZOrder(); Record.bAutoSize=CS->GetAutoSize();
		HomeChildren.Add(Record); const TSharedRef<SWidget> KeepAlive=Child->TakeWidget();
		Child->RemoveFromParent(); CS=HomeOverlay->AddChildToCanvas(Child);
		CS->SetLayout(Record.Layout); CS->SetZOrder(Record.ZOrder); CS->SetAutoSize(Record.bAutoSize);
	}
}

void UKnightCodexTransition::Prepare(UKnightMainMenuWidget* Menu)
{
	Owner=Menu; Settings=Menu->CodexTransitionSettings; PageVisibility=Menu->Panel_Codex->GetVisibility();
	auto Track=[&](const TCHAR* Name,float Direction=0.f)
	{
		if(UWidget* W=Owner->CodexWidget->GetWidgetFromName(Name))
		{
			FKnightCodexMotionTrack T; T.Widget=W; T.Transform=W->GetRenderTransform(); T.Opacity=W->GetRenderOpacity(); T.Direction=Direction; Tracks.Add(T);
		}
	};
	Track(TEXT("Row_CodexHeader")); Track(TEXT("Rule_Header")); Track(TEXT("Row_ArchiveFooter")); Track(TEXT("Rule_Footer"));
	Track(TEXT("Border_CodexScrim")); Track(TEXT("Size_EntryList"),-1); Track(TEXT("Size_MonsterInfo"),1);
	Menu->Panel_Codex->SetVisibility(ESlateVisibility::HitTestInvisible); Menu->SetFocus();
	if(auto* Root=Cast<UCanvasPanel>(Menu->WidgetTree->RootWidget))
	{
		Shield=Menu->WidgetTree->ConstructWidget<UKnightCodexMotionShield>();
		auto* Slot=Root->AddChildToCanvas(Shield); Slot->SetAnchors(FAnchors(0,0,1,1)); Slot->SetOffsets(FMargin()); Slot->SetZOrder(10000);
	}
	PrepareHome();
}

void UKnightCodexTransition::Enter(UKnightMainMenuWidget* Menu)
{
	if(IsActive() || !Menu->CodexWidget) return;
	HeaderAlpha=SideAlpha=ContentAlpha=Reveal=Glow=0.f; Offset=HomeAlpha=1.f;
	Prepare(Menu); Phase=EKnightCodexPhase::Entering; Elapsed=0; Apply();
}

void UKnightCodexTransition::Exit(UKnightMainMenuWidget* Menu)
{
	if(Phase==EKnightCodexPhase::Exiting || !Menu->CodexWidget) return;
	if(!IsActive())
	{
		HeaderAlpha=SideAlpha=ContentAlpha=Reveal=1.f; Offset=HomeAlpha=Glow=0;
		Prepare(Menu);
	}
	StartHeader=HeaderAlpha; StartSide=SideAlpha; StartOffset=Offset; StartContent=ContentAlpha;
	StartReveal=Reveal; StartGlow=Glow; StartHome=HomeAlpha;
	Phase=EKnightCodexPhase::Exiting; Elapsed=0; Apply();
}

void UKnightCodexTransition::Apply()
{
	if(!Owner || !Owner->CodexWidget) return;
	for(const auto& Track:Tracks)
	{
		FWidgetTransform Transform=Track.Transform;
		Transform.Translation.X+=Track.Direction*Settings.SlideDistance*Offset;
		Track.Widget->SetRenderTransform(Transform);
		Track.Widget->SetRenderOpacity(Track.Opacity*(Track.Direction==0.f?HeaderAlpha:SideAlpha));
	}
	if(HomeOverlay) HomeOverlay->SetRenderOpacity(HomeAlpha);
	for(const TCHAR* Name:{TEXT("Reveal_World"),TEXT("Reveal_Mech"),TEXT("Reveal_Monster")})
		if(auto* Mask=Cast<UKnightCodexReveal>(Owner->CodexWidget->GetWidgetFromName(Name)))
			Mask->SetReveal(Reveal,ContentAlpha,Glow,Settings.RevealFeather,true,
				FCString::Strcmp(Name,TEXT("Reveal_Monster"))==0?Owner->CodexWidget->GetWidgetFromName(TEXT("Image_Portrait")):nullptr);
}

void UKnightCodexTransition::Tick(float Dt)
{
	if(!IsActive() || !Owner) return;
	Elapsed+=FMath::Max(0.f,Dt);
	const bool Exit=Phase==EKnightCodexPhase::Exiting;
	const float T=FMath::Clamp(Elapsed/FMath::Max(.01f,Exit?Settings.ExitDuration:Settings.EnterDuration),0.f,1.f);
	if(!Exit)
	{
		HeaderAlpha=KnightCodexMotion::Out(Elapsed/FMath::Max(.01f,Settings.HeaderDuration));
		// Move immediately, but let the Home labels clear before the side text becomes readable.
		SideAlpha=KnightCodexMotion::Smooth(T/.65f); Offset=1.f-KnightCodexMotion::Out(T/.8f);
		ContentAlpha=KnightCodexMotion::Smooth(T/.25f); Reveal=KnightCodexMotion::Out(T);
		Glow=Settings.ScanIntensity*KnightCodexMotion::Smooth(T/.12f)*(1.f-KnightCodexMotion::Smooth((T-.65f)/.35f));
		HomeAlpha=1.f-KnightCodexMotion::Smooth(Elapsed/.12f);
	}
	else
	{
		const float Move=KnightCodexMotion::InOut(T), Fade=1.f-KnightCodexMotion::Smooth(T/.96f);
		HeaderAlpha=StartHeader*Fade; SideAlpha=StartSide*Fade; ContentAlpha=StartContent*Fade;
		Offset=FMath::Lerp(StartOffset,1.f,Move); Reveal=StartReveal*(1.f-Move);
		Glow=(StartGlow*(1.f-KnightCodexMotion::Smooth(T/.3f))+Settings.ScanIntensity*.6f*KnightCodexMotion::Smooth(T/.15f))*Fade;
		HomeAlpha=FMath::Lerp(StartHome,1.f,KnightCodexMotion::Out((T-.45f*StartContent)/(1.f-.45f*StartContent)));
	}
	Apply();
	if(T>=1.f)
	{
		if(Exit) Owner->Panel_Codex->SetVisibility(ESlateVisibility::Collapsed);
		Restore(); Phase=Exit?EKnightCodexPhase::Hidden:EKnightCodexPhase::Visible;
		Owner->CompleteCodexTransition(Exit);
	}
}

void UKnightCodexTransition::Restore()
{
	if(!Owner) return;
	for(const auto& Track:Tracks) { Track.Widget->SetRenderTransform(Track.Transform); Track.Widget->SetRenderOpacity(Track.Opacity); }
	Tracks.Reset();
	if(Owner->CodexWidget)
		for(const TCHAR* Name:{TEXT("Reveal_World"),TEXT("Reveal_Mech"),TEXT("Reveal_Monster")})
			if(auto* Mask=Cast<UKnightCodexReveal>(Owner->CodexWidget->GetWidgetFromName(Name))) Mask->SetReveal(1,1,0,Settings.RevealFeather,false);
	if(HomeOverlay) HomeOverlay->SetVisibility(ESlateVisibility::Collapsed);
	if(auto* Home=Cast<UCanvasPanel>(Owner->Panel_Home))
		for(const auto& Record:HomeChildren)
		{
			const TSharedRef<SWidget> KeepAlive=Record.Widget->TakeWidget(); Record.Widget->RemoveFromParent();
			auto* Slot=Home->AddChildToCanvas(Record.Widget); Slot->SetLayout(Record.Layout); Slot->SetZOrder(Record.ZOrder); Slot->SetAutoSize(Record.bAutoSize);
		}
	HomeChildren.Reset();
	if(HomeOverlay) { HomeOverlay->RemoveFromParent(); HomeOverlay->ReleaseSlateResources(true); HomeOverlay=nullptr; }
	if(Shield) { Shield->SetVisibility(ESlateVisibility::Collapsed); Shield->RemoveFromParent(); Shield->ReleaseSlateResources(true); Shield=nullptr; }
	if(Owner->Panel_Codex->GetVisibility()!=ESlateVisibility::Collapsed) Owner->Panel_Codex->SetVisibility(PageVisibility);
	Owner->ForceLayoutPrepass();
}

void UKnightCodexTransition::Cancel()
{
	if(IsActive())
	{
		Owner->Panel_Codex->SetVisibility(ESlateVisibility::Collapsed); Restore();
		Owner->Panel_Codex->SetVisibility(PageVisibility);
	}
	Phase=EKnightCodexPhase::Hidden;
}
