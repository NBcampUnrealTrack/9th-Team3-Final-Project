#include "Foundation/knightActivatableWidget.h"

TOptional<FUIInputConfig> UknightActivatableWidget::GetDesiredInputConfig() const
{
	switch (InputMode)
	{
	case EknightWidgetInputMode::GameAndMenu:
		return FUIInputConfig(ECommonInputMode::All, GameMouseCaptureMode);
	case EknightWidgetInputMode::Game:
		return FUIInputConfig(ECommonInputMode::Game, GameMouseCaptureMode);
	case EknightWidgetInputMode::Menu:
		return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	default:
		return TOptional<FUIInputConfig>();
	}
}
