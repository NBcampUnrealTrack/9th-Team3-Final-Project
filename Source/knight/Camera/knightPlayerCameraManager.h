#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "knightPlayerCameraManager.generated.h"

class UAbilitySystemComponent;
class UCameraShakeBase;
class UknightCameraImpactSettings;
class UknightCameraModifier_Impact;
struct FGameplayEffectSpec;
struct FGameplayEventData;
struct FknightCameraImpactReaction;
class UknightCameraModifier_Darkness;
class UknightCameraModifier_Deafen;
class UknightCameraModifier_RoarShockwave;
class UPrimitiveComponent;
class UknightGroggyFeedbackComponent;
struct FStreamableHandle;

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
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

	/**
	 * Source(몬스터 메시)가 플레이어 둘레만 보이게 시야를 덮는다. See UknightCameraModifier_Darkness.
	 * 덮는 동안 보고 있는 폰에 State.LockOn.Blocked를 붙여 락온을 풀고 막는다. 끄기 시작하면 바로 뗀다.
	 */
	void StartDarkness(UPrimitiveComponent* Source);
	void StopDarkness(const UPrimitiveComponent* Source);
	/** 어둠 속에서 Source의 윤곽선을 보이거나 숨긴다. */
	void SetDarknessSilhouette(const UPrimitiveComponent* Source, bool bVisible);
	/** 어둠 속 Location에 검붉은 빛무리를 Strength만큼 띄운다(날아드는 몬스터 경고). */
	void SetDarknessWarning(const UPrimitiveComponent* Source, const FVector& Location, float Strength);

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

	/**
	 * 카메라 반응 표(ImpactSettings)의 항목 하나를 바로 재생한다. Moment는 항목 이름(PlayerHeavyHit 등), Strength가 0 이하면
	 * 그 항목의 최대 세기로 재생한다. 켜짐 여부와 쿨다운은 보지 않는다(튜닝 탭의 미리보기용).
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Camera")
	void PreviewImpactReaction(FName Moment, float Strength = 0.f);
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

	/**
	 * 전투 순간별 카메라 반응 표. 보고 있는 플레이어의 전투 이벤트(때림, 맞음, 가드, 패링 …)와 몬스터 착지 셰이크를 듣고 재생한다.
	 * 비워 두면 반응하지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera|Impact")
	TSoftObjectPtr<UknightCameraImpactSettings> ImpactSettings;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera|Impact")
	TSubclassOf<UknightCameraModifier_Impact> ImpactModifierClass;

	/**
	 * 그로기 예측 슬로우 중 캐릭터 전신과 검 궤적이 한눈에 들어오도록 카메라를 뒤로 살짝 물리는 거리(cm).
	 * 밀착된 기본 카메라(ArmLength 340~380)에서 FOV가 줄어들 때 머리만 확대되는 현상을 막고 역동적인 구도를 만든다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera|Groggy Framing", meta=(ClampMin=0, Units="cm"))
	float KnockdownFramingPullBack = 50.f;

	/** 그로기 슬로우 중 카메라 높이를 살짝 낮춰(음수) 로우 앵글로 캐릭터 상·하체와 보스를 동시에 담는 오프셋(cm). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera|Groggy Framing", meta=(Units="cm"))
	float KnockdownFramingHeightOffset = -18.f;

	/** 그로기 슬로우 로우 앵글에 맞춰 시선을 살짝 위로 들어 올리는 피치 보정(도). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Camera|Groggy Framing", meta=(Units="deg"))
	float KnockdownFramingPitchOffset = 2.2f;

private:
	void SetLockOnBlocked(bool bBlocked) const;

	/** 보고 있는 폰이 바뀌었으면 이전 ASC 구독을 풀고 새 ASC의 전투 이벤트를 듣는다. */
	void RefreshImpactBinding();
	void UnbindImpactEvents();
	void HandleImpactEffectApplied(UAbilitySystemComponent* Target, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle);
	void HandleImpactEvent(const FGameplayEventData* Payload);
	/** 켜짐, 최소 세기, 쿨다운을 보고 재생한다. Direction은 Push 방향. */
	void PlayImpactReaction(FName Moment, const FknightCameraImpactReaction& Reaction, float Strength, const FVector& Direction, bool bIgnoreGates = false);
	FVector GetDirectionFrom(const AActor* Source) const;

	UPROPERTY(Transient)
	TObjectPtr<UknightCameraImpactSettings> LoadedImpactSettings;

	TWeakObjectPtr<UAbilitySystemComponent> ImpactASC;
	FDelegateHandle ImpactEffectHandle;
	TArray<TPair<FGameplayTag, FDelegateHandle>> ImpactEventHandles;
	TMap<FName, double> LastImpactTimes;
	double LastDefenseEventTime = -UE_BIG_NUMBER;

	/** Keeps the preloaded screen effect assets in memory while this camera lives. */
	TSharedPtr<FStreamableHandle> PreloadHandle;

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
