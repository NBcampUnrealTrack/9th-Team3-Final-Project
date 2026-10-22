#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "knightCameraModifier_Darkness.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPrimitiveComponent;

/**
 * 몬스터가 시야를 덮는 어둠(잔향 활강 꼬리 찌르기). 플레이어에서 Radius 안만 보이고 바깥은 거의 검게 된다.
 * 거리는 화면이 아니라 월드 거리라서 카메라를 돌리거나 멀리 빼도 플레이어 둘레의 보이는 범위는 그대로다. 소리와 HUD는 건드리지 않는다.
 *
 * 실루엣을 켜면 그 몬스터 메시를 커스텀 뎁스로 그려 어둠 속에서도 윤곽선만 희미하게 보인다(찌르기 직전 방향을 알려 주는 용도).
 *
 * 여러 몬스터가 동시에 켜면 마지막에 켠 몬스터를 따른다. 어둠을 켠 몬스터가 사라지면 저절로 꺼진다.
 */
UCLASS()
class KNIGHT_API UknightCameraModifier_Darkness : public UCameraModifier
{
	GENERATED_BODY()

public:
	UknightCameraModifier_Darkness();

	/** Source(어둠을 켠 몬스터의 메시)가 어둠을 켠다. 실루엣은 이 메시로 그린다. */
	void StartDarkness(UPrimitiveComponent* Source);

	/** Source가 켠 어둠을 FadeOutTime 동안 끈다. */
	void StopDarkness(const UPrimitiveComponent* Source);

	/** Source의 윤곽선을 어둠 속에 보이거나 숨긴다. */
	void SetSilhouette(const UPrimitiveComponent* Source, bool bVisible);

	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
	virtual void ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings) override;

protected:
	/** 아래 파라미터를 읽는 후처리 머티리얼(M_PP_Darkness). */
	UPROPERTY(EditDefaultsOnly, Category="Darkness")
	TSoftObjectPtr<UMaterialInterface> Material;

	/** 플레이어에서 이 거리(cm)까지 보인다. */
	UPROPERTY(EditDefaultsOnly, Category="Darkness", meta=(ClampMin=0, Units="cm"))
	float Radius = 600.f;

	/** Radius 안쪽으로 이만큼(cm)에 걸쳐 서서히 어두워진다. */
	UPROPERTY(EditDefaultsOnly, Category="Darkness", meta=(ClampMin=1, Units="cm"))
	float Feather = 250.f;

	/** 바깥의 밝기. 0이면 완전히 검다. */
	UPROPERTY(EditDefaultsOnly, Category="Darkness", meta=(ClampMin=0, ClampMax=1))
	float OutsideBrightness = 0.04f;

	UPROPERTY(EditDefaultsOnly, Category="Darkness", meta=(ClampMin=0.01, Units="s"))
	float FadeInTime = 1.f;

	/** 어둠이 걷히는 시간. 길게 두어 공격이 끝난 뒤에도 천천히 풀린다. */
	UPROPERTY(EditDefaultsOnly, Category="Darkness", meta=(ClampMin=0.01, Units="s"))
	float FadeOutTime = 3.f;

	/** 윤곽선 밝기(0~1). "살짝" 보이는 정도로 둔다. */
	UPROPERTY(EditDefaultsOnly, Category="Darkness|Silhouette", meta=(ClampMin=0, ClampMax=1))
	float SilhouetteStrength = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category="Darkness|Silhouette")
	FLinearColor SilhouetteColor = FLinearColor(0.55f, 0.65f, 0.85f);

	UPROPERTY(EditDefaultsOnly, Category="Darkness|Silhouette", meta=(ClampMin=0.01, Units="s"))
	float SilhouetteFadeTime = 0.1f;

private:
	void SetCustomDepth(bool bOn);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MaterialInstance;

	TWeakObjectPtr<UPrimitiveComponent> DarknessSource;
	bool bDarkness = false;
	bool bSilhouette = false;
	bool bCustomDepthOn = false;
	/** 0 = 어둠 없음, 1 = 다 어두움. */
	float Alpha = 0.f;
	float SilhouetteAlpha = 0.f;
};
