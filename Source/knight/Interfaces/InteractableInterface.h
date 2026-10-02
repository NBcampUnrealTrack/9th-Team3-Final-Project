#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractableInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

/** Gathering points, raid exits and other things the Knight can use. */
class KNIGHT_API IInteractableInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Knight|Interaction")
	bool CanInteract(AActor* Interactor) const;
	virtual bool CanInteract_Implementation(AActor* Interactor) const { return false; }

	/** Prompt text shown by the interaction UI. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Knight|Interaction")
	FText GetInteractionPrompt() const;
	virtual FText GetInteractionPrompt_Implementation() const { return FText::GetEmpty(); }

	/** Seconds the input must be held. Zero means a single press. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Knight|Interaction")
	float GetInteractionDuration() const;
	virtual float GetInteractionDuration_Implementation() const { return 0.f; }

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Knight|Interaction")
	void Interact(AActor* Interactor);
	virtual void Interact_Implementation(AActor* Interactor) {}
};
