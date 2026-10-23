#include "Camera/knightCameraModifier_Deafen.h"

#include "Camera/knightPlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

UknightCameraModifier_Deafen::UknightCameraModifier_Deafen()
{
	Material = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Knight/Camera/PostProcess/M_PP_Deafen.M_PP_Deafen")));
	for (const TCHAR* const Path : { TEXT("/Game/Knight/Audio/Settings/SC_Knight_SFX.SC_Knight_SFX"),
		TEXT("/Game/Knight/Audio/Settings/SC_Knight_BGM.SC_Knight_BGM"), TEXT("/Game/Knight/Audio/Settings/SC_Knight_Voice.SC_Knight_Voice") })
	{
		MutedClasses.Add(TSoftObjectPtr<USoundClass>(FSoftObjectPath(Path)));
	}
}

void UknightCameraModifier_Deafen::StartDeafen(float Seconds)
{
	if (Seconds <= 0.f)
	{
		return;
	}
	Duration = Seconds;
	Age = 0.f;
	Strength = 1.f;
	PulseAge = 0.f;
	const AknightPlayerCameraManager* const KnightCamera = Cast<AknightPlayerCameraManager>(CameraOwner);
	LookScale = KnightCamera ? FMath::Clamp(KnightCamera->GetShakeScale(), 0.f, 2.f) : 1.f;

	if (!SoundMix)
	{
		SoundMix = NewObject<USoundMix>(this, TEXT("DeafenMix"));
		SoundMix->FadeInTime = 0.f;
		SoundMix->FadeOutTime = 0.1f;
	}
	SetVolume(MinVolume);
	if (!bMixPushed)
	{
		UGameplayStatics::PushSoundMixModifier(this, SoundMix);
		bMixPushed = true;
	}

	if (TinnitusComponent)
	{
		TinnitusComponent->Stop();
		TinnitusComponent = nullptr;
	}
	if (USoundBase* const Sound = TinnitusSound.LoadSynchronous())
	{
		TinnitusComponent = UGameplayStatics::SpawnSound2D(this, Sound);
	}
}

void UknightCameraModifier_Deafen::SetVolume(float Volume)
{
	for (const TSoftObjectPtr<USoundClass>& ClassPtr : MutedClasses)
	{
		if (USoundClass* const SoundClass = ClassPtr.LoadSynchronous())
		{
			UGameplayStatics::SetSoundMixClassOverride(this, SoundMix, SoundClass, Volume, 1.f, 0.f, true);
		}
	}
	LastVolume = Volume;
}

void UknightCameraModifier_Deafen::StopAudio()
{
	if (bMixPushed && SoundMix)
	{
		for (const TSoftObjectPtr<USoundClass>& ClassPtr : MutedClasses)
		{
			if (USoundClass* const SoundClass = ClassPtr.Get())
			{
				UGameplayStatics::ClearSoundMixClassOverride(this, SoundMix, SoundClass, 0.f);
			}
		}
		UGameplayStatics::PopSoundMixModifier(this, SoundMix);
	}
	bMixPushed = false;
	LastVolume = 1.f;
}

bool UknightCameraModifier_Deafen::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	PulseAge += DeltaTime;
	NearMissAge += DeltaTime;
	if (Strength > 0.f)
	{
		Age += DeltaTime;
		const float T = FMath::Clamp(Age / FMath::Max(Duration, KINDA_SMALL_NUMBER), 0.f, 1.f);
		// 처음에 빨리 돌아오고 끝으로 갈수록 천천히 다 돌아온다.
		Strength = FMath::Square(1.f - T);
		const float Volume = FMath::Lerp(1.f, MinVolume, Strength);
		if (T >= 1.f)
		{
			Strength = 0.f;
			StopAudio();
		}
		else if (FMath::Abs(Volume - LastVolume) > 0.01f)
		{
			SetVolume(Volume);
		}
	}

	const float PulseT = PulseTime > 0.f ? FMath::Clamp(PulseAge / PulseTime, 0.f, 1.f) : 1.f;
	const float Pulse = FMath::Square(1.f - PulseT) * LookScale;
	const float NearMiss = NearMissTime > 0.f ? FMath::Max(0.f, 1.f - NearMissAge / NearMissTime) * LookScale : 0.f;
	const float LookStrength = Strength * LookScale;
	bLookActive = LookStrength > 0.f || Pulse > 0.f || NearMiss > 0.f;
	if (!bLookActive)
	{
		return Super::ModifyCamera(DeltaTime, InOutPOV);
	}

	ShakeTime += DeltaTime;
	// 먹먹한 동안의 느린 흔들림에, 맞는 순간의 빠르고 센 떨림(펄스)을 더한다.
	const float ShakeAngle = Shake * 0.6f * LookStrength;
	const float PulseAngle = PulseShake * 0.6f * Pulse;
	InOutPOV.Rotation.Pitch += ShakeAngle * FMath::PerlinNoise1D(ShakeTime * 18.f) + PulseAngle * FMath::PerlinNoise1D(ShakeTime * 47.f + 11.3f);
	InOutPOV.Rotation.Yaw += ShakeAngle * 0.8f * FMath::PerlinNoise1D(ShakeTime * 18.f + 37.1f) + PulseAngle * 0.8f * FMath::PerlinNoise1D(ShakeTime * 47.f + 53.7f);
	InOutPOV.Rotation.Roll += ShakeAngle * 0.5f * FMath::PerlinNoise1D(ShakeTime * 11.f + 71.3f);
	if (UMaterialInstanceDynamic* const Instance = GetMaterialInstance())
	{
		Instance->SetVectorParameterValue(TEXT("Look"), FLinearColor(LookStrength, Distortion, ChromaticAberration, ShakeTime));
		// x: 펄스 링이 미는 거리, y: 링 반지름(화면 높이 비율, 0에서 퍼져 나감), z: 귀울림 가장자리 쏠림(펄스가 끝나면 1),
		// w: 스칠 때 가장자리 색 갈라짐.
		Instance->SetVectorParameterValue(TEXT("Pulse"), FLinearColor(Pulse * PulseDistortion, PulseT * 0.9f, PulseT, NearMiss * NearMissChromaticAberration));
	}
	return Super::ModifyCamera(DeltaTime, InOutPOV);
}

UMaterialInstanceDynamic* UknightCameraModifier_Deafen::GetMaterialInstance()
{
	if (!MaterialInstance)
	{
		if (UMaterialInterface* const Base = Material.LoadSynchronous())
		{
			MaterialInstance = UMaterialInstanceDynamic::Create(Base, this);
		}
	}
	return MaterialInstance;
}

void UknightCameraModifier_Deafen::StartNearMiss()
{
	NearMissAge = 0.f;
	if (Strength <= 0.f)
	{
		const AknightPlayerCameraManager* const KnightCamera = Cast<AknightPlayerCameraManager>(CameraOwner);
		LookScale = KnightCamera ? FMath::Clamp(KnightCamera->GetShakeScale(), 0.f, 2.f) : 1.f;
	}
}

void UknightCameraModifier_Deafen::ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings)
{
	if (MaterialInstance && bLookActive)
	{
		PostProcessSettings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, MaterialInstance));
		PostProcessBlendWeight = 1.f;
	}
}

void UknightCameraModifier_Deafen::BeginDestroy()
{
	// 레벨을 나갈 때 소리가 줄어든 채로 남지 않게 한다.
	if (bMixPushed && SoundMix && GetWorld())
	{
		StopAudio();
	}
	Super::BeginDestroy();
}
