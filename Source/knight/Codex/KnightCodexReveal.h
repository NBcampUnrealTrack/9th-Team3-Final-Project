#pragma once

#include "CoreMinimal.h"
#include "Components/ContentWidget.h"
#include "KnightCodexReveal.generated.h"

/** Transient feathered Slate mask. Direct painting at rest preserves crisp text and original artwork. */
UCLASS()
class KNIGHT_API UKnightCodexReveal : public UContentWidget
{
	GENERATED_BODY()
public:
	void SetReveal(float Progress, float Alpha, float Glow, float Feather, bool bActive, UWidget* Target = nullptr);
	float Progress = 1.f, Alpha = 1.f, Glow = 0.f, Feather = 28.f;
	bool bActive = false;
	TWeakObjectPtr<UWidget> Target;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void OnSlotAdded(UPanelSlot* InSlot) override;
	virtual void OnSlotRemoved(UPanelSlot* InSlot) override;
private:
	TSharedPtr<class SKnightCodexReveal> RevealSlate;
};

/** Only present during motion; consumes pointer input without taking keyboard focus from the menu. */
UCLASS()
class KNIGHT_API UKnightCodexMotionShield : public UWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
