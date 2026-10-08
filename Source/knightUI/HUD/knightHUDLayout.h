#pragma once

#include "CoreMinimal.h"
#include "Foundation/knightActivatableWidget.h"
#include "knightHUDLayout.generated.h"

/**
 * In-game HUD pushed onto UI.Layer.Game by AknightHUD (its HUDLayoutClass). Game-only input; it never takes the mouse.
 * The Blueprint subclass (WBP_HUDLayout) places the HUD pieces; each piece binds its own view model.
 */
UCLASS(Abstract, Blueprintable)
class KNIGHTUI_API UknightHUDLayout : public UknightActivatableWidget
{
	GENERATED_BODY()

public:
	UknightHUDLayout(const FObjectInitializer& ObjectInitializer);
};
