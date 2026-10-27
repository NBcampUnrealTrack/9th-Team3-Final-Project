#include "Camera/knightCameraModifier_Darkness.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

UknightCameraModifier_Darkness::UknightCameraModifier_Darkness()
{
	Material = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Knight/Camera/PostProcess/M_PP_Darkness.M_PP_Darkness")));
}

void UknightCameraModifier_Darkness::StartDarkness(UPrimitiveComponent* Source)
{
	if (DarknessSource.Get() != Source)
	{
		SetCustomDepth(false);
		bSilhouette = false;
	}
	DarknessSource = Source;
	bDarkness = Source != nullptr;
}

void UknightCameraModifier_Darkness::StopDarkness(const UPrimitiveComponent* Source)
{
	if (DarknessSource.Get() == Source)
	{
		bDarkness = false;
		bSilhouette = false;
	}
}

void UknightCameraModifier_Darkness::SetSilhouette(const UPrimitiveComponent* Source, bool bVisible)
{
	if (DarknessSource.Get() == Source)
	{
		bSilhouette = bVisible && bDarkness;
	}
}

void UknightCameraModifier_Darkness::SetCustomDepth(bool bOn)
{
	UPrimitiveComponent* const Source = DarknessSource.Get();
	if (bOn == bCustomDepthOn || !Source)
	{
		bCustomDepthOn = bOn && Source;
		return;
	}
	Source->SetRenderCustomDepth(bOn);
	bCustomDepthOn = bOn;
}

bool UknightCameraModifier_Darkness::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	if (bDarkness && !DarknessSource.IsValid())
	{
		bDarkness = false;
		bSilhouette = false;
	}
	Alpha = FMath::Clamp(Alpha + DeltaTime / (bDarkness ? FadeInTime : -FadeOutTime), 0.f, 1.f);
	SilhouetteAlpha = FMath::Clamp(SilhouetteAlpha + DeltaTime / (bSilhouette ? SilhouetteFadeTime : -SilhouetteFadeTime), 0.f, 1.f);
	// 윤곽선이 다 사라진 뒤에 커스텀 뎁스를 끈다. 끄는 순간 윤곽선이 툭 끊기지 않게 한다.
	SetCustomDepth(bSilhouette || SilhouetteAlpha > 0.f);

	if (Alpha > 0.f)
	{
		if (!MaterialInstance)
		{
			if (UMaterialInterface* const Base = Material.LoadSynchronous())
			{
				MaterialInstance = UMaterialInstanceDynamic::Create(Base, this);
			}
		}
		if (MaterialInstance)
		{
			// 머티리얼은 화소 위치를 카메라 기준으로 되살린다. 플레이어 위치도 카메라 기준으로 넘겨 큰 월드 좌표의 정밀도 손실을 피한다.
			const APlayerController* const PC = CameraOwner ? CameraOwner->GetOwningPlayerController() : nullptr;
			const APawn* const Pawn = PC ? PC->GetPawn() : nullptr;
			const FVector Center = (Pawn ? Pawn->GetActorLocation() : InOutPOV.Location) - InOutPOV.Location;
			const FVector Forward = InOutPOV.Rotation.Vector();
			MaterialInstance->SetVectorParameterValue(TEXT("Center"), FLinearColor(Center.X, Center.Y, Center.Z, Radius));
			MaterialInstance->SetVectorParameterValue(TEXT("CameraForward"), FLinearColor(Forward.X, Forward.Y, Forward.Z, Feather));
			MaterialInstance->SetVectorParameterValue(TEXT("Look"), FLinearColor(Alpha, OutsideBrightness, SilhouetteAlpha * SilhouetteStrength, 0.f));
			MaterialInstance->SetVectorParameterValue(TEXT("SilhouetteColor"), SilhouetteColor);
		}
	}
	return Super::ModifyCamera(DeltaTime, InOutPOV);
}

void UknightCameraModifier_Darkness::ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings)
{
	if (MaterialInstance && Alpha > 0.f)
	{
		PostProcessSettings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, MaterialInstance));
		PostProcessBlendWeight = 1.f;
	}
}

void UknightCameraModifier_Darkness::GatherPreloadAssets(TArray<FSoftObjectPath>& OutPaths) const
{
	if (!Material.IsNull())
	{
		OutPaths.AddUnique(Material.ToSoftObjectPath());
	}
}
