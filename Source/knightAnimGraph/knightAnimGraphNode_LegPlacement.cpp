#include "knightAnimGraphNode_LegPlacement.h"

#define LOCTEXT_NAMESPACE "knightAnimGraphNode_LegPlacement"

FText UAnimGraphNode_knightLegPlacement::GetControllerDescription() const
{
	return LOCTEXT("ControllerDescription", "Knight Leg Placement");
}

FText UAnimGraphNode_knightLegPlacement::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetControllerDescription();
}

FText UAnimGraphNode_knightLegPlacement::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Puts a large monster's feet on the ground: lowers and tilts the pelvis to the ground under the feet, then fixes each leg with two-bone IK. Set Legs (ankle bones) and Pelvis Bone.");
}

#undef LOCTEXT_NAMESPACE
