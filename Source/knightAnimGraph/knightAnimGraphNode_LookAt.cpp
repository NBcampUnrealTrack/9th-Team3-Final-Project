#include "knightAnimGraphNode_LookAt.h"

#define LOCTEXT_NAMESPACE "knightAnimGraphNode_LookAt"

FText UAnimGraphNode_knightLookAt::GetControllerDescription() const
{
	return LOCTEXT("ControllerDescription", "Knight Look At");
}

FText UAnimGraphNode_knightLookAt::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetControllerDescription();
}

FText UAnimGraphNode_knightLookAt::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Turns a monster's neck and head toward its current target while idle or moving. Set Bones from the neck root to the head with weights that add up to 1. Released during attacks, reactions, death and (optionally) the air turn.");
}

#undef LOCTEXT_NAMESPACE
