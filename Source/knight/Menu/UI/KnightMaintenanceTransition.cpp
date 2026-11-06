#include "Menu/UI/KnightMaintenanceTransition.h"
#include "Menu/UI/KnightMaintenanceWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/RetainerBox.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	float Smooth(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }
}

void UKnightMaintenanceTransition::SaveOwner(UKnightMaintenanceWidget* InOwner, UWidget* InFooter)
{
	Owner = InOwner; Footer = InFooter; Settings = Owner->EntranceSettings;
	OriginalOpacity = Owner->GetRenderOpacity(); OriginalVisibility = Owner->GetVisibility();
	if (Footer) { FooterOpacity = Footer->GetRenderOpacity(); FooterVisibility = Footer->GetVisibility(); FooterTransform = Footer->GetRenderTransform(); }
	Owner->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UKnightMaintenanceTransition::Begin(UKnightMaintenanceWidget* InOwner, UWidget* InFooter, FSimpleDelegate Finished)
{
	Cancel(); SaveOwner(InOwner, InFooter); OnFinished = MoveTemp(Finished);
	Phase = EPhase::Preparing; PrepareFrames = 1; Elapsed = Progress = 0.f;
	// Give the new destination one arranged frame; no old-layout flash while measuring.
	Owner->SetRenderOpacity(0.f);
	if (Footer) { Footer->SetRenderOpacity(0.f); Footer->SetVisibility(ESlateVisibility::HitTestInvisible); }
}

void UKnightMaintenanceTransition::BeginExit(UKnightMaintenanceWidget* InOwner, UWidget* InFooter, UCanvasPanel* InHome, FSimpleDelegate Finished)
{
	if (IsExiting()) return;
	bExitFromEntrance = IsActive();
	if (!IsActive())
	{
		SaveOwner(InOwner, InFooter);
		if (PrepareLayers()) UpdateEntrance(1.f);
	}
	OnFinished = MoveTemp(Finished);
	ExitStartOpacity = Owner->GetRenderOpacity();
	ExitFooterOpacity = Footer ? Footer->GetRenderOpacity() : 0.f;
	ExitStartOffset = PanelOffset;
	ExitScanIntensity = bExitFromEntrance ? ScanIntensity : 0.f;
	// A partially revealed entrance has less readable content and less remaining
	// travel. Reveal Home earlier instead of leaving a gap after those fragments.
	HomeRevealStartTime = Settings.ExitDuration * Settings.HomeRevealDelayFraction
		* (bExitFromEntrance ? FMath::Clamp(Progress, 0.f, 1.f) : 1.f);
	Owner->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (Footer) Footer->SetVisibility(ESlateVisibility::HitTestInvisible);
	PrepareHome(InHome);
	Phase = EPhase::Exiting; Elapsed = 0.f;
}

void UKnightMaintenanceTransition::PrepareHome(UCanvasPanel* InHome)
{
	Home = InHome;
	UWidgetTree* Tree = Home ? Home->GetTypedOuter<UWidgetTree>() : nullptr;
	UCanvasPanel* Root = Tree ? Cast<UCanvasPanel>(Tree->RootWidget) : nullptr;
	UCanvasPanelSlot* PagesSlot = Home && Home->GetParent() ? Cast<UCanvasPanelSlot>(Home->GetParent()->Slot) : nullptr;
	if (!Root || !PagesSlot) return;
	HomeOverlay = Tree->ConstructWidget<UCanvasPanel>();
	HomeOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	HomeOverlay->SetRenderOpacity(0.f);
	UCanvasPanelSlot* OverlaySlot = Root->AddChildToCanvas(HomeOverlay);
	OverlaySlot->SetLayout(PagesSlot->GetLayout()); OverlaySlot->SetZOrder(PagesSlot->GetZOrder() + 1);
	for (UWidget* Child : Home->GetAllChildren())
	{
		UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Child->Slot);
		if (!Slot) continue;
		FKnightMovingPanelLayer Layer;
		Layer.Widget = Child; Layer.Parent = Home; Layer.Layout = Slot->GetLayout();
		Layer.Z = Slot->GetZOrder(); Layer.bAutoSize = Slot->GetAutoSize(); HomeChildren.Add(Layer);
		const TSharedRef<SWidget> KeepSlateAlive = Child->TakeWidget();
		Child->RemoveFromParent(); Slot = HomeOverlay->AddChildToCanvas(Child);
		Slot->SetLayout(Layer.Layout); Slot->SetZOrder(Layer.Z); Slot->SetAutoSize(Layer.bAutoSize);
	}
	HomeOverlay->ForceLayoutPrepass();
}

void UKnightMaintenanceTransition::AddLayer(UWidget* Widget, bool bRight)
{
	if (!Widget || !Widget->IsVisible()) return;
	for (UWidget* Ancestor = Widget->GetParent(); Ancestor; Ancestor = Ancestor->GetParent())
		if (!Ancestor->IsVisible()) return;
	UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot);
	UCanvasPanel* Parent = Cast<UCanvasPanel>(Widget->GetParent());
	if (!Slot || !Parent) return;
	const FGeometry& Root = Owner->WidgetTree->RootWidget->GetCachedGeometry();
	const FGeometry& G = Widget->GetCachedGeometry();
	if (G.GetLocalSize().IsNearlyZero()) return;
	FKnightMovingPanelLayer Layer;
	Layer.Widget = Widget; Layer.Parent = Parent; Layer.bRight = bRight;
	Layer.Layout = Slot->GetLayout(); Layer.Z = Slot->GetZOrder(); Layer.bAutoSize = Slot->GetAutoSize();
	Layer.Origin = Root.AbsoluteToLocal(G.LocalToAbsolute(FVector2D::ZeroVector));
	Layer.Size = Root.AbsoluteToLocal(G.LocalToAbsolute(G.GetLocalSize())) - Layer.Origin;
	const FGeometry& PG = Parent->GetCachedGeometry();
	Layer.ParentScale = (PG.AbsoluteToLocal(Root.LocalToAbsolute(FVector2D(1,0))) - PG.AbsoluteToLocal(Root.LocalToAbsolute(FVector2D::ZeroVector))).X;
	FBox2D& Bounds = bRight ? RightBounds : LeftBounds;
	Bounds += Layer.Origin; Bounds += Layer.Origin + Layer.Size;
	Layers.Add(Layer);
}

void UKnightMaintenanceTransition::AddCenter(UWidget* Widget)
{
	if (!Widget || !Widget->IsVisible()) return;
	for (UWidget* Ancestor = Widget->GetParent(); Ancestor; Ancestor = Ancestor->GetParent())
		if (!Ancestor->IsVisible()) return;
	FKnightFadingCenterLayer& Layer = Centers.AddDefaulted_GetRef();
	Layer.Widget = Widget; Layer.Opacity = Widget->GetRenderOpacity(); Widget->SetRenderOpacity(0.f);
}

bool UKnightMaintenanceTransition::PrepareLayers()
{
	UWidgetTree* Tree = Owner->WidgetTree;
	UCanvasPanel* Root = Tree ? Cast<UCanvasPanel>(Tree->RootWidget) : nullptr;
	if (!Root || !Owner->EntranceRevealMaterial || !Owner->EntranceScanMaterial) return false;
	RootSize = Root->GetCachedGeometry().GetLocalSize();
	if (RootSize.IsNearlyZero()) return false;
	LeftBounds = FBox2D(ForceInit); RightBounds = FBox2D(ForceInit);
	for (const TCHAR* Name : { TEXT("CraftingSurfaceLeft"), TEXT("Frame_Panel_Left"), TEXT("IndustrialSurfaceLeft"),
		TEXT("VerticalBox_Header"), TEXT("EnhancementOwnedColumn"), TEXT("SocketOwnedColumn") }) AddLayer(Tree->FindWidget(Name), false);
	for (const TCHAR* Name : { TEXT("CraftingSurfaceRight"), TEXT("Frame_Panel_Right"), TEXT("IndustrialSurfaceRight"),
		TEXT("EnhancementPreviewColumn"), TEXT("SocketEffectsColumn") }) AddLayer(Tree->FindWidget(Name), true);
	AddLayer(Owner->CurrencyText, true);
	AddLayer(Owner->EquipmentReplaceButton, false); AddLayer(Owner->EquipmentEnhanceButton, false);
	AddLayer(Footer, true);
	if (!LeftBounds.bIsValid || !RightBounds.bIsValid) { Layers.Reset(); return false; }
	// All moved roots are canvas children. Wrapping is temporary; anchors, auto-size,
	// Z order, padding and authored child transforms are restored exactly afterwards.
	for (FKnightMovingPanelLayer& Layer : Layers)
	{
		// Back belongs to the ancestor menu canvas, already arranged when this
		// child ticks. Keep it in that canvas; reparenting would omit it from the
		// ancestor's paint list for one frame on completion. Move/fade its text.
		if (Layer.Widget == Footer) continue;
		URetainerBox* Retainer = Tree->ConstructWidget<URetainerBox>(); Layer.Retainer = Retainer;
		Layer.Widget->RemoveFromParent(); Retainer->SetContent(Layer.Widget);
		UCanvasPanelSlot* Slot = Layer.Parent->AddChildToCanvas(Retainer);
		Slot->SetLayout(Layer.Layout); Slot->SetZOrder(Layer.Z); Slot->SetAutoSize(Layer.bAutoSize);
		Retainer->SetTextureParameter(TEXT("Texture")); Retainer->SetEffectMaterial(Owner->EntranceRevealMaterial);
		Retainer->SetRenderingPhase(0, 1); Retainer->SetVisibility(ESlateVisibility::HitTestInvisible);
		Retainer->SetRenderOpacity(0.f); Retainer->TakeWidget();
		// The wrappers are inserted during Tick, after the parent's prepass. Size
		// auto-sized title/currency/action roots before this frame can paint them.
		Retainer->ForceLayoutPrepass();
		Layer.Material = Retainer->GetEffectMaterial();
		if (Layer.Material)
		{
			Layer.Material->SetScalarParameterValue(TEXT("OriginX"), Layer.Origin.X);
			Layer.Material->SetScalarParameterValue(TEXT("Width"), Layer.Size.X);
			Layer.Material->SetScalarParameterValue(TEXT("Direction"), Layer.bRight ? -1.f : 1.f);
			Layer.Material->SetScalarParameterValue(TEXT("Softness"), Settings.RevealSoftness);
		}
	}
	for (const TCHAR* Name : { TEXT("MaintenanceBackdrop"), TEXT("EnhancementArtwork"), TEXT("EnhancementTargetColumn"),
		TEXT("EnhancementPreviewHost"), TEXT("EnhancementReadoutBackdropLayer"), TEXT("SocketArtwork"), TEXT("SocketAssignmentColumn") }) AddCenter(Tree->FindWidget(Name));
	for (bool bRight : { false, true })
	{
		TObjectPtr<UImage>& Scan = bRight ? RightScan : LeftScan;
		TObjectPtr<UMaterialInstanceDynamic>& Material = bRight ? RightScanMaterial : LeftScanMaterial;
		if (!Scan)
		{
			Scan = Tree->ConstructWidget<UImage>(UImage::StaticClass(), bRight ? TEXT("MaintenanceScanRight") : TEXT("MaintenanceScanLeft"));
			UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Scan); Slot->SetZOrder(90);
			Material = UMaterialInstanceDynamic::Create(Owner->EntranceScanMaterial, this); Scan->SetBrushFromMaterial(Material);
		}
		Scan->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	Owner->SetRenderOpacity(OriginalOpacity);
	return true;
}

void UKnightMaintenanceTransition::UpdateEntrance(float T)
{
	Progress = T;
	const float Ease = 1.f - FMath::Pow(1.f - T, FMath::Max(1.f, Settings.EaseOutExponent));
	const float Fade = Smooth(T / FMath::Max(.01f, Settings.FadeInFraction));
	const float Pad = Settings.GlowWidth * 4.f + 8.f;
	PanelOffset = FVector2D((-LeftBounds.Max.X - Pad) * (1.f - Ease), (RootSize.X - RightBounds.Min.X + Pad) * (1.f - Ease));
	const float Reveal = 1.f - FMath::Pow(1.f - T, 1.4f);
	const float LeftX = FMath::Lerp(LeftBounds.Min.X - Settings.RevealSoftness, LeftBounds.Max.X + Settings.RevealSoftness, Reveal);
	const float RightX = FMath::Lerp(RightBounds.Max.X + Settings.RevealSoftness, RightBounds.Min.X - Settings.RevealSoftness, Reveal);
	ScanPosition = FVector2D(LeftX, RightX);
	for (FKnightMovingPanelLayer& Layer : Layers)
	{
		if (Layer.Widget == Footer)
		{
			FWidgetTransform Transform = FooterTransform;
			Transform.Translation.X += PanelOffset.Y * Layer.ParentScale;
			Footer->SetRenderTransform(Transform);
			const float Coverage = Smooth(.5f + (Layer.Origin.X + Layer.Size.X * .5f - RightX)
				/ FMath::Max(Layer.Size.X, Settings.RevealSoftness));
			Footer->SetRenderOpacity(FooterOpacity * Fade * Coverage);
			Footer->SetVisibility(T > .4f && Coverage > .5f ? FooterVisibility : ESlateVisibility::HitTestInvisible);
			continue;
		}
		Layer.Retainer->SetRenderTranslation(FVector2D((Layer.bRight ? PanelOffset.Y : PanelOffset.X) * Layer.ParentScale, 0.f));
		Layer.Retainer->SetRenderOpacity(Fade);
		if (Layer.Material) Layer.Material->SetScalarParameterValue(TEXT("ScanX"), Layer.bRight ? RightX : LeftX);
		Layer.Retainer->RequestRender();
	}
	for (const FKnightFadingCenterLayer& Layer : Centers) if (Layer.Widget) Layer.Widget->SetRenderOpacity(Layer.Opacity * Fade);
	UpdateScan(Settings.GlowIntensity * Smooth(T / .12f) * (1.f - Smooth((T - .63f) / .37f)));
}

void UKnightMaintenanceTransition::UpdateScan(float Intensity, float BoundaryStrength)
{
	ScanIntensity = Intensity;
	const float Pad = Settings.GlowWidth * 4.f + 8.f;
	for (bool bRight : { false, true })
	{
		UImage* Scan = bRight ? RightScan : LeftScan;
		UMaterialInstanceDynamic* Material = bRight ? RightScanMaterial : LeftScanMaterial;
		if (!Scan || !Material) continue;
		const FBox2D& Bounds = bRight ? RightBounds : LeftBounds;
		const float Offset = bRight ? PanelOffset.Y : PanelOffset.X;
		const FVector2D Origin(Bounds.Min.X - Pad, Bounds.Min.Y - 24.f);
		const FVector2D Size = Bounds.GetSize() + FVector2D(Pad * 2.f, 48.f);
		UCanvasPanelSlot* Slot = CastChecked<UCanvasPanelSlot>(Scan->Slot);
		Slot->SetPosition(Origin + FVector2D(Offset, 0.f)); Slot->SetSize(Size);
		Material->SetScalarParameterValue(TEXT("Width"), Size.X);
		Material->SetScalarParameterValue(TEXT("Height"), Size.Y);
		Material->SetScalarParameterValue(TEXT("ScanX"), (bRight ? ScanPosition.Y : ScanPosition.X) - Origin.X);
		Material->SetScalarParameterValue(TEXT("Direction"), bRight ? -1.f : 1.f);
		Material->SetScalarParameterValue(TEXT("CoreWidth"), Settings.EdgeWidth);
		Material->SetScalarParameterValue(TEXT("GlowWidth"), Settings.GlowWidth);
		Material->SetScalarParameterValue(TEXT("Intensity"), Intensity);
		Material->SetScalarParameterValue(TEXT("TrailStrength"), Settings.TrailStrength);
		Material->SetScalarParameterValue(TEXT("OriginX"), Origin.X + Offset);
		Material->SetScalarParameterValue(TEXT("ViewWidth"), RootSize.X);
		Material->SetScalarParameterValue(TEXT("BoundaryStrength"), BoundaryStrength);
	}
}

void UKnightMaintenanceTransition::UpdateExit(float T)
{
	Progress = T;
	const float Ease = T < .5f ? 4.f * T * T * T : 1.f - FMath::Pow(-2.f * T + 2.f, 3.f) * .5f;
	const float Fade = 1.f - Smooth(T / FMath::Max(.01f, Settings.ExitFadeEndFraction));
	const float Boundary = Smooth(T / .18f);
	if (!Layers.IsEmpty())
	{
		const float Pad = Settings.GlowWidth * 4.f + 8.f;
		PanelOffset = FMath::Lerp(ExitStartOffset,
			FVector2D(-LeftBounds.Max.X - Pad, RootSize.X - RightBounds.Min.X + Pad), Ease);
		for (FKnightMovingPanelLayer& Layer : Layers)
		{
			const float Offset = Layer.bRight ? PanelOffset.Y : PanelOffset.X;
			if (Layer.Widget == Footer)
			{
				FWidgetTransform Transform = FooterTransform; Transform.Translation.X += Offset * Layer.ParentScale;
				Footer->SetRenderTransform(Transform);
				const float Edge = Smooth((RootSize.X - Layer.Origin.X - Layer.Size.X - Offset) / 48.f);
				Footer->SetRenderOpacity(ExitFooterOpacity * Fade * FMath::Lerp(1.f, Edge, Boundary));
				continue;
			}
			Layer.Retainer->SetRenderTranslation(FVector2D(Offset * Layer.ParentScale, 0.f));
			if (Layer.Material)
			{
				Layer.Material->SetScalarParameterValue(TEXT("ScreenOffset"), Offset);
				Layer.Material->SetScalarParameterValue(TEXT("ViewWidth"), RootSize.X);
				Layer.Material->SetScalarParameterValue(TEXT("BoundaryStrength"), Boundary);
			}
			Layer.Retainer->RequestRender();
		}
		const float Light = bExitFromEntrance ? ExitScanIntensity * (1.f - Smooth(T / .45f))
			: Settings.GlowIntensity * Settings.ExitGlowMultiplier * Smooth(T / .18f) * (1.f - Smooth(T / .70f));
		UpdateScan(Light * Fade, Boundary);
	}
	else if (Footer) Footer->SetRenderOpacity(ExitFooterOpacity * Fade);
	Owner->SetRenderOpacity(ExitStartOpacity * Fade);
	if (HomeOverlay)
	{
		const float HomeT = FMath::Clamp((Elapsed - HomeRevealStartTime)
			/ FMath::Max(.01f, Settings.HomeRevealDuration), 0.f, 1.f);
		HomeOverlay->SetRenderOpacity(1.f - FMath::Pow(1.f - HomeT, 3.f));
	}
}

void UKnightMaintenanceTransition::Tick(float DeltaTime)
{
	if (!Owner || !IsActive()) return;
	if (Phase == EPhase::Preparing)
	{
		if (PrepareFrames-- > 0) return;
		if (!PrepareLayers())
		{
			FSimpleDelegate Finished = MoveTemp(OnFinished); Restore(); Finished.ExecuteIfBound(); return;
		}
		Phase = EPhase::Entering; UpdateEntrance(0.f); return;
	}
	Elapsed += FMath::Max(0.f, DeltaTime);
	if (Phase == EPhase::Entering)
	{
		const float T = FMath::Clamp(Elapsed / FMath::Max(.01f, Settings.Duration), 0.f, 1.f);
		UpdateEntrance(T);
		if (T >= 1.f)
		{
			FSimpleDelegate Finished = MoveTemp(OnFinished); Restore(); Finished.ExecuteIfBound();
		}
	}
	else
	{
		const float T = FMath::Clamp(Elapsed / FMath::Max(.01f, Settings.ExitDuration), 0.f, 1.f);
		UpdateExit(T);
		const float End = FMath::Max(Settings.ExitDuration, HomeRevealStartTime + Settings.HomeRevealDuration);
		if (Elapsed >= End)
		{
			FSimpleDelegate Finished = MoveTemp(OnFinished);
			Restore(true); Finished.ExecuteIfBound();
			// Navigation has hidden the maintenance page. Restore its authored
			// visibility now so the next entrance starts from clean defaults.
			if (Owner) Owner->SetVisibility(OriginalVisibility);
		}
	}
}

void UKnightMaintenanceTransition::Restore(bool bKeepOutgoingHidden)
{
	if (bKeepOutgoingHidden)
	{
		if (Owner) Owner->SetVisibility(ESlateVisibility::Collapsed);
		if (Footer) Footer->SetVisibility(ESlateVisibility::Collapsed);
	}
	for (FKnightMovingPanelLayer& Layer : Layers)
		if (Layer.Widget && Layer.Parent && Layer.Retainer)
		{
			Layer.Retainer->SetVisibility(ESlateVisibility::Collapsed);
			Layer.Retainer->SetContent(nullptr); Layer.Retainer->RemoveFromParent();
			// Release temporary GPU targets now, rather than accumulating them until GC
			// when the player repeatedly opens and closes maintenance.
			Layer.Retainer->ReleaseSlateResources(true);
			UCanvasPanelSlot* Slot = Layer.Parent->AddChildToCanvas(Layer.Widget);
			Slot->SetLayout(Layer.Layout); Slot->SetZOrder(Layer.Z); Slot->SetAutoSize(Layer.bAutoSize);
		}
	Layers.Reset();
	for (const FKnightFadingCenterLayer& Layer : Centers) if (Layer.Widget) Layer.Widget->SetRenderOpacity(Layer.Opacity);
	Centers.Reset();
	if (LeftScan) LeftScan->SetVisibility(ESlateVisibility::Collapsed);
	if (RightScan) RightScan->SetVisibility(ESlateVisibility::Collapsed);
	if (Owner) { Owner->SetRenderOpacity(OriginalOpacity); if (!bKeepOutgoingHidden) Owner->SetVisibility(OriginalVisibility); }
	if (Footer) { Footer->SetRenderOpacity(FooterOpacity); if (!bKeepOutgoingHidden) Footer->SetVisibility(FooterVisibility); Footer->SetRenderTransform(FooterTransform); }
	for (const FKnightMovingPanelLayer& Layer : HomeChildren)
	{
		const TSharedRef<SWidget> KeepSlateAlive = Layer.Widget->TakeWidget();
		Layer.Widget->RemoveFromParent();
		UCanvasPanelSlot* Slot = Layer.Parent->AddChildToCanvas(Layer.Widget);
		Slot->SetLayout(Layer.Layout); Slot->SetZOrder(Layer.Z); Slot->SetAutoSize(Layer.bAutoSize);
	}
	HomeChildren.Reset();
	if (HomeOverlay) { HomeOverlay->SetVisibility(ESlateVisibility::Collapsed); HomeOverlay->RemoveFromParent(); HomeOverlay->ReleaseSlateResources(true); HomeOverlay = nullptr; }
	if (Home) Home->ForceLayoutPrepass();
	// Completion runs in the widget tick (after this frame's normal prepass).
	// Reparenting reconstructs Slate children; measure them immediately so the
	// completion frame cannot briefly paint zero-sized art and overlapping text.
	if (Owner && Owner->WidgetTree && Owner->WidgetTree->RootWidget) Owner->WidgetTree->RootWidget->ForceLayoutPrepass();
	if (Footer && Footer->GetParent()) Footer->GetParent()->ForceLayoutPrepass();
	Phase = EPhase::Idle; PanelOffset = FVector2D::ZeroVector;
}

void UKnightMaintenanceTransition::Cancel()
{
	OnFinished.Unbind();
	if (IsActive() || !Layers.IsEmpty()) Restore();
}
