#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "knightActivatableWidget.generated.h"

/** Input routing while the widget is the active one in its layer. */
UENUM(BlueprintType)
enum class EknightWidgetInputMode : uint8
{
	/** Leave input mode alone. */
	Default,
	/** Game and menu both receive input (HUD with a clickable element, radial menus). */
	GameAndMenu,
	/** Only the game receives input (the HUD). */
	Game,
	/** Only the menu receives input; mouse is shown (pause, settings). */
	Menu
};

/** Base for every CommonUI screen and HUD layout in the project. */
UCLASS(Abstract, Blueprintable)
class KNIGHTUI_API UknightActivatableWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	UPROPERTY(EditDefaultsOnly, Category="Knight|Input")
	EknightWidgetInputMode InputMode = EknightWidgetInputMode::Default;

	/** Mouse capture while in Game or GameAndMenu mode. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Input")
	EMouseCaptureMode GameMouseCaptureMode = EMouseCaptureMode::CapturePermanently;
};
