#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "Combat/knightCombatTypes.h"
#include "Combat/knightGhostTrailActor.h"
#include "Input/knightMovePressTracker.h"
#include "knightGameplayAbility_HeavyAttack.generated.h"

class UAnimMontage;
class UAudioComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;

/**
 * Secondary attack key (mouse thumb button): tap for the heavy combo, hold to charge.
 *   released before HoldThreshold  -> activates the heavy combo step whose tags fit (HeavyAbilityTag, e.g.
 *                                     Ability.Attack.Heavy on GA_Knight_Heavy_1..4). If no step can start yet (an
 *                                     attack is still swinging), the tap is retried until TapBufferTime runs out,
 *                                     like the input buffer does for the light combo.
 *   held past HoldThreshold        -> charge: ChargeStartMontage, then ChargeLoopMontage repeats while held
 *                                     (its melee trace hits with LoopHitParams). Releasing plays ChargeReleaseMontage,
 *                                     whose hits use ReleaseHitParams scaled up to MaxChargeMultiplier after MaxChargeTime.
 * bChargeOnPress를 켜면 누르는 순간 차지를 시작하고, 짧게 떼도 강공격 콤보 대신 모은 만큼의 해방 공격이 나간다(아래 칸 설명).
 * Charging starts only when no other attack / guard / dash is running, or inside Window.Cancel (it cancels them).
 * 그때 바로 시작할 수 없으면 누르고 있는 동안 ChargeWaitTime까지 다시 본다(후딜에 누른 차지가 버려지지 않게).
 * While charging the owner has State.Charging and other attacks are blocked.
 *
 * 해방부터는 일반 공격과 같은 캔슬 규칙: 해방할 때 교체 가능 배타 그룹으로 바뀌어(ChangeActivationGroupWhileActive)
 *   캔슬 창(Window.Cancel) 전에는 가드·퀵스텝·점프가 안 되고, 캔슬 창 처음 AttackRecoveryDashOnlyTime(0.25초)은 퀵스텝과 다음 공격만,
 *   그 뒤에는 가드·이동·점프도 된다. 모으는 동안은 지금처럼 독립 그룹이라 가드·퀵스텝으로 그만둘 수 있다.
 *   행동 에너지: 모으는 동안 ChargeStaminaCost를 조금씩, 해방할 때 ReleaseStaminaCost. 계수·그로기는 FullCharge* 값까지 오른다.
 * 차지해서 치는 느낌: 차지량에 따라 해방 모션 배속 1 → FullChargeReleaseRate(1.2), 락온 중 돌진 거리
 *   MaxWarpDistance(4m) → LockOnFullChargeWarpDistance(7m → 10m). 돌진하는 동안 대상을 따라가고(bTrackTargetDuringLunge) 잔상을 남긴다(bReleaseGhostTrail).
 *   잔상은 해방 시작부터 휘두르기 끝까지 남긴다(bReleaseGhostTrailWholeSwing).
 *
 * Bind to InputTag.Attack.Secondary. The heavy combo steps are ordinary knightGameplayAbility_MeleeAttack Blueprints
 * with no input tag; chain them with Combo tags exactly like the light combo.
 *
 * 행동 속도(ActionSpeed): 차지 몽타주 3개와 발사 돌진은 행동 속도 배속을 따른다(반복 몽타주가 빨라지면 차지 중 타격도 잦아진다).
 * HoldThreshold·MaxChargeTime·TapBufferTime은 버튼을 누르는 시간 기준이라 그대로 둔다.
 * 단 풀차지 시간은 스탠스 공격 속도로 나눈다(bScaleChargeTimeByStance). 행동 속도는 위처럼 따르지 않는다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_HeavyAttack : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_HeavyAttack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Holding longer than this (seconds) charges instead of a heavy combo step. */
	UPROPERTY(EditDefaultsOnly, Category="Heavy", meta=(ClampMin=0.05, Units="s"))
	float HoldThreshold = 0.3f;

	/**
	 * 켜면 누르는 순간 차지를 시작한다(HoldThreshold를 기다리지 않는다). 짧게 눌렀다 떼도 강공격 콤보 대신
	 * 모은 만큼(거의 0)의 가장 약한 해방 공격이 나간다. 앞 공격이 캔슬 창 전이라 바로 모으지 못한 채 떼면
	 * TapBufferTime 동안 다시 보고, 모을 수 있게 되는 순간 모으기 없이 바로 해방한다(끝내 안 되면 아무것도 안 나감).
	 * 끄면 예전처럼 짧게 누르면 강공격 콤보(HeavyAbilityTag), 길게 누르면 차지. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Heavy")
	bool bChargeOnPress = false;

	/** Tag of the heavy combo step abilities. */
	UPROPERTY(EditDefaultsOnly, Category="Heavy", meta=(Categories="Ability"))
	FGameplayTag HeavyAbilityTag;

	/** How long a tap keeps trying to start the next heavy step while the current attack is still swinging. */
	UPROPERTY(EditDefaultsOnly, Category="Heavy", meta=(ClampMin=0, Units="s"))
	float TapBufferTime = 0.35f;

	/**
	 * HoldThreshold가 지났는데 아직 차지를 시작할 수 없으면(앞 공격이 캔슬 창 전이거나 가드·퀵스텝 중)
	 * 누르고 있는 동안 0.05초마다 다시 보고, 되는 순간 차지를 시작한다. 이 시간이 지나도 안 되면 그만둔다.
	 * 기다리는 동안 키를 떼면 아무것도 하지 않는다(탭 강공격도 나가지 않는다). 0이면 예전처럼 한 번만 본다. 바뀔 수 있는 값.
	 * 1초 → 2초: 4타는 캔슬 창이 늦게 열려 한손 4타 중에 누른 차지가 1.02초 기다리고 0.1~0.3초 차이로 놓쳤다.
	 * 양손 4타는 더 느려서 시작하자마자 누르면 1.6~1.7초쯤 기다려야 한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Heavy", meta=(ClampMin=0, Units="s"))
	float ChargeWaitTime = 2.f;

	UPROPERTY(EditDefaultsOnly, Category="Charge")
	TObjectPtr<UAnimMontage> ChargeStartMontage;

	/** Repeated while the key is held. */
	UPROPERTY(EditDefaultsOnly, Category="Charge")
	TObjectPtr<UAnimMontage> ChargeLoopMontage;

	UPROPERTY(EditDefaultsOnly, Category="Charge")
	TObjectPtr<UAnimMontage> ChargeReleaseMontage;

	/** Hits during the start / loop montages. */
	UPROPERTY(EditDefaultsOnly, Category="Charge")
	FknightMeleeHitParams LoopHitParams;

	/** Hits of the release montage at no charge; motion value and poise damage scale up with the charge time. */
	UPROPERTY(EditDefaultsOnly, Category="Charge")
	FknightMeleeHitParams ReleaseHitParams;

	/** Charge time (seconds, counted from the start of the charge) that reaches MaxChargeMultiplier. */
	UPROPERTY(EditDefaultsOnly, Category="Charge", meta=(ClampMin=0.1, Units="s"))
	float MaxChargeTime = 2.f;

	/**
	 * 켜면 풀차지까지 걸리는 시간 = MaxChargeTime ÷ 스탠스 공격 속도(ASC 스탠스 보정 AttackSpeedScale).
	 * 한손(1.1) 약 1.82초, 양손(0.92) 약 2.17초. 차지는 버튼 시간이라 한손이 빨라도 이득이 없고 양손 배율만 남던 것을 맞춘다.
	 * 행동 속도(ActionSpeed)·공격 배속(AttackAnimationSpeed)은 여전히 따르지 않는다. 끄면 두 스탠스 모두 MaxChargeTime. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Charge")
	bool bScaleChargeTimeByStance = true;

	UPROPERTY(EditDefaultsOnly, Category="Charge", meta=(ClampMin=1))
	float MaxChargeMultiplier = 2.f;

	/**
	 * 풀차지(MaxChargeTime) 때의 계수·그로기. 0보다 크면 MaxChargeMultiplier 대신
	 * ReleaseHitParams 값(기본 55/25)에서 이 값까지 차지 시간에 따라 오른다(풀차지 120/60). 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Charge", meta=(ClampMin=0))
	float FullChargeMotionValue = 120.f;

	UPROPERTY(EditDefaultsOnly, Category="Charge", meta=(ClampMin=0))
	float FullChargePoiseDamage = 60.f;

	/**
	 * (2026-11-03 팀 피드백 '공격 무게에 따라 역경직 차등') 풀차지 때의 역경직(초). 0보다 크면 ReleaseHitParams.HitStopDuration
	 * (차지 없음, 기본 0.14)에서 이 값까지 차지 시간에 따라 오른다. 전에는 차지량과 상관없이 0.2초라 풀차지가 강공격 4타(0.22)보다
	 * 짧았다. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Charge", meta=(ClampMin=0, Units="s"))
	float FullChargeHitStopDuration = 0.26f;

	/**
	 * (2026-11-03) 풀차지 해방의 카메라 킥 세기. 차지 없음(CameraKickScale)에서 이 값까지 차지량에 따라 오른다.
	 * 0이면 차지량과 상관없이 CameraKickScale. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Charge", meta=(ClampMin=0, ClampMax=3))
	float FullChargeCameraKickScale = 1.6f;

	virtual float GetCameraKickScale() const override;

	/**
	 * 차지 행동 에너지. 모으는 동안 MaxChargeTime에 걸쳐 ChargeStaminaCost만큼 조금씩 쓰고(풀차지면 25),
	 * 해방(돌진)할 때 ReleaseStaminaCost를 더 쓴다(풀차지 합 40). 0보다 많아야 모으기 시작하고, 시작한 뒤에는 모자라도
	 * 끝까지 쓴다(가불). 이 어빌리티의 StaminaCost(시작할 때 한 번)는 쓰지 않는다(0). 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Cost", meta=(ClampMin=0))
	float ChargeStaminaCost = 25.f;

	UPROPERTY(EditDefaultsOnly, Category="Charge|Cost", meta=(ClampMin=0))
	float ReleaseStaminaCost = 15.f;

	/** 해방 후딜을 이동으로 끊을 때 해방 모션이 빠지는 시간(일반 공격의 RecoveryCancelBlendOut과 같은 값). */
	UPROPERTY(EditDefaultsOnly, Category="Charge", meta=(ClampMin=0, Units="s"))
	float ReleaseMoveCancelBlendOut = 0.25f;

	/** Warp target of the release montage, set when the button is released. */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Motion Warping")
	FName WarpTargetName = TEXT("AttackTarget");

	UPROPERTY(EditDefaultsOnly, Category="Charge|Motion Warping", meta=(ClampMin=0, Units="cm"))
	float MaxWarpDistance = 400.f;

	/** The warp stops this far in front of the target's surface (or part point), so the blade lands instead of the body. */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Motion Warping", meta=(ClampMin=0, Units="cm"))
	float WarpStopDistance = 150.f;

	/** Without a lock, steer into the closest enemy in the attack direction. */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Motion Warping")
	bool bUseSoftTarget = true;

	UPROPERTY(EditDefaultsOnly, Category="Charge|Motion Warping", meta=(ClampMin=0, Units="cm", EditCondition="bUseSoftTarget"))
	float SoftTargetRange = 500.f;

	UPROPERTY(EditDefaultsOnly, Category="Charge|Motion Warping", meta=(ClampMin=1, ClampMax=180, Units="deg", EditCondition="bUseSoftTarget"))
	float SoftTargetAngle = 75.f;

	/**
	 * 락온 중 해방 돌진 최대 거리. 차지량에 따라 MaxWarpDistance(4m)에서 이 값까지 늘어난다(풀차지 7m).
	 * 락온 없이 유도로 잡은 대상은 MaxWarpDistance 그대로. 바뀔 수 있는 값.
	 * 7m → 10m. 돌진은 해방 몽타주의 워프 구간(휘두르기 직전) 안에 끝나야 해서,
	 * 양손 풀차지 약 0.43초·한손 약 0.36초 안에 10m를 간다(7m일 때보다 1.5배쯤 빠름).
	 */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Motion Warping", meta=(ClampMin=0, Units="cm"))
	float LockOnFullChargeWarpDistance = 1000.f;

	/**
	 * 해방 돌진 속도. 돌진 시간 = 거리 / 이 값(최소 0.25초, 휘두르기 직전까지만).
	 * 1600 = 4m를 0.25초에 가던 이전 속도. 7m 돌진이 순간 이동처럼 보이지 않게 거리만큼 시간을 늘린다. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Motion Warping", meta=(ClampMin=100, Units="cm/s"))
	float ReleaseLungeSpeed = 1600.f;

	/**
	 * 약간의 유도: 해방 돌진 중 대상의 지금 위치를 따라간다(휘두르기 직전까지, 돌진 최대 거리 안).
	 * 패링 반격 돌진(MeleeAttack bWarpToParriedAttacker)과 같은 방식.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Motion Warping")
	bool bTrackTargetDuringLunge = true;

	/**
	 * 풀차지 해방 모션 배속. 차지량에 따라 1에서 이 값까지 오른다(행동 속도에 곱함).
	 * 해방 모션·돌진·캔슬 창이 모두 그만큼 빨라진다. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Release", meta=(ClampMin=0.5, ClampMax=2))
	float FullChargeReleaseRate = 1.2f;

	/** 해방 돌진하는 동안 락온 돌진과 같은 잔상을 남긴다. 대상 1m(InRangeDistance) 안이면 남기지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Ghost Trail")
	bool bReleaseGhostTrail = true;

	/**
	 * 공격 전체에 잔상: 켜면 돌진할 때만이 아니라 해방 시작부터 휘두르기 판정이 끝날 때까지 잔상을 남긴다.
	 * 락온·돌진이 없어도, 대상 가까이 가도 끊지 않는다. 끄면 위 설명대로 돌진하는 동안만. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Ghost Trail", meta=(EditCondition="bReleaseGhostTrail"))
	bool bReleaseGhostTrailWholeSwing = true;

	UPROPERTY(EditDefaultsOnly, Category="Charge|Ghost Trail", meta=(EditCondition="bReleaseGhostTrail"))
	FknightGhostTrailSettings ReleaseGhostTrailSettings;

	/** 차지 중 기류·검신 발광·풀차지 섬광 및 해방 시 지면 균열 이펙트를 켤지 여부. */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Effects")
	bool bPlayChargeEffects = true;

	/** 차지 홀드 중 발밑에서 소용돌이치는 기류 이펙트 (비우면 NS_Monster_WindStreak). */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Effects", meta=(EditCondition="bPlayChargeEffects"))
	TObjectPtr<UNiagaraSystem> ChargeAuraEffect;

	/** 한손 스탠스 차지 중 검신에 감기는 푸른 에너지 이펙트 (비우면 NS_Sword_Scifi). */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Effects", meta=(EditCondition="bPlayChargeEffects"))
	TObjectPtr<UNiagaraSystem> OneHandedChargeSwordEffect;

	/** 양손 스탠스 차지 중 검신에 타오르는 화염 이펙트 (비우면 NS_Sword_Fire). */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Effects", meta=(EditCondition="bPlayChargeEffects"))
	TObjectPtr<UNiagaraSystem> TwoHandedChargeSwordEffect;

	/** 최대 차지(MaxChargeTime) 도달 순간 검에서 터지는 풀차지 완료 스파크 (비우면 NS_Knight_ParrySpark). */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Effects", meta=(EditCondition="bPlayChargeEffects"))
	TObjectPtr<UNiagaraSystem> FullChargeBurstEffect;

	/**
	 * 최대 차지 순간 검 주변이 일렁이는 굴절 이펙트 (비우면 NS_Hit_Distortion_Once).
	 * 전에는 칸 없이 코드에서 경로로 불렀다. 칸에 넣어 두면 맵과 함께 미리 읽히고 패키징에도 들어간다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Effects", meta=(EditCondition="bPlayChargeEffects"))
	TObjectPtr<UNiagaraSystem> FullChargeDistortionEffect;

	/** 차지 해방 타격이 지면을 때릴 때 생성되는 바닥 균열 이펙트 (비우면 NS_Hit_GroundCrack). */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Effects", meta=(EditCondition="bPlayChargeEffects"))
	TObjectPtr<UNiagaraSystem> ReleaseGroundCrackEffect;

	/** 풀차지 또는 양손 차지 해방 시 추가로 터지는 대지 균열/파열 이펙트 (비우면 NS_Hit_GroundRift). */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Effects", meta=(EditCondition="bPlayChargeEffects"))
	TObjectPtr<UNiagaraSystem> ReleaseGroundRiftEffect;

	/** 차지 해방 타격 시 지면에서 피어오르는 흙먼지 이펙트 (비우면 NS_Monster_GroundDust). */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Effects", meta=(EditCondition="bPlayChargeEffects"))
	TObjectPtr<UNiagaraSystem> ReleaseGroundDustEffect;

	/**
	 * 차지 해방 몽타주의 MeleeTrace 판정 구간(시작~끝) 중 지면 균열 이펙트가 터질 진행 비율(0 = 휘두르기 시작, 0.8 = 베기 끝 무렵 지면 타격 순간, 1 = 판정 종료).
	 * 몽타주 재생 위치(Montage_GetPosition)를 직접 추적하므로 기갑 타이밍(Lock 멈춤)·히트스탑·슬로모션과 100% 동기화된다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Effects", meta=(ClampMin=0.0, ClampMax=1.0, EditCondition="bPlayChargeEffects"))
	float ReleaseGroundCrackSwingProgress = 0.8f;

	/** 몽타주에 MeleeTrace 노티파이가 없을 때 사용할 폴백 몽타주 위치(초). */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Effects", meta=(ClampMin=0.02, Units="s", EditCondition="bPlayChargeEffects"))
	float ReleaseGroundCrackDelay = 0.60f;

	/**
	 * (2026-10-28) 차지 유지 루프 사운드. 차지 시작 후 ChargeLoopSoundDelay 뒤부터 페이드 인으로 깔리고, 해방·취소하면 페이드 아웃.
	 * 차지 시작 소리는 AM_Knight_Charge_Start 몽타주 노티파이가 낸다(터빈 가속음이 끝날 무렵 루프가 이어 받는다). 비우면 없다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Sound")
	TObjectPtr<USoundBase> ChargeLoopSound;

	UPROPERTY(EditDefaultsOnly, Category="Charge|Sound", meta=(ClampMin=0, Units="s"))
	float ChargeLoopSoundDelay = 0.7f;

	UPROPERTY(EditDefaultsOnly, Category="Charge|Sound", meta=(ClampMin=0, Units="s"))
	float ChargeLoopFadeIn = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category="Charge|Sound", meta=(ClampMin=0, Units="s"))
	float ChargeLoopFadeOut = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category="Charge|Sound", meta=(ClampMin=0))
	float ChargeLoopVolume = 0.8f;

	/** (2026-10-28) 최대 차지(MaxChargeTime) 도달 순간 사운드. 비우면 없다. */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Sound")
	TObjectPtr<USoundBase> ChargeFullSound;

private:
	UFUNCTION()
	void OnReleased(float TimeHeld);

	UFUNCTION()
	void OnHoldThreshold();

	UFUNCTION()
	void OnChargeStartFinished();

	UFUNCTION()
	void OnChargeLoopFinished();

	UFUNCTION()
	void OnReleaseFinished();

	UFUNCTION()
	void OnMeleeHitEvent(FGameplayEventData Payload);

	void TryHeavyStep();
	/** 누르고 있는 동안 차지를 시작할 수 있는지 다시 본다(ChargeWaitTime까지). */
	void TryStartChargeWhileHeld();
	bool CanStartCharge() const;
	/** 이번 차지의 풀차지 시간(MaxChargeTime, 켜져 있으면 스탠스 공격 속도로 나눔). 모으는 동안은 스탠스를 바꿀 수 없다(State.Charging). */
	float GetStanceMaxChargeTime() const;
	void StartCharge();
	void StartChargeEffects();
	void StopChargeEffects();
	/** (2026-10-28) 차지 유지 루프 사운드. StopChargeEffects가 함께 끈다. */
	void StartChargeLoopSound();
	void StopChargeLoopSound();
	void OnFullChargeReached();
	void CheckReleaseGroundCrackTiming();
	void SpawnReleaseGroundCrack();
	void PlayLoop();
	void PlayMontage(UAnimMontage* Montage, FName TaskName, bool bLoopOnEnd);
	void Finish(bool bWasCancelled);
	/** 모으는 동안 행동 에너지를 조금씩 쓴다(ChargeStaminaCost를 MaxChargeTime에 걸쳐). */
	void TickChargeStamina();
	/** 해방 후딜: 캔슬 창이 열리고 후딜이 지난 뒤 새로 누른 이동 입력이면 해방 모션을 끊는다(일반 공격과 같은 규칙). */
	void CheckReleaseMoveCancel();

	// WaitingCharge: HoldThreshold는 지났는데 아직 차지를 시작할 수 없어 기다리는 중.
	enum class EPhase : uint8 { Deciding, TapBuffer, WaitingCharge, Charging, Releasing };
	EPhase Phase = EPhase::Deciding;
	FknightMeleeHitParams ActiveHitParams;
	double ChargeStartTime = 0.0;
	double TapDeadline = 0.0;
	double ChargeWaitStartTime = 0.0;
	FTimerHandle ChargeWaitTimer;
	float LastReleaseChargeAlpha = 0.f;
	/** 이번 해방 모션 배속(차지량에 따라 1 ~ FullChargeReleaseRate). 행동 속도에 곱한다. */
	float ReleaseRateScale = 1.f;
	float ReleaseCrackTargetPosition = 0.60f;
	bool bReleased = false;
	bool bChargeStateApplied = false;
	FTimerHandle TapTimer;
	FTimerHandle FullChargeTimer;
	FTimerHandle ChargeLoopSoundTimer;
	FTimerHandle GroundCrackTimer;

	// 차지 행동 에너지·해방 후딜.
	FTimerHandle ChargeStaminaTimer;
	FTimerHandle ReleaseMoveCancelTimer;
	/** 해방 돌진 잔상을 돌진이 시작될 때 켜는 타이머. */
	FTimerHandle ReleaseGhostTimer;
	double LastChargeStaminaTime = 0.0;
	float ChargeStaminaSpent = 0.f;
	/** 모으는 동안 다른 공격을 막아 둔 상태(해방하면 풀고 일반 공격 규칙에 맡긴다). */
	bool bAttackBlockApplied = false;
	FknightMovePressTracker ReleaseMovePress;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActiveChargeAuraFX;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActiveChargeSwordFX;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ActiveChargeLoopAudio;
};
