#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "TargetableInterface.generated.h"

/** One lock-on point. Stores a component and socket so the point follows animation. */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightTargetPoint
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Targeting")
	TObjectPtr<USceneComponent> Component = nullptr;

	/** NAME_None uses the component location. */
	UPROPERTY(BlueprintReadOnly, Category="Targeting")
	FName SocketName;

	/** Part under Monster.Part, or empty for a single-point target. */
	UPROPERTY(BlueprintReadOnly, Category="Targeting")
	FGameplayTag PartTag;

	FVector GetLocation() const
	{
		return Component ? Component->GetSocketLocation(SocketName) : FVector::ZeroVector;
	}
};

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UTargetableInterface : public UInterface
{
	GENERATED_BODY()
};

/** Rules for lock-on. Giant monsters expose several points so the player can switch between parts. */
class KNIGHT_API ITargetableInterface
{
	GENERATED_BODY()

public:
	virtual bool IsTargetable() const = 0;

	/** Appends points in a stable order; the first point is the default lock target. */
	virtual void GetTargetPoints(TArray<FknightTargetPoint>& OutPoints) const = 0;

	/** Approximate radius in cm, used by the lock-on camera to frame the target. */
	virtual float GetTargetRadius() const = 0;
};
