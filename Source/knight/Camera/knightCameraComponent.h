#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "GameplayTagContainer.h"
#include "knightCameraComponent.generated.h"

class UknightCameraMode;
class UknightCameraFadeHandler;
class UknightCameraModeStack;
class UknightCameraOcclusionHandler;

/** Use CameraMode while the owner's ASC has RequiredTag. Rules are checked in order; the first match wins. */
USTRUCT(BlueprintType)
struct FknightCameraModeRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Camera", meta=(Categories="State"))
	FGameplayTag RequiredTag;

	UPROPERTY(EditAnywhere, Category="Camera")
	TSubclassOf<UknightCameraMode> CameraMode;
};

/**
 * Camera driven by a blended stack of camera modes.
 *
 * Each frame the mode is chosen as: an active temporary override (parry punch-in, finisher), else the first
 * rule whose tag the owner has (State.Flying -> Flight, State.LockedOn -> LockOn, State.Boosting -> Boost),
 * else DefaultCameraMode. Mode changes blend over the new mode's BlendTime.
 *
 * With no DefaultCameraMode set, it behaves like a plain camera component (the template spring arm setup),
 * so Blueprints keep working until they are configured.
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightCameraComponent : public UCameraComponent
{
	GENERATED_BODY()

public:
	UknightCameraComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView) override;

	/** Forces a mode for Duration seconds regardless of rules (parry, finishers, scripted beats). */
	UFUNCTION(BlueprintCallable, Category="Knight|Camera")
	void SetTemporaryCameraMode(TSubclassOf<UknightCameraMode> CameraMode, float Duration);

	UFUNCTION(BlueprintCallable, Category="Knight|Camera")
	void ClearTemporaryCameraMode();

	/** Mode currently on top of the stack, for debugging. */
	UknightCameraMode* GetActiveCameraMode() const;

	/**
	 * (2026-10-23) 공격 휘두르기 카메라 연출. 기계식 타이밍(FknightMechAttackTiming)이 부른다.
	 * Windup = 준비 동작의 락(칼을 젖힌 순간): 시야각을 살짝 조인다. Swing = 휘두르는 순간: 조임을 풀며 시야각이 튀고
	 * 카메라가 보는 방향으로 밀렸다가 돌아온다. Strength 1 = 한손 기준(양손은 더 크게 넘긴다).
	 */
	void StartAttackWindup(float Strength);
	void StartAttackSwing(float Strength);

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera")
	TSubclassOf<UknightCameraMode> DefaultCameraMode;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera", meta=(TitleProperty="RequiredTag"))
	TArray<FknightCameraModeRule> CameraModeRules;

	UPROPERTY(EditDefaultsOnly, Instanced, Category="Knight|Camera")
	TObjectPtr<UknightCameraOcclusionHandler> OcclusionHandler;

	/** Fades or hides monsters and tagged props between the camera and the Knight. */
	UPROPERTY(EditDefaultsOnly, Instanced, Category="Knight|Camera")
	TObjectPtr<UknightCameraFadeHandler> FadeHandler;

	/**
	 * (2026-10-23 사용자 요청 '휘두르는 건 묵직한데 화면이 너무 정적') 공격 휘두르기 카메라 연출을 쓸지.
	 * PIE 콘솔 knight.Camera.AttackKick 0/1로 바로 비교할 수 있다. 아래 값은 모두 바뀔 수 있는 값.
	 * (2026-10-26 사용자 '공격 때 카메라 흔들림은 일단 빼고') 기본으로 끔. 다시 쓰려면 BP_Knight에서 켜면 된다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera|Attack Kick")
	bool bAttackCameraKick = false;

	/**
	 * 준비 동작(락) 동안 시야각 변화(음수 = 조임). 휘두르면 SwingRiseTime 동안 풀린다.
	 * (2026-10-26 사용자 '너무 인위적으로 흔들림') 크기를 절반으로(조임 -2 → -1, 튐 4 → 2, 밀기 25 → 12)
	 * 하고, 한 프레임에 최대치로 튀던 것을 부드럽게 올라갔다 내려오게 바꿨다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera|Attack Kick", meta=(Units="deg", EditCondition="bAttackCameraKick"))
	float WindupFOV = -1.f;

	/** 휘두를 때 시야각이 넓어지는 크기. SwingRiseTime 동안 올라가고 SwingKickDuration에 0으로 돌아온다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera|Attack Kick", meta=(Units="deg", EditCondition="bAttackCameraKick"))
	float SwingFOV = 2.f;

	/** 휘두를 때 카메라를 보는 방향(캐릭터 쪽)으로 미는 거리. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera|Attack Kick", meta=(ClampMin=0, Units="cm", EditCondition="bAttackCameraKick"))
	float SwingPush = 12.f;

	/** 휘두르기 연출이 최대가 되기까지 걸리는 시간(이만큼 부드럽게 올라간다). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera|Attack Kick", meta=(ClampMin=0.01, Units="s", EditCondition="bAttackCameraKick"))
	float SwingRiseTime = 0.06f;

	/** 휘두르기 연출 전체 길이(올라가는 시간 포함). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera|Attack Kick", meta=(ClampMin=0.05, Units="s", EditCondition="bAttackCameraKick"))
	float SwingKickDuration = 0.45f;

	/** 준비 동작 조임이 들어가는 시간. 휘두르기가 안 오면(공격이 끊김) WindupTimeout 뒤 저절로 풀린다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera|Attack Kick", meta=(ClampMin=0.01, Units="s", EditCondition="bAttackCameraKick"))
	float WindupBlendTime = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera|Attack Kick", meta=(ClampMin=0.1, Units="s", EditCondition="bAttackCameraKick"))
	float WindupTimeout = 0.6f;

private:
	/** 지금 프레임의 공격 연출 시야각 변화와 앞으로 미는 거리. */
	void EvaluateAttackKick(float& OutFOV, float& OutPush) const;

	double WindupStartTime = -1.0;
	float WindupStrength = 0.f;
	double SwingStartTime = -1.0;
	float SwingStrength = 0.f;
	/** 휘두르기가 시작될 때 남아 있던 연출(준비 동작 조임·앞 공격의 휘두르기). 새 휘두르기가 올라가는 동안 부드럽게 풀어 끊김이 없게 한다. */
	float CarriedFOVAtSwing = 0.f;
	float CarriedPushAtSwing = 0.f;

	TSubclassOf<UknightCameraMode> DetermineCameraMode() const;

	UPROPERTY(Transient)
	TObjectPtr<UknightCameraModeStack> ModeStack;

	UPROPERTY(Transient)
	TSubclassOf<UknightCameraMode> TemporaryMode;

	double TemporaryModeEndTime = 0.0;
};
