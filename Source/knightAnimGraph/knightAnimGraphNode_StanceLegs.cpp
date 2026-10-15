#include "knightAnimGraphNode_StanceLegs.h"

#define LOCTEXT_NAMESPACE "knightAnimGraphNode_StanceLegs"

FText UAnimGraphNode_knightStanceLegs::GetControllerDescription() const
{
	return LOCTEXT("ControllerDescription", "Knight Stance Legs");
}

FText UAnimGraphNode_knightStanceLegs::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetControllerDescription();
}

FText UAnimGraphNode_knightStanceLegs::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Narrows and straightens the combat stance: raises the pelvis, pulls the feet toward the pelvis sideways (keeping them on the floor), and solves each leg with two-bone IK.");
}

#undef LOCTEXT_NAMESPACE
