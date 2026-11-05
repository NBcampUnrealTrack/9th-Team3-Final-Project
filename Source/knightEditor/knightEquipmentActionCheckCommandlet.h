#pragma once
#include "Commandlets/Commandlet.h"
#include "knightEquipmentActionCheckCommandlet.generated.h"

/** Isolated equipment replacement, layout and real offscreen UMG render checks. */
UCLASS()
class UknightEquipmentActionCheckCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	virtual int32 Main(const FString& Params) override;
};
