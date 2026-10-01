#include "Settings/knightGameUserSettings.h"

#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

namespace
{
	void SetConsoleInt(const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* const Variable = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Variable->Set(Value, ECVF_SetByGameSetting);
		}
	}
}

UknightGameUserSettings* UknightGameUserSettings::Get()
{
	return GEngine ? Cast<UknightGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void UknightGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	Upscaler = EknightUpscaler::TSR;
	bMotionBlurEnabled = true;
}

void UknightGameUserSettings::ApplyNonResolutionSettings()
{
	Super::ApplyNonResolutionSettings();

	// r.AntiAliasingMethod: 0 none, 2 TAA, 4 TSR.
	switch (Upscaler)
	{
	case EknightUpscaler::TSR:
		SetConsoleInt(TEXT("r.AntiAliasingMethod"), 4);
		break;
	case EknightUpscaler::TAA:
		SetConsoleInt(TEXT("r.AntiAliasingMethod"), 2);
		break;
	default:
		SetConsoleInt(TEXT("r.AntiAliasingMethod"), 0);
		break;
	}
	SetConsoleInt(TEXT("r.MotionBlurQuality"), bMotionBlurEnabled ? 4 : 0);
}
