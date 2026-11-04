#pragma once

#include "Commandlets/Commandlet.h"
#include "knightEnhancementPreviewCheckCommandlet.generated.h"

/** Read-only component render regression check; no profile, world, roll or save is created. */
UCLASS()
class UknightEnhancementPreviewCheckCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	virtual int32 Main(const FString& Params) override;
};
