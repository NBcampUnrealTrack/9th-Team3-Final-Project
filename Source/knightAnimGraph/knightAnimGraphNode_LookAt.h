#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_SkeletalControlBase.h"
#include "Animation/knightAnimNode_LookAt.h"
#include "knightAnimGraphNode_LookAt.generated.h"

/** 애님 그래프의 "Knight Look At" 노드. 실제 동작은 FAnimNode_knightLookAt에 있다. */
UCLASS()
class UAnimGraphNode_knightLookAt : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Settings")
	FAnimNode_knightLookAt Node;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;

protected:
	virtual FText GetControllerDescription() const override;
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
};
