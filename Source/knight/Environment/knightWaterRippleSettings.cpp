#include "Environment/knightWaterRippleSettings.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

UknightWaterRippleSettings::UknightWaterRippleSettings()
{
	OutputTarget = TSoftObjectPtr<UTextureRenderTarget2D>(FSoftObjectPath(TEXT("/Game/Knight/Environment/Water/RT_WaterRipple.RT_WaterRipple")));
	StepMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Knight/Environment/Water/M_WaterRipple_Step.M_WaterRipple_Step")));
	NormalMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Knight/Environment/Water/M_WaterRipple_Normal.M_WaterRipple_Normal")));
	PreviewWaterMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Knight/Environment/Water/MI_Water_Lake_Ripple.MI_Water_Lake_Ripple")));
	SplashSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Knight/Environment/Water/Splash/Systems/PSN_WaterLight_Surface.PSN_WaterLight_Surface")));
	SplashHeavySystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Knight/Environment/Water/Splash/Systems/PSN_WaterHeavy_Surface.PSN_WaterHeavy_Surface")));
}
