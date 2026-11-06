#pragma once
#include "Commandlets/Commandlet.h"
#include "knightCraftResultCheckCommandlet.generated.h"

/** Isolated real crafting/equipment transactions and offscreen UMG render checks. */
UCLASS()
class UknightCraftResultCheckCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	virtual int32 Main(const FString& Params) override;
};
