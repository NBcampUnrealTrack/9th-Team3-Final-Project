#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "KnightMenuVideoBackgroundWidget.generated.h"

class UBorder;
class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMediaPlayer;
class UMediaSource;
class UMediaTexture;
class UScaleBox;
class UTexture2D;

/** Passive, full-viewport menu background. The owning menu decides when decoding may run. */
UCLASS()
class KNIGHT_API UKnightMenuVideoBackgroundWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Configure(bool bInEnabled, UMediaSource* InSource, bool bInLoop,
		UTexture2D* InFallback, float InDimOpacity, UMaterialInterface* InVideoMaterial);
	void Start();
	void Stop();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleMediaOpened(FString OpenedUrl);
	UFUNCTION()
	void HandleMediaOpenFailed(FString FailedUrl);
	UFUNCTION()
	void HandleTracksChanged();

	void ShowFallback();
	void ShowVideo();
	void EnsurePlayer();
	void RefreshVideoDimensions();

	UPROPERTY(Transient)
	TObjectPtr<UImage> FallbackImage;
	UPROPERTY(Transient)
	TObjectPtr<UImage> VideoImage;
	UPROPERTY(Transient)
	TObjectPtr<UScaleBox> FallbackScaleBox;
	UPROPERTY(Transient)
	TObjectPtr<UScaleBox> VideoScaleBox;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> DimBorder;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> BackgroundBorder;
	UPROPERTY(Transient)
	TObjectPtr<UMediaPlayer> Player;
	UPROPERTY(Transient)
	TObjectPtr<UMediaTexture> MediaTexture;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> VideoMaterialInstance;
	UPROPERTY(Transient)
	TObjectPtr<UMediaSource> Source;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Fallback;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> VideoMaterial;

	bool bEnabled = false;
	bool bLoop = true;
	bool bActive = false;
	float DimOpacity = 0.0f;
};
