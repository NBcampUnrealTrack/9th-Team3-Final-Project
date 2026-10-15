#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_SkeletalControlBase.h"
#include "Animation/knightAnimNode_StanceLegs.h"
#include "knightAnimGraphNode_StanceLegs.generated.h"

/** 애님 그래프의 "Knight Stance Legs" 노드. 실제 동작은 FAnimNode_knightStanceLegs에 있다. (2026-10-15) */
UCLASS()
class UAnimGraphNode_knightStanceLegs : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Settings")
	FAnimNode_knightStanceLegs Node;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;

protected:
	virtual FText GetControllerDescription() const override;
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
};
