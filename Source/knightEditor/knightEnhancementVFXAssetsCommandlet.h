#pragma once

#include "Commandlets/Commandlet.h"
#include "knightEnhancementVFXAssetsCommandlet.generated.h"

/** Authors only the dedicated enhancement VFX assets and their maintenance widget references. */
UCLASS()
class UknightEnhancementVFXAssetsCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	virtual int32 Main(const FString& Params) override;
};
