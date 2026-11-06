#pragma once
#include "Commandlets/Commandlet.h"
#include "knightOperationMapCheckCommandlet.generated.h"

UCLASS()
class UknightOperationMapCheckCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	virtual int32 Main(const FString& Params) override;
};
