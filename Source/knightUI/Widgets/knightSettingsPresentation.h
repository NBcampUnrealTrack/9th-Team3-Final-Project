#pragma once
#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "knightSettingsPresentation.generated.h"

/** Settings owns its static state layer. CommonUI retains focus, input and sounds. */
UCLASS()
class KNIGHTUI_API UknightSettingsButtonStyle : public UCommonButtonStyle
{
	GENERATED_BODY()
public:
	UknightSettingsButtonStyle();
};
