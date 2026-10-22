#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "knightCameraModifier_Deafen.generated.h"

class UAudioComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USoundBase;
class USoundClass;
class USoundMix;

/**
 * 초음파에 맞아 귀가 먹먹해지는 연출(잔향 초음파 발사). 맞는 순간 게임 소리가 MinVolume으로 줄었다가 Duration 동안 원래대로 돌아오고,
 * 그동안 화면이 흔들리고 일그러진다. 흔들림과 일그러짐도 같은 시간 동안 약해진다.
 *
 * 다시 맞으면 세기는 그대로 두고 시간만 처음부터 다시 센다. 소리 줄이기는 MutedClasses에만 걸린다.
 * TinnitusSound(이명)는 MutedClasses가 아닌 사운드 클래스로 두어야 함께 줄지 않는다. 비우면 이명 없이 나머지만 한다.
 */
UCLASS()
class KNIGHT_API UknightCameraModifier_Deafen : public UCameraModifier
{
	GENERATED_BODY()

public:
	UknightCameraModifier_Deafen();

	/** Seconds 동안 먹먹해진다. 이미 먹먹하면 시간만 다시 센다. */
	void StartDeafen(float Seconds);

	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
	virtual void ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings) override;
	virtual void BeginDestroy() override;

protected:
	/** 아래 파라미터를 읽는 후처리 머티리얼(M_PP_Deafen). */
	UPROPERTY(EditDefaultsOnly, Category="Deafen")
	TSoftObjectPtr<UMaterialInterface> Material;

	/** 줄이는 사운드 클래스. 기본은 SC_Knight_SFX, SC_Knight_BGM, SC_Knight_Voice(하위 클래스 포함). */
	UPROPERTY(EditDefaultsOnly, Category="Deafen|Audio")
	TArray<TSoftObjectPtr<USoundClass>> MutedClasses;

	/** 맞는 순간의 볼륨 배율. 0.1이면 90% 줄어든다. */
	UPROPERTY(EditDefaultsOnly, Category="Deafen|Audio", meta=(ClampMin=0, ClampMax=1))
	float MinVolume = 0.1f;

	/** 이명. 비우면 재생하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Deafen|Audio")
	TSoftObjectPtr<USoundBase> TinnitusSound;

	/** 화면 흔들림 세기. 1 = 약 0.6도. 설정의 화면 흔들림 배율을 따른다. */
	UPROPERTY(EditDefaultsOnly, Category="Deafen|Look", meta=(ClampMin=0))
	float Shake = 0.6f;

	/** 화면 일그러짐 세기(화면 높이 비율). */
	UPROPERTY(EditDefaultsOnly, Category="Deafen|Look", meta=(ClampMin=0))
	float Distortion = 0.006f;

	UPROPERTY(EditDefaultsOnly, Category="Deafen|Look", meta=(ClampMin=0))
	float ChromaticAberration = 0.5f;

private:
	void SetVolume(float Volume);
	void StopAudio();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> SoundMix;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> TinnitusComponent;

	float Duration = 0.f;
	float Age = 0.f;
	/** 1 = 맞은 순간, 0 = 다 돌아옴. */
	float Strength = 0.f;
	float LastVolume = 1.f;
	float ShakeTime = 0.f;
	bool bMixPushed = false;
	/** 설정의 화면 흔들림 배율. 0이면 화면 연출을 끈다(소리는 그대로). */
	float LookScale = 1.f;
};
