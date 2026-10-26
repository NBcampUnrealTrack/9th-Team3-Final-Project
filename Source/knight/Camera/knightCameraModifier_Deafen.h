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
 * 맞는 순간에는 PulseTime 동안 화면 가운데서 링 하나가 퍼지며 화면을 바깥으로 밀고 빠르게 떨린다(타격). 그 뒤의 일그러짐은
 * 가운데보다 가장자리에 남는다(귀울림). 음파가 맞지 않고 가까이 스치면 StartNearMiss로 가장자리 색 갈라짐만 짧게 보인다.
 * 초음파 브레스 기둥 아래에 있는 동안은 HoldJitter로 화면이 빠르게 떨리고 자글거린다(소리는 그대로).
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

	/** 음파가 가까이 스쳐 지나갔다. 소리는 그대로 두고 화면 가장자리 색만 NearMissTime 동안 갈라진다. */
	void StartNearMiss();

	/** 초음파 브레스 기둥 아래에 있다. 부르는 동안과 그 뒤 JitterTime 동안 화면이 빠르게 떨리고 일그러진다(소리는 그대로). */
	void HoldJitter();

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

	/** 맞는 순간의 충격 펄스 길이. 0이면 펄스 없이 먹먹함만 한다. */
	UPROPERTY(EditDefaultsOnly, Category="Deafen|Pulse", meta=(ClampMin=0, Units="s"))
	float PulseTime = 0.25f;

	/** 펄스 링이 화면을 바깥으로 미는 거리(화면 높이 비율). */
	UPROPERTY(EditDefaultsOnly, Category="Deafen|Pulse", meta=(ClampMin=0))
	float PulseDistortion = 0.03f;

	/** 펄스 동안 더하는 빠른 흔들림. 1 = 약 0.6도. */
	UPROPERTY(EditDefaultsOnly, Category="Deafen|Pulse", meta=(ClampMin=0))
	float PulseShake = 1.2f;

	UPROPERTY(EditDefaultsOnly, Category="Deafen|NearMiss", meta=(ClampMin=0, Units="s"))
	float NearMissTime = 0.15f;

	/** 스칠 때 가장자리 색 갈라짐 세기(ChromaticAberration과 같은 단위). */
	UPROPERTY(EditDefaultsOnly, Category="Deafen|NearMiss", meta=(ClampMin=0))
	float NearMissChromaticAberration = 1.2f;

	/** 기둥에서 벗어난 뒤 떨림이 잦아드는 시간. */
	UPROPERTY(EditDefaultsOnly, Category="Deafen|Jitter", meta=(ClampMin=0, Units="s"))
	float JitterTime = 0.2f;

	/** 기둥 아래에서의 빠른 떨림. 1 = 약 0.6도. */
	UPROPERTY(EditDefaultsOnly, Category="Deafen|Jitter", meta=(ClampMin=0))
	float JitterShake = 0.7f;

	/** 기둥 아래에서의 화면 일그러짐(화면 높이 비율). 먹먹함의 일그러짐 무늬를 빠르게 돌려 자글거리게 한다. */
	UPROPERTY(EditDefaultsOnly, Category="Deafen|Jitter", meta=(ClampMin=0))
	float JitterDistortion = 0.004f;

	/** 기둥 아래에서의 가장자리 색 갈라짐(ChromaticAberration과 같은 단위). */
	UPROPERTY(EditDefaultsOnly, Category="Deafen|Jitter", meta=(ClampMin=0))
	float JitterChromaticAberration = 0.9f;

private:
	void UpdateLookScale();

	void SetVolume(float Volume);
	UMaterialInstanceDynamic* GetMaterialInstance();
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
	/** 맞은 뒤 지난 시간(펄스용). PulseTime을 넘으면 펄스가 끝난다. */
	float PulseAge = TNumericLimits<float>::Max();
	float NearMissAge = TNumericLimits<float>::Max();
	/** 기둥에서 벗어난 뒤 지난 시간. HoldJitter가 0으로 되돌린다. */
	float JitterAge = TNumericLimits<float>::Max();
	/** 일그러짐 무늬 시간. 기둥 아래에서는 빨리 흐른다. */
	float WarpTime = 0.f;
	/** 이번 프레임에 후처리 머티리얼을 써야 하는지. */
	bool bLookActive = false;
	bool bMixPushed = false;
	/** 설정의 화면 흔들림 배율. 0이면 화면 연출을 끈다(소리는 그대로). */
	float LookScale = 1.f;
};
