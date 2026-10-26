#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "knightPlayerCameraManager.generated.h"

class UCameraShakeBase;
class UknightCameraModifier_Darkness;
class UknightCameraModifier_Deafen;
class UknightCameraModifier_RoarShockwave;
class UPrimitiveComponent;
class UknightGroggyFeedbackComponent;

/**
 * Global camera concerns only: pitch limits, combat shakes, and cinematic view targets (Sequencer camera
 * cuts go through the manager's view target as usual). Per-frame framing lives in UknightCameraComponent.
 */
UCLASS()
class KNIGHT_API AknightPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	AknightPlayerCameraManager();
	virtual void UpdateCamera(float DeltaTime) override;

	/** Applies the player's CameraShakeScale setting to every shake. */
	virtual UCameraShakeBase* StartCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass, const FAddCameraShakeParams& Params) override;
	using Super::StartCameraShake;

	/** Plays HitShake scaled by Intensity (0..1 typical). Used by hit, guard and parry feedback. */
	UFUNCTION(BlueprintCallable, Category="Knight|Camera")
	void PlayHitShake(float Intensity = 1.f);

	/** Plays HeavyImpactShake (monster slams, roars) scaled by Intensity. */
	UFUNCTION(BlueprintCallable, Category="Knight|Camera")
	void PlayHeavyImpactShake(float Intensity = 1.f);

	/** Starts the roar screen ripple from Source (a monster mesh) at Socket. See UknightCameraModifier_RoarShockwave. */
	void StartRoarShockwave(USceneComponent* Source, FName Socket);

	/** Stops sending new ripple rings for Source. */
	void StopRoarShockwave(const USceneComponent* Source);

	/** Source(몬스터 메시)가 플레이어 둘레만 보이게 시야를 덮는다. See UknightCameraModifier_Darkness. */
	void StartDarkness(UPrimitiveComponent* Source);
	void StopDarkness(const UPrimitiveComponent* Source);
	/** 어둠 속에서 Source의 윤곽선을 보이거나 숨긴다. */
	void SetDarknessSilhouette(const UPrimitiveComponent* Source, bool bVisible);

	/** Seconds 동안 소리가 줄고 화면이 흔들린다(초음파 피격). See UknightCameraModifier_Deafen. */
	UFUNCTION(BlueprintCallable, Category="Knight|Camera")
	void StartDeafen(float Seconds);

	/** 초음파가 맞지 않고 가까이 스쳤다. 화면 가장자리 색만 짧게 갈라진다. See UknightCameraModifier_Deafen. */
	void StartSonicNearMiss();

	/** 초음파 브레스 기둥 아래에 있다. 부르는 동안 화면이 빠르게 자글거린다. See UknightCameraModifier_Deafen. */
	void HoldSonicJitter();

	/** World의 로컬 플레이어 카메라마다 Function을 부른다. */
	static void ForEachLocalCamera(const UObject* WorldContext, TFunctionRef<void(AknightPlayerCameraManager&)> Function);

	/** Additive pre-hit zoom; holds at full weight until ReturnKnockdownZoom is called. */
	void StartKnockdownZoom(AActor* InViewTarget, float Degrees, float EnterSeconds, double StartRealTime);
	/** Ease out from the exact zoom weight reached at the hit or cancellation. */
	void ReturnKnockdownZoom(float ReturnSeconds);
	/** (2026-10-20) 맞는 순간 줌을 ExtraDegrees만큼 Seconds 동안 더 당긴다. ReturnKnockdownZoom이 불리면 함께 돌아간다. */
	void PunchKnockdownZoom(float ExtraDegrees, float Seconds);
	void CancelKnockdownZoom();

	/** The player's CameraShakeScale setting. Screen effects that move or distort the view scale with it too. */
	float GetShakeScale() const;
	void SetRadialPullSource(UknightGroggyFeedbackComponent* Source);

protected:
	virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override;
	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera")
	TSubclassOf<UCameraShakeBase> HitShake;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera")
	TSubclassOf<UCameraShakeBase> HeavyImpactShake;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera")
	TSubclassOf<UknightCameraModifier_RoarShockwave> RoarShockwaveClass;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera")
	TSubclassOf<UknightCameraModifier_Darkness> DarknessClass;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera")
	TSubclassOf<UknightCameraModifier_Deafen> DeafenClass;

private:
	TWeakObjectPtr<UknightGroggyFeedbackComponent> RadialPullSource;
	TWeakObjectPtr<AActor> KnockdownViewTarget;
	double KnockdownZoomStartTime = 0.0;
	double KnockdownZoomReturnStartTime = 0.0;
	float KnockdownZoomDegrees = 0.f;
	float KnockdownZoomEnter = 0.f;
	float KnockdownZoomReturn = 0.f;
	float KnockdownZoomReturnStartWeight = 0.f;
	bool bKnockdownZoomActive = false;
	bool bKnockdownPunched = false;
	float KnockdownPunchDegrees = 0.f;
	float KnockdownPunchTime = 0.f;
	double KnockdownPunchStartTime = 0.0;
	/** 지금 줌 각도 = 기본 + 펀치(올라가는 중이면 그만큼만). */
	float GetKnockdownZoomDegrees(double Now) const;
	bool bKnockdownZoomReturning = false;
	float GetKnockdownZoomWeight(double Now) const;
};
