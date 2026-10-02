#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GenericTeamAgentInterface.h"
#include "Interfaces/CombatInterface.h"
#include "Interfaces/InteractableInterface.h"
#include "Interfaces/TargetableInterface.h"
#include "KnightCoreInterfaceProbe.generated.h"

/** Development actor for C++ and Blueprint interface dispatch checks. */
UCLASS(Blueprintable)
class KNIGHT_API AKnightCoreInterfaceProbe : public AActor, public ICombatInterface,
	public ITargetableInterface, public IInteractableInterface, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	AKnightCoreInterfaceProbe();
	virtual bool CanReceiveHit(const AActor* Attacker) const override { return true; }
	virtual bool IsDead() const override { return false; }
	virtual bool IsTargetable() const override { return true; }
	virtual void GetTargetPoints(TArray<FknightTargetPoint>& OutPoints) const override;
	virtual float GetTargetRadius() const override { return 100.f; }
	virtual bool CanInteract_Implementation(AActor* Interactor) const override { return true; }
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual FGenericTeamId GetGenericTeamId() const override { return FGenericTeamId::NoTeam; }
};
