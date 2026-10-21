#include "Menu/UI/KnightMenuVideoBackgroundWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaTexture.h"
#include "Engine/Texture2D.h"
#include "knight.h"

void UKnightMenuVideoBackgroundWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(false);
	SetVisibility(ESlateVisibility::Collapsed);
	if (!WidgetTree || WidgetTree->RootWidget) return;

	UOverlay* const Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("VideoBackgroundRoot"));
	Root->SetClipping(EWidgetClipping::ClipToBounds);
	WidgetTree->RootWidget = Root;
	// Match WBP_KnightTitleMenu's opaque intro backdrop so the unused left edge never reveals the 3D world.
	BackgroundBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TitleDarkBackground"));
	BackgroundBorder->SetBrush(FSlateColorBrush(FLinearColor::White));
	BackgroundBorder->SetBrushColor(FLinearColor(0.001518f, 0.002428f, 0.004025f, 1.f));
	UOverlaySlot* const BackgroundSlot = Root->AddChildToOverlay(BackgroundBorder);
	BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
	BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
	BackgroundSlot->SetPadding(FMargin(0.f));

	FallbackScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("FallbackCover"));
	FallbackScaleBox->SetStretch(EStretch::ScaleToFill);
	FallbackImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("FallbackImage"));
	FallbackScaleBox->SetContent(FallbackImage);
	Root->AddChildToOverlay(FallbackScaleBox);

	VideoScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("VideoCover"));
	VideoScaleBox->SetStretch(EStretch::None); // Keep the media's current 1:1 size and aspect ratio.
	VideoImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("VideoImage"));
	VideoImage->SetDesiredSizeOverride(FVector2D(1920.f, 1080.f));
	VideoScaleBox->SetContent(VideoImage);
	UOverlaySlot* const VideoSlot = Root->AddChildToOverlay(VideoScaleBox);
	VideoSlot->SetHorizontalAlignment(HAlign_Right);
	VideoSlot->SetVerticalAlignment(VAlign_Top);
	VideoSlot->SetPadding(FMargin(0.f));

	DimBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("VideoDim"));
	Root->AddChildToOverlay(DimBorder);
	for (UWidget* Child : {static_cast<UWidget*>(BackgroundBorder), static_cast<UWidget*>(FallbackScaleBox), static_cast<UWidget*>(VideoScaleBox), static_cast<UWidget*>(DimBorder)})
	{
		Child->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	VideoScaleBox->SetVisibility(ESlateVisibility::Collapsed);
	FallbackScaleBox->SetVisibility(ESlateVisibility::Collapsed);
}

void UKnightMenuVideoBackgroundWidget::Configure(bool bInEnabled, UMediaSource* InSource, bool bInLoop,
	UTexture2D* InFallback, float InDimOpacity, UMaterialInterface* InVideoMaterial)
{
	Stop();
	bEnabled = bInEnabled;
	Source = InSource;
	bLoop = bInLoop;
	Fallback = InFallback;
	DimOpacity = FMath::Clamp(InDimOpacity, 0.f, 1.f);
	VideoMaterial = InVideoMaterial;
	if (FallbackImage && Fallback)
	{
		FallbackImage->SetBrushFromTexture(Fallback, true);
	}
	if (DimBorder) DimBorder->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, DimOpacity));
}

void UKnightMenuVideoBackgroundWidget::EnsurePlayer()
{
	if (Player || !VideoMaterial) return;
	Player = NewObject<UMediaPlayer>(this);
	Player->PlayOnOpen = false;
	Player->NativeAudioOut = false;
	MediaTexture = NewObject<UMediaTexture>(this);
	MediaTexture->NewStyleOutput = true; // UI material samples this as a regular 2D texture.
	MediaTexture->SetMediaPlayer(Player);
	MediaTexture->UpdateResource();
	VideoMaterialInstance = UMaterialInstanceDynamic::Create(VideoMaterial, this);
	VideoMaterialInstance->SetTextureParameterValue(TEXT("VideoTexture"), MediaTexture);
	if (VideoImage) VideoImage->SetBrushFromMaterial(VideoMaterialInstance);
}

void UKnightMenuVideoBackgroundWidget::Start()
{
	if (bActive || !bEnabled) return;
	bActive = true;
	ShowFallback();
	if (!Source) return;
	EnsurePlayer();
	if (!Player || !MediaTexture || !VideoMaterialInstance)
	{
		UE_LOG(Logknight, Warning, TEXT("Menu video material is missing; using the background fallback."));
		return;
	}
	MediaTexture->SetMediaPlayer(Player);
	Player->OnMediaOpened.AddUniqueDynamic(this, &ThisClass::HandleMediaOpened);
	Player->OnMediaOpenFailed.AddUniqueDynamic(this, &ThisClass::HandleMediaOpenFailed);
	Player->OnTracksChanged.AddUniqueDynamic(this, &ThisClass::HandleTracksChanged);
	if (!Player->OpenSource(Source))
	{
		UE_LOG(Logknight, Warning, TEXT("Menu video source could not be opened: %s"), *Source->GetUrl());
		ShowFallback();
	}
}

void UKnightMenuVideoBackgroundWidget::Stop()
{
	bActive = false;
	SetVisibility(ESlateVisibility::Collapsed);
	if (Player)
	{
		Player->OnMediaOpened.RemoveDynamic(this, &ThisClass::HandleMediaOpened);
		Player->OnMediaOpenFailed.RemoveDynamic(this, &ThisClass::HandleMediaOpenFailed);
		Player->OnTracksChanged.RemoveDynamic(this, &ThisClass::HandleTracksChanged);
		Player->Close(); // Stops playback and releases the decoder before another menu page or map.
	}
	if (MediaTexture) MediaTexture->SetMediaPlayer(nullptr);
	if (VideoScaleBox) VideoScaleBox->SetVisibility(ESlateVisibility::Collapsed);
}

void UKnightMenuVideoBackgroundWidget::HandleMediaOpened(FString OpenedUrl)
{
	if (!bActive || !Source || !Player || OpenedUrl != Source->GetUrl()) return;
	Player->SetLooping(bLoop);
	Player->SelectTrack(EMediaPlayerTrack::Audio, INDEX_NONE);
	if (Player->GetNumTracks(EMediaPlayerTrack::Video) <= 0)
	{
		UE_LOG(Logknight, Warning, TEXT("Menu media source has no video track: %s"), *OpenedUrl);
		Player->Close();
		ShowFallback();
		return;
	}
	RefreshVideoDimensions();
	if (Player->Play()) ShowVideo();
	else
	{
		Player->Close();
		ShowFallback();
	}
}

void UKnightMenuVideoBackgroundWidget::HandleMediaOpenFailed(FString FailedUrl)
{
	if (!bActive) return;
	UE_LOG(Logknight, Warning, TEXT("Menu video open failed: %s"), *FailedUrl);
	if (Player) Player->Close();
	ShowFallback();
}

void UKnightMenuVideoBackgroundWidget::HandleTracksChanged()
{
	if (bActive) RefreshVideoDimensions();
}

void UKnightMenuVideoBackgroundWidget::RefreshVideoDimensions()
{
	if (!Player || !VideoImage) return;
	const int32 Track = Player->GetSelectedTrack(EMediaPlayerTrack::Video);
	if (Track < 0) return;
	const FIntPoint Size = Player->GetVideoTrackDimensions(Track, Player->GetTrackFormat(EMediaPlayerTrack::Video, Track));
	if (Size.X > 0 && Size.Y > 0) VideoImage->SetDesiredSizeOverride(FVector2D(Size));
}

void UKnightMenuVideoBackgroundWidget::ShowFallback()
{
	if (VideoScaleBox) VideoScaleBox->SetVisibility(ESlateVisibility::Collapsed);
	if (FallbackScaleBox) FallbackScaleBox->SetVisibility(Fallback ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UKnightMenuVideoBackgroundWidget::ShowVideo()
{
	if (FallbackScaleBox) FallbackScaleBox->SetVisibility(ESlateVisibility::Collapsed);
	if (VideoScaleBox) VideoScaleBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UKnightMenuVideoBackgroundWidget::NativeDestruct()
{
	Stop();
	Super::NativeDestruct();
}
