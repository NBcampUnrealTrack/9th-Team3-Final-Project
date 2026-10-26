#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "Combat/knightCombatTypes.h"
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

	/** 차지 해방 타격이 지면을 때릴 때 생성되는 바닥 균열 이펙트 (비우면 NS_Hit_GroundCrack). */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Effects", meta=(EditCondition="bPlayChargeEffects"))
	TObjectPtr<UNiagaraSystem> ReleaseGroundCrackEffect;

	/** 풀차지 또는 양손 차지 해방 시 추가로 터지는 대지 균열/파열 이펙트 (비우면 NS_Hit_GroundRift). */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Effects", meta=(EditCondition="bPlayChargeEffects"))
	TObjectPtr<UNiagaraSystem> ReleaseGroundRiftEffect;

	/** 차지 해방 타격 시 지면에서 피어오르는 흙먼지 이펙트 (비우면 NS_Monster_GroundDust). */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Effects", meta=(EditCondition="bPlayChargeEffects"))
	TObjectPtr<UNiagaraSystem> ReleaseGroundDustEffect;

	/** 차지 해방 몽타주 시작 후 지면 균열 이펙트가 터지기까지의 기준 지연 시간(초, 공격 속도에 반비례). */
	UPROPERTY(EditDefaultsOnly, Category="Charge|Effects", meta=(ClampMin=0.02, Units="s", EditCondition="bPlayChargeEffects"))
	float ReleaseGroundCrackDelay = 0.24f;

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
	void SpawnReleaseGroundCrack();
	void PlayLoop();
	void PlayMontage(UAnimMontage* Montage, FName TaskName, bool bLoopOnEnd);
	void Finish(bool bWasCancelled);

	enum class EPhase : uint8 { Deciding, TapBuffer, Charging, Releasing };
	EPhase Phase = EPhase::Deciding;
	FknightMeleeHitParams ActiveHitParams;
	double ChargeStartTime = 0.0;
	double TapDeadline = 0.0;
	float LastReleaseChargeAlpha = 0.f;
	bool bReleased = false;
	bool bChargeStateApplied = false;
	FTimerHandle TapTimer;
	FTimerHandle FullChargeTimer;
	FTimerHandle GroundCrackTimer;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActiveChargeAuraFX;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActiveChargeSwordFX;
};
