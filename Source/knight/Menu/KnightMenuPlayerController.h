#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "KnightMenuPlayerController.generated.h"

/** Shared local controller for title and main menu; widget ownership follows in step 2. */
UCLASS()
class KNIGHT_API AKnightMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AKnightMenuPlayerController();
};
