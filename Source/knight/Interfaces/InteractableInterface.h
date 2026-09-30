#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractableInterface.generated.h"

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

/** Gathering points, raid exits and other things the Knight can use. */
class KNIGHT_API IInteractableInterface
{
	GENERATED_BODY()

public:
	virtual bool CanInteract(const AActor* Interactor) const = 0;

	/** Prompt text shown by the interaction UI. */
	virtual FText GetInteractionPrompt() const = 0;

	/** Seconds the input must be held. Zero means a single press. */
	virtual float GetInteractionDuration() const { return 0.f; }

	virtual void Interact(AActor* Interactor) = 0;
};
