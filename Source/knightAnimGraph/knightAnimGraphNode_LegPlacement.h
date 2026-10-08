#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_SkeletalControlBase.h"
#include "Animation/knightAnimNode_LegPlacement.h"
#include "knightAnimGraphNode_LegPlacement.generated.h"

/** 애님 그래프의 "Knight Leg Placement" 노드. 실제 동작은 FAnimNode_knightLegPlacement에 있다. */
UCLASS()
class UAnimGraphNode_knightLegPlacement : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Settings")
	FAnimNode_knightLegPlacement Node;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;

protected:
	virtual FText GetControllerDescription() const override;
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
};
