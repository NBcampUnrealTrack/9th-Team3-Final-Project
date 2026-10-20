#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_SkeletalControlBase.h"
#include "Animation/knightAnimNode_MassLag.h"
#include "knightAnimGraphNode_MassLag.generated.h"

/** 애님 그래프의 "Knight Mass Lag" 노드. 실제 동작은 FAnimNode_knightMassLag에 있다. (2026-10-20) */
UCLASS()
class UAnimGraphNode_knightMassLag : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Settings")
	FAnimNode_knightMassLag Node;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;

protected:
	virtual FText GetControllerDescription() const override;
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
};
