#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "KnightGameInstance.generated.h"

/** Process lifetime entry point. State remains in the save, progression, flow and raid subsystems. */
UCLASS()
class KNIGHT_API UKnightGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void OnStart() override;
};
