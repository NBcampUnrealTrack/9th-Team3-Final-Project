#include "knightAnimGraphNode_WingClearance.h"

#define LOCTEXT_NAMESPACE "knightAnimGraphNode_WingClearance"

FText UAnimGraphNode_knightWingClearance::GetControllerDescription() const
{
	return LOCTEXT("ControllerDescription", "Knight Wing Clearance");
}

FText UAnimGraphNode_knightWingClearance::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetControllerDescription();
}

FText UAnimGraphNode_knightWingClearance::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Keeps a low-hovering flyer's wing tips above the ground by lifting each wing at its root bone. Set Wings (root bone and tip bones).");
}

#undef LOCTEXT_NAMESPACE
