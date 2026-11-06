#include "Menu/UI/KnightOperationMapTransition.h"
#include "Menu/UI/KnightMainMenuWidget.h"
#include "Menu/UI/KnightStageMapWidget.h"
#include "Menu/UI/KnightQuestMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "Components/CanvasPanel.h"
#include "Components/RetainerBox.h"

namespace KnightOperationMapMotion
{
	float Smooth(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }
	float Out(float T) { T = FMath::Clamp(T, 0.f, 1.f); return 1.f - FMath::Pow(1.f - T, 3.f); }
	float InOut(float T) { return T < .5f ? 4.f*T*T*T : 1.f - FMath::Pow(-2.f*T + 2.f, 3.f)*.5f; }
}

void UKnightOperationMapTransition::PrepareHome()
{
	if (HomeOverlay || !Owner || !Owner->WidgetTree) return;
	auto* Home = Cast<UCanvasPanel>(Owner->Panel_Home);
	auto* Root = Cast<UCanvasPanel>(Owner->WidgetTree->RootWidget);
	auto* PagesSlot = Home && Home->GetParent() ? Cast<UCanvasPanelSlot>(Home->GetParent()->Slot) : nullptr;
	if (!Home || !Root || !PagesSlot) return;
	HomeOverlay = Owner->WidgetTree->ConstructWidget<UCanvasPanel>();
	HomeOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	HomeOverlay->SetRenderOpacity(HomeAlpha);
	auto* OverlaySlot = Root->AddChildToCanvas(HomeOverlay);
	OverlaySlot->SetLayout(PagesSlot->GetLayout()); OverlaySlot->SetZOrder(PagesSlot->GetZOrder() - 1);
	for (UWidget* Child : Home->GetAllChildren())
	{
		auto* Slot = Cast<UCanvasPanelSlot>(Child->Slot); if (!Slot) continue;
		FKnightMapHomeChild Entry; Entry.Widget = Child; Entry.Layout = Slot->GetLayout();
		Entry.ZOrder = Slot->GetZOrder(); Entry.bAutoSize = Slot->GetAutoSize(); HomeChildren.Add(Entry);
		const TSharedRef<SWidget> KeepSlateAlive = Child->TakeWidget();
		Child->RemoveFromParent(); Slot = HomeOverlay->AddChildToCanvas(Child);
		Slot->SetLayout(Entry.Layout); Slot->SetZOrder(Entry.ZOrder); Slot->SetAutoSize(Entry.bAutoSize);
	}
	HomeOverlay->ForceLayoutPrepass();
}

void UKnightOperationMapTransition::AddTrack(UWidget* Widget, float SlideFraction, bool bReward)
{
	if (!Widget) return;
	FKnightMapTransitionTrack Track; Track.Widget = Widget; Track.Transform = Widget->GetRenderTransform();
	Track.Opacity = Widget->GetRenderOpacity(); Track.SlideFraction = SlideFraction; Track.bReward = bReward;
	Tracks.Add(Track);
}

void UKnightOperationMapTransition::Prepare(UKnightMainMenuWidget* Menu)
{
	Owner = Menu; Settings = Menu->OperationMapTransitionSettings;
	StageVisibility = Owner->Panel_Stage->GetVisibility();
	BackVisibility = Owner->Button_Back->GetVisibility();
	AddTrack(Owner->Map_Stages, -Settings.MapSlideFraction);
	AddTrack(Composite(Owner->QuestWidget), Settings.RewardSlideFraction, true);
	AddTrack(Owner->WidgetTree->FindWidget(TEXT("Img_StageDim")));
	AddTrack(Owner->WidgetTree->FindWidget(TEXT("VerticalBox_StageHeader")));
	AddTrack(Owner->WidgetTree->FindWidget(TEXT("Panel_StageHints")));
	AddTrack(Composite(Owner->Button_Back));
	Owner->Panel_Stage->SetVisibility(ESlateVisibility::HitTestInvisible);
	Owner->Button_Back->SetVisibility(ESlateVisibility::HitTestInvisible);
	Owner->SetFocus();
}

UWidget* UKnightOperationMapTransition::Composite(UWidget* Content)
{
	if (!Content) return nullptr;
	for (auto& Layer : Composites) if (Layer.Content == Content)
	{
		Layer.Group->SetRetainRendering(true); Layer.Group->RequestRender(); return Layer.Group;
	}
	auto* Slot = Cast<UCanvasPanelSlot>(Content->Slot);
	auto* Parent = Cast<UCanvasPanel>(Content->GetParent());
	if (!Slot || !Parent) return Content;
	FKnightMapCompositeLayer Layer;
	Layer.Content = Content; Layer.Parent = Parent; Layer.Layout = Slot->GetLayout();
	Layer.ZOrder = Slot->GetZOrder(); Layer.bAutoSize = Slot->GetAutoSize();
	Layer.Group = Owner->WidgetTree->ConstructWidget<URetainerBox>();
	const TSharedRef<SWidget> KeepSlateAlive = Content->TakeWidget();
	Content->RemoveFromParent(); Layer.Group->SetContent(Content);
	Slot = Parent->AddChildToCanvas(Layer.Group); Slot->SetLayout(Layer.Layout);
	Slot->SetZOrder(Layer.ZOrder); Slot->SetAutoSize(Layer.bAutoSize);
	Layer.Group->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Layer.Group->SetRenderingPhase(0,1); Layer.Group->RequestRender(); Layer.Group->ForceLayoutPrepass();
	Composites.Add(Layer); return Layer.Group;
}

void UKnightOperationMapTransition::ReleaseComposites()
{
	for (auto& Layer : Composites)
	{
		Layer.Group->SetVisibility(ESlateVisibility::Collapsed);
		const TSharedRef<SWidget> KeepSlateAlive = Layer.Content->TakeWidget();
		Layer.Group->SetContent(nullptr); Layer.Group->RemoveFromParent(); Layer.Group->ReleaseSlateResources(true);
		auto* Slot = Layer.Parent->AddChildToCanvas(Layer.Content);
		Slot->SetLayout(Layer.Layout); Slot->SetZOrder(Layer.ZOrder); Slot->SetAutoSize(Layer.bAutoSize);
	}
	Composites.Reset();
}

void UKnightOperationMapTransition::Enter(UKnightMainMenuWidget* Menu)
{
	if (IsActive()) return;
	Prepare(Menu); Phase = EPhase::Entering; Elapsed = 0.f;
	MapAlpha = RewardAlpha = Reveal = Scan = 0.f; HomeAlpha = 1.f; MapOffset = RewardOffset = 1.f;
	PrepareHome(); Apply(); // Set every track before the newly activated page can paint.
}

void UKnightOperationMapTransition::Exit(UKnightMainMenuWidget* Menu)
{
	if (IsExiting()) return;
	if (!IsActive())
	{
		Prepare(Menu); MapAlpha = RewardAlpha = Reveal = 1.f;
		MapOffset = RewardOffset = HomeAlpha = Scan = 0.f;
		PrepareHome();
	}
	StartMapAlpha = MapAlpha; StartRewardAlpha = RewardAlpha; StartHomeAlpha = HomeAlpha;
	StartReveal = Reveal; StartMapOffset = MapOffset; StartRewardOffset = RewardOffset; StartScan = Scan;
	Phase = EPhase::Exiting; Elapsed = 0.f;
	Owner->Panel_Stage->SetVisibility(ESlateVisibility::HitTestInvisible);
	Owner->Button_Back->SetVisibility(ESlateVisibility::HitTestInvisible); Owner->SetFocus();
	Apply(); // Reversing entry preserves exactly the last rendered values.
}

void UKnightOperationMapTransition::Apply()
{
	for (const auto& Track : Tracks)
	{
		FWidgetTransform Transform = Track.Transform;
		const double Width = Track.Widget->GetCachedGeometry().GetLocalSize().X;
		Transform.Translation.X += Width * Track.SlideFraction * (Track.bReward ? RewardOffset : MapOffset);
		Track.Widget->SetRenderTransform(Transform);
		Track.Widget->SetRenderOpacity(Track.Opacity * (Track.bReward ? RewardAlpha : MapAlpha));
	}
	if (HomeOverlay) HomeOverlay->SetRenderOpacity(HomeAlpha);
	if (Owner->Map_Stages) Owner->Map_Stages->SetTransitionReveal(Reveal, Scan, Settings.ScanWidth, Settings.ScanGlowWidth, true, IsExiting());
	// Back remains a real action during entry; the map and all other controls are inert.
	if (!IsExiting() && MapAlpha > .2f) Owner->Button_Back->SetVisibility(BackVisibility);
}

void UKnightOperationMapTransition::Tick(float DeltaTime)
{
	if (!IsActive() || !Owner) return;
	// Opacity-zero widgets still arrange. Wait for actual sizes on the very first
	// opening instead of exposing a frame at a guessed/fixed slide distance.
	if (!IsExiting() && Elapsed == 0.f)
		for (const auto& Track : Tracks)
			if (Track.SlideFraction != 0.f && Track.Widget->GetCachedGeometry().GetLocalSize().X <= 0.f) return;
	Elapsed += FMath::Max(0.f, DeltaTime);
	const float T = FMath::Clamp(Elapsed / FMath::Max(.01f, IsExiting() ? Settings.ExitDuration : Settings.EnterDuration), 0.f, 1.f);
	if (!IsExiting())
	{
		const float R = FMath::Clamp((Elapsed - Settings.RewardDelay) / FMath::Max(.01f, Settings.EnterDuration - Settings.RewardDelay), 0.f, 1.f);
		MapOffset = 1.f - KnightOperationMapMotion::Out(T); RewardOffset = 1.f - KnightOperationMapMotion::Out(R);
		MapAlpha = KnightOperationMapMotion::Smooth(T / .65f); RewardAlpha = KnightOperationMapMotion::Smooth(R);
		HomeAlpha = 1.f - KnightOperationMapMotion::Smooth(T / .55f); Reveal = KnightOperationMapMotion::Out(T);
		Scan = Settings.ScanIntensity * KnightOperationMapMotion::Smooth(T / .12f) * (1.f - KnightOperationMapMotion::Smooth((T - .65f) / .35f));
	}
	else
	{
		const float Move = KnightOperationMapMotion::InOut(T), Fade = 1.f - KnightOperationMapMotion::Smooth(T / .92f);
		MapOffset = FMath::Lerp(StartMapOffset, 1.f, Move); RewardOffset = FMath::Lerp(StartRewardOffset, 1.f, Move);
		MapAlpha = StartMapAlpha * Fade; RewardAlpha = StartRewardAlpha * Fade; Reveal = StartReveal * (1.f - Move);
		HomeAlpha = FMath::Lerp(StartHomeAlpha, 1.f, KnightOperationMapMotion::Out((T - .25f * StartMapAlpha) / (1.f - .25f * StartMapAlpha)));
		Scan = (StartScan * (1.f - KnightOperationMapMotion::Smooth(T / .35f)) + Settings.ScanIntensity * .55f * KnightOperationMapMotion::Smooth(T / .18f)) * Fade;
	}
	Apply();
	if (T >= 1.f)
	{
		const bool bExit = IsExiting();
		// Hidden before restoring defaults: no one-frame flash of the outgoing map.
		if (bExit) Owner->Panel_Stage->SetVisibility(ESlateVisibility::Collapsed);
		Restore();
		Owner->CompleteOperationMapTransition(bExit);
	}
}

void UKnightOperationMapTransition::Restore()
{
	if (!Owner) return;
	for (const auto& Track : Tracks)
	{
		Track.Widget->SetRenderTransform(Track.Transform); Track.Widget->SetRenderOpacity(Track.Opacity);
	}
	Tracks.Reset();
	// Rounded-box outline alpha can ignore parent tint. Composite the whole group
	// only while fading. Keep wrappers stable at entry completion (direct painting,
	// no ongoing render-target updates), then unwrap while outgoing widgets are hidden.
	if (IsExiting()) { Owner->Button_Back->SetVisibility(ESlateVisibility::Collapsed); ReleaseComposites(); }
	else for (auto& Layer : Composites) Layer.Group->SetRetainRendering(false);
	if (Owner->Map_Stages) Owner->Map_Stages->SetTransitionReveal(1.f, 0.f, Settings.ScanWidth, Settings.ScanGlowWidth, false, false);
	if (HomeOverlay) HomeOverlay->SetVisibility(ESlateVisibility::Collapsed);
	auto* Home = Cast<UCanvasPanel>(Owner->Panel_Home);
	for (const auto& Child : HomeChildren)
	{
		const TSharedRef<SWidget> KeepSlateAlive = Child.Widget->TakeWidget();
		Child.Widget->RemoveFromParent(); auto* Slot = Home->AddChildToCanvas(Child.Widget);
		Slot->SetLayout(Child.Layout); Slot->SetZOrder(Child.ZOrder); Slot->SetAutoSize(Child.bAutoSize);
	}
	HomeChildren.Reset();
	if (HomeOverlay) { HomeOverlay->RemoveFromParent(); HomeOverlay->ReleaseSlateResources(true); HomeOverlay = nullptr; }
	if (Owner->Panel_Stage->GetVisibility() != ESlateVisibility::Collapsed) Owner->Panel_Stage->SetVisibility(StageVisibility);
	Owner->Button_Back->SetVisibility(BackVisibility);
	Owner->ForceLayoutPrepass(); Phase = EPhase::Idle;
}

void UKnightOperationMapTransition::Cancel()
{
	if (IsActive()) Restore();
	ReleaseComposites();
}
