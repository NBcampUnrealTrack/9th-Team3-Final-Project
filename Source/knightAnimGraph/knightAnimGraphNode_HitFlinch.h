#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_Base.h"
#include "Animation/knightAnimNode_HitFlinch.h"
#include "knightAnimGraphNode_HitFlinch.generated.h"

/** 애님 그래프의 "Knight Hit Flinch" 노드. 실제 동작은 FAnimNode_knightHitFlinch에 있다. */
UCLASS()
class UAnimGraphNode_knightHitFlinch : public UAnimGraphNode_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Settings")
	FAnimNode_knightHitFlinch Node;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FString GetNodeCategory() const override;
};
