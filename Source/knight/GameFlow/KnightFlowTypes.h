#pragma once

#include "CoreMinimal.h"
#include "KnightFlowTypes.generated.h"

/** The active level, independent of pages shown inside a menu widget. */
UENUM(BlueprintType)
enum class EKnightFlowState : uint8
{
	Uninitialized,
	Title,
	MainMenu,
	Gameplay
};
