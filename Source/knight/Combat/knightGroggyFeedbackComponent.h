// 그로기 연출 컴포넌트 (2026-10-14)
//
// 플레이어(나이트)가 상대의 그로기(Poise 붕괴)를 일으킨 순간, 일반 타격보다 큰 히트스탑과 이펙트를 낸다.
// 일반 공격·강공격·스킬·퍼펙트 가드(패링) 그로기 모두 상대의 UknightHitReactionComponent가 Event.PoiseBroken을 보내는
// 한 곳을 지나므로(OnAnyPoiseBroken), 공격마다 따로 넣지 않는다. 그로기가 났는지와 반응(다운 모션)은 바꾸지 않는다.
//
// 흐름 (2026-10-14 사용자 결정 '더 역동적으로' = 멈춤 후 쾅 + 슬로모션으로 풀기, 전체 0.4초 → 0.5초):
//   1. 멈춤 FreezeTime(0.2초): 플레이어·상대가 거의 정지(히트스탑 컴포넌트). 타격 지점에 GroggyCueTag(번개).
//   2. 풀리는 순간: ReleaseCueTag(충격파·불꽃·강한 흔들림) = '쾅'.
//   3. 슬로모션 SlowReleaseTime(0.3초): SlowReleaseStartSpeed(0.2배속)에서 점점 빨라져 정상 속도로(처음 천천히, 끝에 빨리).
//   전체 = 0.5초. 플레이어와 상대만 느려지고 월드(다른 몬스터·UI)는 그대로다.
//
// 실제 공격 몽타주의 판정 직전에 보스 다운이 예상되면 국소 슬로우와 FOV 확대를 선행한다.
// 유효 명중은 ApplyMeleeHit에서 확정하고, 선행 슬로우는 피해·히트스톱 처리 전에 해제한다.
// 조절(BP_Knight → GroggyFeedback): 위 시간·속도·큐 태그와 Prediction 값.
// 이펙트·소리·흔들림은 큐 에셋 GCN_Hit_Groggy(타격 순간), GCN_Hit_GroggyRelease(풀리는 순간)에서 바꾼다(/Game/Knight/Abilities/Cues).
// 확인: 출력 로그 LogknightCombat의 "groggy feedback" 줄.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/knightCombatTypes.h"
#include "Containers/Ticker.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "knightGroggyFeedbackComponent.generated.h"

class AknightCharacter;
class AknightMonsterCharacterBase;
class AknightPlayerCameraManager;
class UGameplayAbility;
class UAnimMontage;

UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightGroggyFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UknightGroggyFeedbackComponent();

	/** Watch an already playing attack montage; the trace notify's lead-in gates prediction. */
	void WatchAttack(UGameplayAbility* Attack, UAnimMontage* Montage, const FknightMeleeHitParams& HitParams);
	void EndPredictionForAttack(const UGameplayAbility* Attack);
	/** Resolve before the damage effect or any hit stop can snapshot our local time dilation. */
	void ResolvePredictedMeleeContact(AActor* HitTarget, bool bAccepted, const UGameplayAbility* SourceAbility);
	/** Reports the existing, post-effect Poise/knockdown verdict; never starts a second slow. */
	void CompletePredictedMeleeHit(AActor* HitTarget, bool bNewKnockdown);
	void CancelKnockdownPresentation();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * 거의 멈춰 있는 시간(초). 히트스탑 컴포넌트로 건다(같은 순간의 일반 타격·패링 히트스탑보다 길면 늘어남).
	 * (2026-10-14) 처음엔 0.35초 통째로 멈췄는데, '0.4초로, 더 역동적으로' → 0.15초 멈춤 + 0.25초 슬로모션.
	 * 같은 날 '0.5초로 더 늘리자' → 비율을 유지해 0.2초 멈춤 + 0.3초 슬로모션.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy", meta=(ClampMin=0, Units="s"))
	float FreezeTime = 0.2f;

	/** 멈춤이 풀린 뒤 느린 속도에서 정상 속도로 돌아오는 시간(초). 0이면 바로 정상 속도. FreezeTime + 이 값 = 전체(기본 0.5초). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy", meta=(ClampMin=0, Units="s"))
	float SlowReleaseTime = 0.3f;

	/** 슬로모션이 시작되는 속도(1 = 정상). SlowReleaseTime 동안 점점 빨라져 1이 된다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy", meta=(ClampMin=0.02, ClampMax=1))
	float SlowReleaseStartSpeed = 0.2f;

	/** 타격 순간 상대 쪽에서 실행할 큐(번개). 비우면 없음. 기본 GameplayCue.Hit.Groggy (GCN_Hit_Groggy). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy", meta=(Categories="GameplayCue"))
	FGameplayTag GroggyCueTag;

	/** 멈춤이 풀리는 순간 실행할 큐(충격파·불꽃·강한 흔들림). 비우면 없음. 기본 GameplayCue.Hit.Groggy.Release (GCN_Hit_GroggyRelease). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy", meta=(Categories="GameplayCue"))
	FGameplayTag ReleaseCueTag;

	/** Real-time interval before the next trace notify in which prediction may start. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Prediction", meta=(ClampMin=0, Units="s"))
	float PredictionLeadTime = 0.50f;

	/** Surface distance from the player to a target point along the current attack direction. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Prediction", meta=(ClampMin=0, Units="cm"))
	float PredictionRange = 350.f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Prediction", meta=(ClampMin=1, ClampMax=180, Units="deg"))
	float PredictionAngle = 50.f;

	/** Maximum real-time lead effect; a hit ends it immediately. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Prediction", meta=(ClampMin=0, Units="s"))
	float PredictionMaxTime = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Prediction", meta=(ClampMin=0.02, ClampMax=1))
	float PredictionSlowSpeed = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Prediction", meta=(ClampMin=0, Units="s"))
	float PredictionSlowEnterTime = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Prediction", meta=(ClampMin=0, Units="deg"))
	float PredictionZoomDegrees = 25.f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Prediction", meta=(ClampMin=0, Units="s"))
	float PredictionZoomEnterTime = 0.45f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Prediction", meta=(ClampMin=0, Units="s"))
	float PredictionZoomReturnTime = 0.18f;

private:
	/** 내가 일으킨 그로기면 멈춤과 타격 큐를 내고, FreezeTime 뒤 풀기를 예약한다. 남이 일으켰거나 내가 그로기에 빠진 경우는 무시. */
	void HandleAnyPoiseBroken(AActor* Victim, AActor* Instigator, const FGameplayEffectContextHandle& Context);

	/** 멈춤이 풀리는 순간: 풀림 큐를 내고 슬로모션을 시작한다. */
	void StartRelease();

	/** 슬로모션 한 단계. 다른 히트스탑이 걸려 있는 액터는 그쪽에 맡기고 기다린다. 끝나면 원래 속도로 돌려놓는다. */
	void TickRelease();
	bool TickFeedback(float DeltaRealTime);
	void EnsureFeedbackTicker();
	void TickWatchedAttack();
	bool TryStartPredictedKnockdown(UGameplayAbility* Attack, const FknightMeleeHitParams& HitParams);
	void TickPrediction();
	void FinishPredictionSlow();
	void BeginPredictionReturn();
	void FinishPredictionVisual();

	/** 진행 중인 풀기를 끝내고 이 컴포넌트가 적용한 속도만 돌려놓는다. */
	void FinishRelease();

	UFUNCTION()
	void HandlePlayerDied(AknightCharacter* DeadCharacter);

	UFUNCTION()
	void HandleBossDefeated(AknightMonsterCharacterBase* Boss, AActor* Killer);

	FDelegateHandle PoiseBrokenHandle;
	FTimerHandle ReleaseDelayTimer;
	FTSTicker::FDelegateHandle FeedbackTicker;

	TWeakObjectPtr<AActor> Victim;
	TWeakObjectPtr<AknightMonsterCharacterBase> PredictedBoss;
	TWeakObjectPtr<UGameplayAbility> PredictionAttack;
	TWeakObjectPtr<UGameplayAbility> WatchedAttack;
	TWeakObjectPtr<UAnimMontage> WatchedMontage;
	TSet<TWeakObjectPtr<AActor>> PredictedTargetsThisAttack;
	UPROPERTY(Transient)
	FknightMeleeHitParams WatchedHitParams;
	TWeakObjectPtr<AknightPlayerCameraManager> ZoomCameraManager;
	FGameplayCueParameters CueParams;
	double ReleaseStartTime = 0.0;
	double PredictionStartTime = 0.0;
	double PresentationEndTime = 0.0;
	float OwnerBaseSpeed = 1.f;
	float VictimBaseSpeed = 1.f;
	float OwnerAppliedSpeed = 1.f;
	float VictimAppliedSpeed = 1.f;
	bool bReleasing = false;
	bool bOwnerSpeedApplied = false;
	bool bVictimSpeedApplied = false;
	bool bPredictionActive = false;
	bool bPredictionHitPending = false;
	bool bPredictionWasLocked = false;
	bool bWatchedPredictionAttempted = false;
	bool bSkipGenericReleaseSlow = false;
	bool bCameraBeatActive = false;
};
