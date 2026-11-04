#include "Menu/UI/KnightButtonFillExtension.h"

#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "Components/ContentWidget.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

void UKnightButtonFillExtension::SetEnabled(UCommonButtonBase* Button, bool bInEnabled, FLinearColor Color)
{
	if (!Button) return;
	UKnightButtonFillExtension* Effect = bInEnabled
		? &Button->GetOrAddExtension<UKnightButtonFillExtension>()
		: Button->GetExtension<UKnightButtonFillExtension>();
	if (!Effect) return;
	Effect->bEnabled = bInEnabled;
	Effect->FillColor = Color;
	if (bInEnabled) Effect->EnsureLayers();
	else Effect->Clear();
}

void UKnightButtonFillExtension::Reset(UCommonButtonBase* Button)
{
	if (Button)
		if (UKnightButtonFillExtension* Effect = Button->GetExtension<UKnightButtonFillExtension>()) Effect->Clear();
}

void UKnightButtonFillExtension::SetLabelFitted(UCommonButtonBase* Button, FLinearColor Color)
{
	SetEnabled(Button, true, Color);
	if (UKnightButtonFillExtension* Effect = Button ? Button->GetExtension<UKnightButtonFillExtension>() : nullptr)
	{
		Effect->bFitToLabel = true;
		Effect->BandLighten = 0.08f;
	}
}

void UKnightButtonFillExtension::Construct()
{
	Super::Construct();
	if (bEnabled) EnsureLayers();
	Clear();
}

void UKnightButtonFillExtension::Destruct()
{
	Clear();
	Super::Destruct();
}

void UKnightButtonFillExtension::EnsureLayers()
{
	if (FillImage && BandImage) return;
	UCommonButtonBase* const Button = Cast<UCommonButtonBase>(GetUserWidget());
	UWidgetTree* const Tree = Button ? Button->WidgetTree : nullptr;
	UContentWidget* const Root = Tree ? Cast<UContentWidget>(Tree->RootWidget) : nullptr;
	UWidget* const OldContent = Root ? Root->GetContent() : nullptr;
	if (!OldContent) return;

	UOverlay* const Layers = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("KnightFillLayers"));
	UOverlay* const Clip = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("KnightFillClip"));
	FillClip = Clip;
	Clip->SetRenderTransformPivot(FVector2D::ZeroVector);
	Clip->SetClipping(EWidgetClipping::ClipToBounds);
	Clip->SetVisibility(ESlateVisibility::HitTestInvisible);
	IdleImage = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("KnightFillIdle"));
	FillImage = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("KnightFillSurface"));
	BandImage = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("KnightFillBand"));
	FSlateBrush SurfaceBrush = *FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	if (const UCommonButtonStyle* const Style = Button->GetStyleCDO())
	{
		FSlateBrush BaseBrush;
		Style->GetNormalBaseBrush(BaseBrush);
		if (BaseBrush.DrawAs == ESlateBrushDrawType::RoundedBox)
		{
			SurfaceBrush = BaseBrush;
			SurfaceBrush.TintColor = FSlateColor(FLinearColor::White);
			SurfaceBrush.OutlineSettings.Width = 0.f;
		}
	}
	for (UImage* Image : { IdleImage.Get(), FillImage.Get(), BandImage.Get() })
	{
		Image->SetBrush(Image != BandImage ? SurfaceBrush : *FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")));
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		Image->SetRenderTransformPivot(FVector2D::ZeroVector);
		UOverlaySlot* const Slot = Clip->AddChildToOverlay(Image);
		Slot->SetHorizontalAlignment(HAlign_Fill);
		Slot->SetVerticalAlignment(VAlign_Fill);
	}
	UOverlaySlot* ClipSlot = nullptr;
	if (UWidget* const Background = Tree->FindWidget(TEXT("Img_Background")))
		if (UOverlay* const BackgroundParent = Cast<UOverlay>(Background->GetParent()))
			ClipSlot = Cast<UOverlaySlot>(BackgroundParent->InsertChildAt(BackgroundParent->GetChildIndex(Background) + 1, Clip));
	if (!ClipSlot)
	{
		Root->SetContent(Layers);
		ClipSlot = Layers->AddChildToOverlay(Clip);
		UOverlaySlot* const ContentSlot = Layers->AddChildToOverlay(OldContent);
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Fill);
	}
	ClipSlot->SetHorizontalAlignment(HAlign_Fill);
	ClipSlot->SetVerticalAlignment(VAlign_Fill);
	ApplyVisual(FVector2D::ZeroVector);
}

void UKnightButtonFillExtension::Clear()
{
	Progress = 0.f;
	TransitionStart = TransitionTarget = TransitionElapsed = 0.f;
	LastSize = FVector2D::ZeroVector;
	ApplyVisual(FVector2D::ZeroVector);
}

void UKnightButtonFillExtension::Tick(const FGeometry& MyGeometry, float InDeltaTime)
{
	UCommonButtonBase* const Button = Cast<UCommonButtonBase>(GetUserWidget());
	if (!Button || !bEnabled || !FillImage || !BandImage) return;
	bool bVisibleInTree = Button->IsVisible();
	for (const UWidget* Parent = Button->GetParent(); bVisibleInTree && Parent; Parent = Parent->GetParent())
		bVisibleInTree = Parent->IsVisible();
	const bool bInteractive = bVisibleInTree && Button->GetIsEnabled() && !Button->GetLocked();
	if (!bInteractive)
	{
		if (Progress != 0.f) Clear();
		return;
	}
	const bool bHot = Button->IsHovered() || Button->HasAnyUserFocus() || Button->HasFocusedDescendants();
	const float Target = bHot ? 1.f : 0.f;
	const float Previous = Progress;
	if (Target != TransitionTarget)
	{
		TransitionStart = Progress;
		TransitionTarget = Target;
		TransitionElapsed = 0.f;
	}
	const float Duration = bHot ? 0.2f : 0.12f;
	TransitionElapsed = FMath::Min(TransitionElapsed + FMath::Max(InDeltaTime, 0.f), Duration);
	const float T = TransitionElapsed / Duration;
	const float EaseOut = 1.f - FMath::Pow(1.f - T, 3.f);
	Progress = FMath::Lerp(TransitionStart, TransitionTarget, EaseOut);
	const FVector2D Size = MyGeometry.GetLocalSize();
	if (bFitToLabel || Progress != Previous || !Size.Equals(LastSize, 0.5f)) ApplyVisual(Size);
}

void UKnightButtonFillExtension::ApplyVisual(const FVector2D& Size)
{
	if (!FillImage || !BandImage) return;
	LastSize = Size;
	float BackgroundScale = 1.f;
	FVector2D SurfaceSize = Size;
	if (bFitToLabel && FillClip)
	{
		const UUserWidget* Button = GetUserWidget();
		const UTextBlock* Label = Button ? Cast<UTextBlock>(Button->GetWidgetFromName(TEXT("Txt_Label"))) : nullptr;
		const FGeometry& ClipGeometry = FillClip->GetCachedGeometry();
		if (Label && FillClip->GetParent() && ClipGeometry.GetLocalSize().X > 0.f)
		{
			SurfaceSize = ClipGeometry.GetLocalSize();
			const FGeometry& LabelGeometry = Label->GetCachedGeometry();
			// Measure in the unchanged parent layout, avoiding feedback from our render scale.
			const FGeometry& ParentGeometry = FillClip->GetParent()->GetCachedGeometry();
			const float Start = ParentGeometry.AbsoluteToLocal(ClipGeometry.LocalToAbsolute(FVector2D::ZeroVector)).X;
			const float TextStart = ParentGeometry.AbsoluteToLocal(LabelGeometry.LocalToAbsolute(FVector2D::ZeroVector)).X;
			const float TextEnd = ParentGeometry.AbsoluteToLocal(LabelGeometry.LocalToAbsolute(FVector2D(Label->GetDesiredSize().X, 0.f))).X;
			const float LeftPadding = FMath::Max(0.f, TextStart - Start);
			const float Width = TextEnd - Start + LeftPadding;
			BackgroundScale = Width > 0.f ? Width / SurfaceSize.X : 1.f;
		}
		FillClip->SetRenderScale(FVector2D(BackgroundScale, 1.f));
	}
	FillImage->SetColorAndOpacity(FillColor);
	if (IdleImage)
	{
		FLinearColor IdleColor = FillColor;
		IdleColor.A = bEnabled ? 0.18f : 0.f;
		IdleImage->SetColorAndOpacity(IdleColor);
	}
	FillImage->SetRenderScale(FVector2D(Progress, 1.f));
	const float Width = FMath::Max(0.f, SurfaceSize.X);
	const float BandWidth = FMath::Min(Width, FMath::Max(8.f, SurfaceSize.Y * 0.9f) / BackgroundScale);
	FLinearColor BandColor = FLinearColor::LerpUsingHSV(FillColor, FLinearColor::White, BandLighten);
	BandColor.A = 1.f;
	BandImage->SetColorAndOpacity(BandColor);
	FWidgetTransform BandTransform;
	BandTransform.Scale = FVector2D(Width > 0.f ? BandWidth / Width : 0.f, 1.f);
	BandTransform.Translation = FVector2D(Progress * Width - BandWidth, 0.f);
	BandImage->SetRenderTransform(BandTransform);
	BandImage->SetRenderOpacity(Progress > 0.f && Progress < 1.f ? FMath::Min(1.f, (1.f - Progress) * 5.f) : 0.f);
}
