#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "Combat/knightCombatTypes.h"
#include "Input/knightMovePressTracker.h"
#include "knightGameplayAbility_HeavyAttack.generated.h"

class UAnimMontage;
class UNiagaraComponent;
class UNiagaraSystem;

/**
 * Secondary attack key (mouse thumb button): tap for the heavy combo, hold to charge.
 *   released before HoldThreshold  -> activates the heavy combo step whose tags fit (HeavyAbilityTag, e.g.
 *                                     Ability.Attack.Heavy on GA_Knight_Heavy_1..4). If no step can start yet (an
 *                                     attack is still swinging), the tap is retried until TapBufferTime runs out,
 *                                     like the input buffer does for the light combo.
 *   held past HoldThreshold        -> charge: ChargeStartMontage, then ChargeLoopMontage repeats while held
 *                                     (its melee trace hits with LoopHitParams). Releasing plays ChargeReleaseMontage,
 *                                     whose hits use ReleaseHitParams scaled up to MaxChargeMultiplier after MaxChargeTime.
 * Charging starts only when no other attack / guard / dash is running, or inside Window.Cancel (it cancels them).
 * While charging the owner has State.Charging and other attacks are blocked.
 *
 * (2026-10-26 사용자 결정) 해방부터는 일반 공격과 같은 캔슬 규칙: 해방할 때 교체 가능 배타 그룹으로 바뀌어(ChangeActivationGroupWhileActive)
 *   캔슬 창(Window.Cancel) 전에는 가드·퀵스텝·점프가 안 되고, 캔슬 창 처음 AttackRecoveryDashOnlyTime(0.25초)은 퀵스텝과 다음 공격만,
 *   그 뒤에는 가드·이동·점프도 된다. 모으는 동안은 지금처럼 독립 그룹이라 가드·퀵스텝으로 그만둘 수 있다.
 *   행동 에너지: 모으는 동안 ChargeStaminaCost를 조금씩, 해방할 때 ReleaseStaminaCost. 계수·그로기는 FullCharge* 값까지 오른다.
 *
 * Bind to InputTag.Attack.Secondary. The heavy combo steps are ordinary knightGameplayAbility_MeleeAttack Blueprints
 * with no input tag; chain them with Combo tags exactly like the light combo.
 *
 * (2026-10-08) 행동 속도(ActionSpeed): 차지 몽타주 3개와 발사 돌진은 행동 속도 배속을 따른다(반복 몽타주가 빨라지면 차지 중 타격도 잦아진다).
 * HoldThreshold·MaxChargeTime·TapBufferTime은 버튼을 누르는 시간 기준이라 그대로 둔다.
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

	/** Tag of the heavy combo step abilities. */
	UPROPERTY(EditDefaultsOnly, Category="Heavy", meta=(Categories="Ability"))
	FGameplayTag HeavyAbilityTag;

	/** How long a tap keeps trying to start the next heavy step while the current attack is still swinging. */
	UPROPERTY(EditDefaultsOnly, Category="Heavy", meta=(ClampMin=0, Units="s"))
	float TapBufferTime = 0.35f;

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

	UPROPERTY(EditDefaultsOnly, Category="Charge", meta=(ClampMin=1))
	float MaxChargeMultiplier = 2.f;

	/**
	 * (2026-10-26 사용자 결정) 풀차지(MaxChargeTime) 때의 계수·그로기. 0보다 크면 MaxChargeMultiplier 대신
	 * ReleaseHitParams 값(기본 55/25)에서 이 값까지 차지 시간에 따라 오른다(풀차지 120/60). 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Charge", meta=(ClampMin=0))
	float FullChargeMotionValue = 120.f;

	UPROPERTY(EditDefaultsOnly, Category="Charge", meta=(ClampMin=0))
	float FullChargePoiseDamage = 60.f;

	/**
	 * (2026-10-26 사용자 결정) 차지 행동 에너지. 모으는 동안 MaxChargeTime에 걸쳐 ChargeStaminaCost만큼 조금씩 쓰고(풀차지면 25),
	 * 해방(돌진)할 때 ReleaseStaminaCost를 더 쓴다(풀차지 합 40). 0보다 많아야 모으기 시작하고, 시작한 뒤에는 모자라도
	 * 끝까지 쓴다(가불). 이 어빌리티의 StaminaCost(시작할 때 한 번)는 쓰지 않는다(0). 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Cost", meta=(ClampMin=0))
	float ChargeStaminaCost = 25.f;

	UPROPERTY(EditDefaultsOnly, Category="Charge|Cost", meta=(ClampMin=0))
	float ReleaseStaminaCost = 15.f;

	/** (2026-10-26) 해방 후딜을 이동으로 끊을 때 해방 모션이 빠지는 시간(일반 공격의 RecoveryCancelBlendOut과 같은 값). */
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
	 * (2026-10-27) 최대 차지 순간 검 주변이 일렁이는 굴절 이펙트 (비우면 NS_Hit_Distortion_Once).
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
	bool CanStartCharge() const;
	void StartCharge();
	void StartChargeEffects();
	void StopChargeEffects();
	void OnFullChargeReached();
	void CheckReleaseGroundCrackTiming();
	void SpawnReleaseGroundCrack();
	void PlayLoop();
	void PlayMontage(UAnimMontage* Montage, FName TaskName, bool bLoopOnEnd);
	void Finish(bool bWasCancelled);
	/** (2026-10-26) 모으는 동안 행동 에너지를 조금씩 쓴다(ChargeStaminaCost를 MaxChargeTime에 걸쳐). */
	void TickChargeStamina();
	/** (2026-10-26) 해방 후딜: 캔슬 창이 열리고 후딜이 지난 뒤 새로 누른 이동 입력이면 해방 모션을 끊는다(일반 공격과 같은 규칙). */
	void CheckReleaseMoveCancel();

	enum class EPhase : uint8 { Deciding, TapBuffer, Charging, Releasing };
	EPhase Phase = EPhase::Deciding;
	FknightMeleeHitParams ActiveHitParams;
	double ChargeStartTime = 0.0;
	double TapDeadline = 0.0;
	float LastReleaseChargeAlpha = 0.f;
	float ReleaseCrackTargetPosition = 0.60f;
	bool bReleased = false;
	bool bChargeStateApplied = false;
	FTimerHandle TapTimer;
	FTimerHandle FullChargeTimer;
	FTimerHandle GroundCrackTimer;

	// (2026-10-26) 차지 행동 에너지·해방 후딜.
	FTimerHandle ChargeStaminaTimer;
	FTimerHandle ReleaseMoveCancelTimer;
	double LastChargeStaminaTime = 0.0;
	float ChargeStaminaSpent = 0.f;
	/** 모으는 동안 다른 공격을 막아 둔 상태(해방하면 풀고 일반 공격 규칙에 맡긴다). */
	bool bAttackBlockApplied = false;
	FknightMovePressTracker ReleaseMovePress;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActiveChargeAuraFX;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActiveChargeSwordFX;
};
