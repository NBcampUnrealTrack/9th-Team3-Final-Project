#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "knightAnimNotifyState_GameplayTagWindow.generated.h"

/**
 * Adds a loose gameplay tag to the owner's ASC for the notify's duration.
 * Use for Window.Combo, Window.Cancel, State.Invulnerable (evasion frames), State.SuperArmor
 * and State.Parrying. Loose tags are counted, so overlapping windows with the same tag are safe.
 */
UCLASS(meta=(DisplayName="Knight Gameplay Tag Window"))
class KNIGHT_API UknightAnimNotifyState_GameplayTagWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	UPROPERTY(EditAnywhere, Category="Window", meta=(Categories="Window,State"))
	FGameplayTag Tag;
};
