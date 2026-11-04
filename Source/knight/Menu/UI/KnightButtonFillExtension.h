#pragma once

#include "CoreMinimal.h"
#include "Extensions/UserWidgetExtension.h"
#include "KnightButtonFillExtension.generated.h"

class UCommonButtonBase;
class UImage;

/** Opt-in background fill for menu buttons. The button's existing content and Slate hit area stay intact. */
UCLASS()
class KNIGHT_API UKnightButtonFillExtension : public UUserWidgetExtension
{
	GENERATED_BODY()

public:
	static void SetEnabled(UCommonButtonBase* Button, bool bEnabled,
		FLinearColor Color = FLinearColor(FColor(21, 106, 203)));
	static void Reset(UCommonButtonBase* Button);
	float GetProgress() const { return Progress; }

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
	FLinearColor FillColor = FLinearColor(FColor(21, 106, 203));
	float Progress = 0.f;
	FVector2D LastSize = FVector2D::ZeroVector;
	bool bEnabled = false;
};
