#pragma once

#include "CoreMinimal.h"
#include "Extensions/UserWidgetExtension.h"
#include "KnightMaintenanceStateExtension.generated.h"

class UCommonButtonBase;
class UImage;
class UTextBlock;

/** Static maintenance feedback. Never changes selection, equipment or hit geometry. */
UCLASS()
class KNIGHT_API UKnightMaintenanceStateExtension : public UUserWidgetExtension
{
	GENERATED_BODY()
public:
	static void Enable(UCommonButtonBase* Button);
	static void SetEquipped(UCommonButtonBase* Button, bool bEquipped);
	static void NavigationFocus(UCommonButtonBase* Button);
	/** Text-only focus for compact navigation components; list indicators remain unchanged. */
	static void SetFocusOutlineEnabled(UCommonButtonBase* Button, bool bEnabled);
	bool IsEquipped() const { return bEquipped; }
	virtual void Construct() override;
	virtual bool RequiresTick() const override { return true; }
	virtual void Tick(const FGeometry& Geometry, float DeltaTime) override;

private:
	void EnsureLayers();
	UPROPERTY(Transient) TObjectPtr<UImage> SelectionEdge;
	UPROPERTY(Transient) TObjectPtr<UImage> FocusOutline;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EquippedLabel;
	bool bEquipped = false;
	bool bDrawFocusOutline = true;
};
