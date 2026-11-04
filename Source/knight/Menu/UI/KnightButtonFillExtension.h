#pragma once

#include "CoreMinimal.h"
#include "Extensions/UserWidgetExtension.h"
#include "KnightButtonFillExtension.generated.h"

class UCommonButtonBase;
class UImage;
class UOverlay;

/** Opt-in background fill for menu buttons. The button's existing content and Slate hit area stay intact. */
UCLASS()
class KNIGHT_API UKnightButtonFillExtension : public UUserWidgetExtension
{
	GENERATED_BODY()

public:
	static void SetEnabled(UCommonButtonBase* Button, bool bEnabled,
		FLinearColor Color = FLinearColor(FColor(21, 106, 203)));
	static void Reset(UCommonButtonBase* Button);
	/** Fit only the background to the label, preserving the original content and hit area. */
	static void SetLabelFitted(UCommonButtonBase* Button, FLinearColor Color);
	float GetProgress() const { return Progress; }
	bool IsEnabled() const { return bEnabled; }

	virtual void Construct() override;
	virtual void Destruct() override;
	virtual bool RequiresTick() const override { return true; }
	virtual void Tick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void EnsureLayers();
	void ApplyVisual(const FVector2D& Size);
	void Clear();

	UPROPERTY(Transient)
	TObjectPtr<UImage> FillImage;
	UPROPERTY(Transient)
	TObjectPtr<UImage> BandImage;
	UPROPERTY(Transient)
	TObjectPtr<UOverlay> FillClip;
	FLinearColor FillColor = FLinearColor(FColor(21, 106, 203));
	float Progress = 0.f;
	float TransitionStart = 0.f;
	float TransitionTarget = 0.f;
	float TransitionElapsed = 0.f;
	FVector2D LastSize = FVector2D::ZeroVector;
	bool bEnabled = false;
	bool bFitToLabel = false;
	float BandLighten = 0.22f;
};
