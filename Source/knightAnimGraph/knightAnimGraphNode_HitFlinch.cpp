#include "knightAnimGraphNode_HitFlinch.h"

#define LOCTEXT_NAMESPACE "knightAnimGraphNode_HitFlinch"

FText UAnimGraphNode_knightHitFlinch::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Title", "Knight Hit Flinch");
}

FText UAnimGraphNode_knightHitFlinch::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Adds the monster's hit animation on top of the current pose for a moment on every hit, toward the side that was hit. Does not interrupt attacks. Settings are in the monster definition (Hit Flinch).");
}

FString UAnimGraphNode_knightHitFlinch::GetNodeCategory() const
{
	return TEXT("Knight");
}

#undef LOCTEXT_NAMESPACE
