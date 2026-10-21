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
class USoundClass;
class USoundMix;

/** 그로기 연출 단계 알림. bImpact = false: 예측 슬로우 시작, true: 그로기 명중(멈춤 시작). */
DECLARE_MULTICAST_DELEGATE_OneParam(FknightGroggyPresentationEvent, bool /*bImpact*/);

UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightGroggyFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UknightGroggyFeedbackComponent();

	/**
	 * Watch an already playing attack montage; the trace notify's lead-in gates prediction.
	 * (2026-10-20) AlreadyHitTargets: 같은 어빌리티가 앞 몽타주에서 이미 맞혀 이번 몽타주에서는 맞지 않을 대상(점프 공격 1타 뒤 착지).
	 * 이 대상들은 예측하지 않는다. 전에는 착지 때 슬로우만 걸리고 타격은 없었다.
	 */
	void WatchAttack(UGameplayAbility* Attack, UAnimMontage* Montage, const FknightMeleeHitParams& HitParams,
		const TSet<TWeakObjectPtr<AActor>>* AlreadyHitTargets = nullptr);

	/**
	 * (2026-10-20) 락온 돌진 베기: 돌진(판정 없음) 중에도 예측 슬로를 시작할 수 있게 미리 알린다.
	 * 도착 자리(Destination)와 돌진 길이, 베기 모션·배속으로 맞기까지 남은 시간을 재고, LungePredictionLeadTime 안에 들면 시작한다.
	 * 베기가 시작되면(WatchAttack) 같은 슬로를 이어받아 베기 판정 직전에 채찍으로 푼다.
	 */
	void WatchLungeDash(UGameplayAbility* Attack, UAnimMontage* StrikeMontage, float StrikePlayRate, const FknightMeleeHitParams& HitParams,
		const FVector& Destination, float DashDuration);
	void EndPredictionForAttack(const UGameplayAbility* Attack);
	/** Resolve before the damage effect or any hit stop can snapshot our local time dilation. */
	void ResolvePredictedMeleeContact(AActor* HitTarget, bool bAccepted, const UGameplayAbility* SourceAbility);
	/** Reports the existing, post-effect Poise/knockdown verdict; never starts a second slow. */
	void CompletePredictedMeleeHit(AActor* HitTarget, bool bNewKnockdown);
	void CancelKnockdownPresentation();

	/** 예측 슬로우·줌, 멈춤, 슬로모션 풀기 중 하나라도 진행 중이면 true. HUD 연출이 끝나는 시점을 여기서 판단한다. */
	bool IsPresentationActive() const;

	/** 연출이 시작될 때(예측 슬로우, 그로기 명중) 알린다. 끝은 IsPresentationActive로 확인한다. */
	FknightGroggyPresentationEvent OnPresentationEvent;

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

	/**
	 * 첫 판정 몇 초 전(정상 속도 기준)부터 예측 슬로를 시작할지. 슬로로 늘어난 실제 길이 ≈ 이 값 x 0.8 / PredictionSlowSpeed + 채찍.
	 * (2026-10-20) 예전에는 코드가 이 값을 무시하고 1.2초로 시작해 슬로가 안전 제한(1.5초)에 걸려 맞기 전에 풀렸다. 0.5 → 0.35(실제 약 0.8초).
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Prediction", meta=(ClampMin=0.1, Units="s"))
	float PredictionLeadTime = 0.35f;

	/** (2026-10-20) 락온 돌진 베기는 돌진까지 슬로에 넣으려고 더 일찍(맞기 이 초 전, 정상 속도 기준) 시작한다. bLungeSlowFromDashStart면 무시. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Prediction", meta=(ClampMin=0.1, Units="s"))
	float LungePredictionLeadTime = 0.55f;

	/** (2026-10-20 사용자 요청) 락온 돌진 베기는 돌진을 시작하는 순간부터 슬로. 끄면 LungePredictionLeadTime 기준. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Prediction")
	bool bLungeSlowFromDashStart = true;

	/**
	 * (2026-10-20) 돌진 베기 슬로 배속. 돌진 전체가 슬로라 일반 예측 슬로(PredictionSlowSpeed)보다 덜 느리게 해서 너무 길어지지 않게 한다.
	 * 베기가 이어받을 때도 이 배속을 유지하다가 채찍으로 푼다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Prediction", meta=(ClampMin=0.02, ClampMax=1))
	float LungeSlowSpeed = 0.5f;

	/** Surface distance from the player to a target point along the current attack direction. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Prediction", meta=(ClampMin=0, Units="cm"))
	float PredictionRange = 350.f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Prediction", meta=(ClampMin=1, ClampMax=180, Units="deg"))
	float PredictionAngle = 50.f;

	/**
	 * Maximum real-time lead effect; a hit ends it immediately.
	 * (2026-10-19 사용자 '그로기 슬로우 시간이 너무 길다') 1.5 → 0.6초. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Prediction", meta=(ClampMin=0, Units="s"))
	float PredictionMaxTime = 0.6f;

	/** 예측 슬로우 배속. (2026-10-19 같은 이유로 0.25 → 0.4, 덜 느리게) 바뀔 수 있는 값. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Prediction", meta=(ClampMin=0.02, ClampMax=1))
	float PredictionSlowSpeed = 0.4f;

	/** (2026-10-20) 슬로로 들어가는 시간(실제 초). 예전에는 코드에 0.04초로 고정돼 있어 뚝 끊기듯 느려졌다. 감속 곡선(처음 빠르고 끝에 부드럽게). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Prediction", meta=(ClampMin=0, Units="s"))
	float PredictionSlowEnterTime = 0.1f;

	/**
	 * (2026-10-20) 슬로를 언제부터 풀지. 공격 모션의 첫 판정까지 진행률(0~1). 이 지점까지 슬로를 유지하다가
	 * 칼이 닿기 직전에 확 빨라져 정상 속도로 맞힌다(채찍). 예전에는 0.5부터 천천히 풀려서 맞기 전에 이미 정상 속도였다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Prediction", meta=(ClampMin=0, ClampMax=0.98))
	float PredictionWhipStartProgress = 0.8f;

	/** (2026-10-20) 예측 슬로로 들어온 그로기 타격의 멈춤 시간(초). 예전에는 0.08초로 고정돼 한 방이 작았다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Prediction", meta=(ClampMin=0, Units="s"))
	float PredictedImpactFreezeTime = 0.14f;

	/** (2026-10-20) 맞는 순간 줌을 한 번 더 당기는 각도(도)와 시간. 0이면 없음. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Prediction", meta=(ClampMin=0, Units="deg"))
	float ImpactZoomPunchDegrees = 6.f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Prediction", meta=(ClampMin=0, Units="s"))
	float ImpactZoomPunchTime = 0.05f;

	/**
	 * (2026-10-20) 그로기 후 여운: 멈춤이 풀린 뒤 보스만 이 시간 동안 느리게 쓰러지다 정상 속도로. 나이트는 바로 정상 속도(조작감 유지).
	 * 0이면 없음(예전과 같음).
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Prediction", meta=(ClampMin=0, Units="s"))
	float VictimFollowThroughTime = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Prediction", meta=(ClampMin=0.02, ClampMax=1))
	float VictimFollowThroughStartSpeed = 0.35f;

	/**
	 * (2026-10-20) 예측 슬로 동안 소리를 먹먹하게(로패스 + 조금 작게) 할 사운드 클래스. 맞는 순간 바로 돌아와 타격음이 더 크게 들린다.
	 * 비우면 소리 연출 없음. 예: SC_Knight_SFX, SC_Knight_BGM.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Audio")
	TArray<TObjectPtr<USoundClass>> PredictionMuffledClasses;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Audio", meta=(ClampMin=100, ClampMax=20000, Units="Hz"))
	float PredictionLowPassFrequency = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Groggy|Audio", meta=(ClampMin=0, ClampMax=1))
	float PredictionVolumeScale = 0.7f;

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
	/** LeadRemaining >= 0: 돌진 중(모션 판정 없음) 시작. 맞기까지 남은 시간과 도착 자리를 호출하는 쪽이 준다. */
	bool TryStartPredictedKnockdown(UGameplayAbility* Attack, const FknightMeleeHitParams& HitParams,
		float LeadRemaining = -1.f, const FVector* LeadDestination = nullptr);
	void TickLungeWatch(float DeltaRealTime);
	void TickPrediction();
	void FinishPredictionSlow();
	void BeginPredictionReturn();
	void FinishPredictionVisual();

	/** 진행 중인 풀기를 끝내고 이 컴포넌트가 적용한 속도만 돌려놓는다. */
	void FinishRelease();

	/** (2026-10-20) 예측 슬로 중 소리 먹먹하게 켜기/끄기. */
	void SetPredictionAudio(bool bMuffled);

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> PredictionSoundMix;
	bool bPredictionAudioPushed = false;

	/** 이번 풀기가 보스만(예측 그로기 여운)인지, 그때의 길이·시작 속도. */
	bool bReleaseVictimOnly = false;
	float ActiveReleaseTime = 0.f;
	float ActiveReleaseStartSpeed = 1.f;

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
	/** 예측 슬로가 시작된 모션 위치와 그때 잰 다음 판정 시각(모션 초). 슬로 곡선의 진행률 기준. */
	float PredictionStartPosition = 0.f;
	float PredictionHitTime = MAX_FLT;

	/** (2026-10-20) 돌진 베기 미리 보기: 돌진 중 예측 시작 조건을 매 틱 본다. */
	TWeakObjectPtr<UGameplayAbility> LungeAttack;
	UPROPERTY(Transient)
	FknightMeleeHitParams LungeHitParams;
	FVector LungeDestination = FVector::ZeroVector;
	float LungeDashRemaining = 0.f;
	float LungeStrikeLead = 0.f;
	bool bLungeWatch = false;
	/** 예측 슬로가 돌진 중에 시작돼 아직 베기 모션이 없는 상태(슬로 유지만). */
	bool bPredictionLead = false;
	/** 이번 예측 슬로의 배속과 안전 제한(실제 초). 시작할 때 정한다. */
	float ActivePredictionSlowSpeed = 0.4f;
	double ActivePredictionMaxDuration = 1.5;
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
