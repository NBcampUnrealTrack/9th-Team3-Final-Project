#pragma once
#include "Commandlets/Commandlet.h"
#include "knightMaintenanceEntranceCheckCommandlet.generated.h"

UCLASS()
class UknightMaintenanceEntranceCheckCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	virtual int32 Main(const FString& Params) override;
};
