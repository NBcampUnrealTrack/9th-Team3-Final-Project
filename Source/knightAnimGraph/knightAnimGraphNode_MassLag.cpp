#include "knightAnimGraphNode_MassLag.h"

#define LOCTEXT_NAMESPACE "knightAnimGraphNode_MassLag"

FText UAnimGraphNode_knightMassLag::GetControllerDescription() const
{
	return LOCTEXT("ControllerDescription", "Knight Mass Lag");
}

FText UAnimGraphNode_knightMassLag::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetControllerDescription();
}

FText UAnimGraphNode_knightMassLag::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Heavy machine inertia: the listed spine bones follow the animation with a damped spring, so the upper body trails the swing and overshoots a little when it stops.");
}

#undef LOCTEXT_NAMESPACE
