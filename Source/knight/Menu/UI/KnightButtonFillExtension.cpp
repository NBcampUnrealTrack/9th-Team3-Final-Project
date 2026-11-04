#include "Menu/UI/KnightButtonFillExtension.h"

#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "Components/ContentWidget.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
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
	Clip->SetClipping(EWidgetClipping::ClipToBounds);
	Clip->SetVisibility(ESlateVisibility::HitTestInvisible);
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
	for (UImage* Image : { FillImage.Get(), BandImage.Get() })
	{
		Image->SetBrush(Image == FillImage ? SurfaceBrush : *FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")));
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
	const float Speed = bHot ? 1.f / 0.2f : 1.f / 0.12f;
	const float Previous = Progress;
	Progress = FMath::FInterpConstantTo(Progress, Target, FMath::Max(InDeltaTime, 0.f), Speed);
	const FVector2D Size = MyGeometry.GetLocalSize();
	if (Progress != Previous || !Size.Equals(LastSize, 0.5f)) ApplyVisual(Size);
}

void UKnightButtonFillExtension::ApplyVisual(const FVector2D& Size)
{
	if (!FillImage || !BandImage) return;
	LastSize = Size;
	FillImage->SetColorAndOpacity(FillColor);
	FillImage->SetRenderScale(FVector2D(Progress, 1.f));
	const float Width = FMath::Max(0.f, Size.X);
	const float BandWidth = FMath::Min(Width, FMath::Max(8.f, Size.Y * 0.9f));
	FLinearColor BandColor = FLinearColor::LerpUsingHSV(FillColor, FLinearColor::White, 0.22f);
	BandColor.A = 1.f;
	BandImage->SetColorAndOpacity(BandColor);
	FWidgetTransform BandTransform;
	BandTransform.Scale = FVector2D(Width > 0.f ? BandWidth / Width : 0.f, 1.f);
	BandTransform.Translation = FVector2D(Progress * Width - BandWidth, 0.f);
	BandImage->SetRenderTransform(BandTransform);
	BandImage->SetRenderOpacity(Progress > 0.f && Progress < 1.f ? FMath::Min(1.f, (1.f - Progress) * 5.f) : 0.f);
}
