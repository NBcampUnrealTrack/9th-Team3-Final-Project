#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Combat/knightCombatTypes.h"
#include "knightAnimNotifyState_MeleeTrace.generated.h"

/** Opens the weapon trace window on the owner's UknightMeleeTraceComponent for the notify's duration. */
UCLASS(meta=(DisplayName="Knight Melee Trace"))
class KNIGHT_API UknightAnimNotifyState_MeleeTrace : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	UPROPERTY(EditAnywhere, Category="Trace", meta=(ShowOnlyInnerProperties))
	FknightMeleeTraceParams TraceParams;
};
