#pragma once
#include "Commandlets/Commandlet.h"
#include "knightMaintenanceEntranceAssetsCommandlet.generated.h"

/** Builds only the two maintenance entrance materials and their widget references. */
UCLASS()
class UknightMaintenanceEntranceAssetsCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	virtual int32 Main(const FString& Params) override;
};
