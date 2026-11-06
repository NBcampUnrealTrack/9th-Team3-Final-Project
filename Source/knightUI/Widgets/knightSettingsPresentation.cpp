#include "Widgets/knightSettingsPresentation.h"
#include "Brushes/SlateNoResource.h"

UknightSettingsButtonStyle::UknightSettingsButtonStyle()
{
	NormalBase=NormalHovered=NormalPressed=SelectedBase=SelectedHovered=SelectedPressed=Disabled=FSlateNoResource();
	ButtonPadding=FMargin(); CustomPadding=FMargin(); MinWidth=MinHeight=0;
}
