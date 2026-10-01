#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "knightBossAIController.generated.h"

class UAISenseConfig_Damage;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;
class UStateTreeAIComponent;

/**
 * Controller for boss monsters. Runs the monster's StateTree and feeds perception into the monster's
 * UknightMonsterTargetingComponent, which owns the choice of target.
 *
 * The StateTree asset is set per monster on the Blueprint subclass (StateTreeAI component). Sight range
 * is large by default because giant monsters notice the Knight from far away; tune per species.
 */
UCLASS(Abstract)
class KNIGHT_API AknightBossAIController : public AAIController
{
	GENERATED_BODY()

public:
	AknightBossAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UStateTreeAIComponent* GetStateTreeComponent() const { return StateTreeAI; }

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UFUNCTION()
	void HandleTargetPerceptionForgotten(AActor* Actor);

private:
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStateTreeAIComponent> StateTreeAI;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UAIPerceptionComponent> Perception;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Damage> DamageConfig;
};
