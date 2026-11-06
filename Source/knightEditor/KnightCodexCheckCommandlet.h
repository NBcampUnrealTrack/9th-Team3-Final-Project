#pragma once
#include "Commandlets/Commandlet.h"
#include "KnightCodexCheckCommandlet.generated.h"

/** Opt-in, isolated static Codex capture using the real menu and compiled widgets. */
UCLASS()
class UKnightCodexCheckCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    virtual int32 Main(const FString& Params) override;
};
