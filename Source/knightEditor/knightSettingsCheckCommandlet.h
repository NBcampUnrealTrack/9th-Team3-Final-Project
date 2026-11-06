#pragma once
#include "Commandlets/Commandlet.h"
#include "knightSettingsCheckCommandlet.generated.h"

/** Explicit, isolated Settings UI regression and real Slate RHI captures. */
UCLASS()
class UknightSettingsCheckCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    virtual int32 Main(const FString& Params) override;
};
