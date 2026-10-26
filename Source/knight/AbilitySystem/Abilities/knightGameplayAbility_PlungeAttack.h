#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "Combat/knightCombatTypes.h"
#include "Combat/knightGhostTrailActor.h"
#include "knightGameplayAbility_PlungeAttack.generated.h"

class UAnimMontage;
class UAudioComponent;
class UCameraShakeBase;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;

/**
 * 내려찍기 (2026-10-12). 공중(점프·체공)에서 공격 키를 누르면 나간다(ActivationRequiredTags State.Airborne).
 * 체공 중이면 체공(Ability.Boost.Flight)을 끝낸다.
 *
 * 추진기 역분사로 내리꽂는 흐름 (사용자 선택 "A+C"):
 *   1. 반동: 발 추진기가 짧게 터지며 BoostUpTime 동안 BoostUpSpeed로 살짝 떠오르고, 몸을 뒤로 젖힌다(BoostLeanPitch).
 *            칼을 머리 위로 드는 자세(StartMontage)로 들어간다.
 *   2. 낙하: 추진기를 위로 뒤집어 뿜으며(역분사) DiveAccelTime 동안 DiveSpeed까지 가속해 아래로 꽂힌다.
 *            몸은 앞으로 숙인다(DiveLeanPitch). 락온 대상이나 앞쪽 가까운 적이 있으면 그 근처로 떨어진다(MaxAimDistance).
 *            공중 조작은 막는다. 떨어지는 동안 LoopMontage를 반복한다.
 *   3. 착지: 땅에 닿으면 EndMontage(내려찍기)를 재생한다. 피해는 몽타주의 Knight Melee Area가 Event.Melee.Hit로 보내고
 *            HitParams로 처리한다(가드·패링·히트스톱은 일반 공격과 같은 규칙). 흔들림·먼지는 몽타주의 Knight Ground Impact와
 *            무브먼트 컴포넌트의 착지 충격이 맡는다. 착지 후 ImpactDelay에 맞은 대상이 없어도 자기 히트스톱(ImpactSelfHitStop)과
 *            충격 이펙트(ImpactEffect)를 낸다.
 *   MaxDiveTime 안에 땅에 닿지 않으면(낭떠러지) 원래대로 돌려 놓고 끝낸다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_PlungeAttack : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_PlungeAttack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** 반동 동안 칼을 드는 자세. */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Montage")
	TObjectPtr<UAnimMontage> StartMontage;

	/** 떨어지는 동안 반복하는 동작. */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Montage")
	TObjectPtr<UAnimMontage> LoopMontage;

	/** 착지해서 내려찍고 일어나는 동작. Knight Melee Area(피해)와 Knight Ground Impact(흔들림·먼지)를 넣는다. */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Montage")
	TObjectPtr<UAnimMontage> EndMontage;

	/** (2026-10-15) 한손 스탠스용. 비어 있는 것은 양손용(위)을 쓴다. 스탠스는 발동할 때 한 번 정한다. */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Montage")
	TObjectPtr<UAnimMontage> OneHandedStartMontage;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Montage")
	TObjectPtr<UAnimMontage> OneHandedLoopMontage;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Montage")
	TObjectPtr<UAnimMontage> OneHandedEndMontage;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Montage", meta=(ClampMin=0.01))
	float PlayRate = 1.f;

	/** EndMontage를 이 시간(몽타주 초)까지만 재생하고 블렌드 아웃해서 끝낸다. 0이면 끝까지. */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Montage", meta=(ClampMin=0, Units="s"))
	float EndRecoveryTime = 0.f;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Montage", meta=(ClampMin=0, Units="s"))
	float EndRecoveryBlendOut = 0.3f;

	/** 반동으로 떠오르는 시간. */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Motion", meta=(ClampMin=0.02, Units="s"))
	float BoostUpTime = 0.18f;

	/** 반동으로 떠오르는 속도(위로). */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Motion", meta=(ClampMin=0, Units="CentimetersPerSecond"))
	float BoostUpSpeed = 380.f;

	/** 내리꽂는 최고 속도(아래로). */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Motion", meta=(ClampMin=100, Units="CentimetersPerSecond"))
	float DiveSpeed = 2800.f;

	/** 0에서 DiveSpeed까지 가속하는 시간. */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Motion", meta=(ClampMin=0.01, Units="s"))
	float DiveAccelTime = 0.12f;

	/** 노릴 대상이 없을 때 앞으로 더하는 수평 속도. */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Motion", meta=(ClampMin=0, Units="CentimetersPerSecond"))
	float DiveForwardSpeed = 250.f;

	/** 수평 속도 상한 (대상 쪽으로 끌 때). */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Motion", meta=(ClampMin=0, Units="CentimetersPerSecond"))
	float MaxDiveHorizontalSpeed = 1400.f;

	/** 이 시간 안에 땅에 닿지 않으면 끝낸다. */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Motion", meta=(ClampMin=0.2, Units="s"))
	float MaxDiveTime = 3.f;

	/** 반동 때 몸을 뒤로 젖히는 각도(+), 낙하 때 앞으로 숙이는 각도(-). 캡슐 가운데를 축으로 메시만 돌린다. */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Motion", meta=(ClampMin=-45, ClampMax=45, Units="deg"))
	float BoostLeanPitch = 10.f;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Motion", meta=(ClampMin=-45, ClampMax=45, Units="deg"))
	float DiveLeanPitch = -18.f;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Motion", meta=(ClampMin=0.1))
	float LeanSpeed = 14.f;

	/**
	 * (2026-10-15) 점프 베기 모드 (GA_Knight_JumpAttack). 켜면 반동·역분사 대신, 대상(락온) 앞 땅으로 포물선을 그리며 뛰어간다.
	 * 뛰는 동안 StartMontage(휘두르기) → LoopMontage, 착지하면 EndMontage. 중력은 그대로 두고 출발 속도만 정한다.
	 * 거리는 MaxAimDistance까지, 도착은 대상 표면에서 AimStopDistance 앞.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Leap")
	bool bLeapToTarget = false;

	/** 뛰어가는 수평 속도. 비행 시간 = 거리 / LeapSpeed (MinLeapTime~MaxLeapTime). */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Leap", meta=(ClampMin=100, Units="CentimetersPerSecond", EditCondition="bLeapToTarget"))
	float LeapSpeed = 1600.f;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Leap", meta=(ClampMin=0.1, Units="s", EditCondition="bLeapToTarget"))
	float MinLeapTime = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Leap", meta=(ClampMin=0.1, Units="s", EditCondition="bLeapToTarget"))
	float MaxLeapTime = 1.1f;

	/** 뛰어가는 동안의 자세 (칼을 든 점프 루프). 도착 지점까지 LeapSwingDistance 안으로 들어오면 StartMontage(휘두르기)로 바꾼다. 비우면 출발하며 바로 휘두른다. */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Leap", meta=(EditCondition="bLeapToTarget"))
	TObjectPtr<UAnimMontage> LeapPoseMontage;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Leap", meta=(EditCondition="bLeapToTarget"))
	TObjectPtr<UAnimMontage> OneHandedLeapPoseMontage;

	/**
	 * 도착 지점(대상 표면에서 AimStopDistance 앞)까지 남은 거리가 이만큼 이하가 되면 휘두른다.
	 * 거리 기준이라 멀리서 뛰든 가까이서 뛰든 대상 앞 같은 곳에서 휘두른다. 출발할 때 이미 안이면 바로 휘두른다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Leap", meta=(ClampMin=0, Units="cm", EditCondition="bLeapToTarget"))
	float LeapSwingDistance = 300.f;

	/** 대상이 없을 때 앞으로 뛰는 거리. */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Leap", meta=(ClampMin=0, Units="cm", EditCondition="bLeapToTarget"))
	float LeapNoTargetDistance = 300.f;

	/** 이 거리 안의 대상(락온, 없으면 앞쪽 가까운 적) 근처로 떨어진다. 0이면 노리지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Aim", meta=(ClampMin=0, Units="cm"))
	float MaxAimDistance = 900.f;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Aim", meta=(ClampMin=0, Units="cm"))
	float AimStopDistance = 150.f;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Aim", meta=(ClampMin=0, Units="cm"))
	float SoftTargetRange = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Aim", meta=(ClampMin=1, ClampMax=180, Units="deg"))
	float SoftTargetAngle = 60.f;

	/**
	 * (2026-10-26 사용자 요청) 점프 공격(bLeapToTarget)이 대상 쪽으로 뛰어갈 때 락온 돌진과 같은 잔상을 남긴다(대상이 있을 때만, 착지까지).
	 * 공중 내려찍기(낙하)에는 남기지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Ghost Trail")
	bool bLeapGhostTrail = true;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Ghost Trail", meta=(EditCondition="bLeapGhostTrail"))
	FknightGhostTrailSettings LeapGhostTrailSettings;

	/** 추진 이펙트 (로컬 -Z로 뿜는 시스템, NS_Knight_Thruster). 반동 때는 아래로, 낙하 때는 위로 뒤집어 뿜는다. */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Effects")
	TObjectPtr<UNiagaraSystem> ThrusterEffect;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Effects")
	TArray<FName> ThrusterSockets;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Effects", meta=(ClampMin=0.1))
	float BoostThrusterScale = 1.6f;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Effects", meta=(ClampMin=0.1))
	float DiveThrusterScale = 1.3f;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Effects")
	TObjectPtr<USoundBase> BoostSound;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Effects")
	TObjectPtr<USoundBase> DiveSound;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Effects")
	TSubclassOf<UCameraShakeBase> BoostShake;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Effects", meta=(ClampMin=0))
	float BoostShakeScale = 0.5f;

	/** 착지 후 이 시간에 자기 히트스톱과 충격 이펙트를 낸다. EndMontage의 내려찍는 프레임에 맞춘다. */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Impact", meta=(ClampMin=0, Units="s"))
	float ImpactDelay = 0.22f;

	/** 맞은 대상이 없어도 거는 자기 히트스톱. 0이면 없음. */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Impact", meta=(ClampMin=0, Units="s"))
	float ImpactSelfHitStop = 0.08f;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Impact")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Impact", meta=(ClampMin=0))
	float ImpactEffectScale = 2.f;

	/** 착지 내려찍기 시 지면에 남는 균열 이펙트 (비우면 NS_Hit_GroundCrack). */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Impact")
	TObjectPtr<UNiagaraSystem> GroundCrackEffect;

	/** 양손 또는 수직 급강하 내려찍기 시 추가로 터지는 지면 파열 이펙트 (비우면 NS_Hit_GroundRift). */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Impact")
	TObjectPtr<UNiagaraSystem> GroundRiftEffect;

	/** 착지 내려찍기 시 지면에서 피어오르는 흙먼지 이펙트 (비우면 NS_Monster_GroundDust). */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Impact")
	TObjectPtr<UNiagaraSystem> GroundDustEffect;

	/** 착지 타격 (Knight Melee Area가 보내는 Event.Melee.Hit에 쓴다). */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Hit")
	FknightMeleeHitParams HitParams;

private:
	enum class EPhase : uint8 { None, Boost, Dive, Leap, Land };

	void StartDive();
	/** 점프 베기: 대상 앞 땅으로 포물선 도약. */
	void StartLeap();
	FVector LeapHorizontalVelocity = FVector::ZeroVector;
	float LeapDuration = 0.f;
	FVector LeapDestination = FVector::ZeroVector;
	/** 도착 지점까지 남은 거리(3D). */
	float GetLeapRemainingDistance() const;
	bool bLeapSwingStarted = false;
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveLeapPoseMontage;
	void TickPlunge();
	void StartLand();
	void OnImpact();
	void FinishRecovery();
	void RestoreMovement();
	void PlayMontage(UAnimMontage* Montage, bool bEndOnFinish);
	void SpawnThrusters(float Scale);
	void StopThrusters();
	void PlayThrusterSound(USoundBase* Sound);
	void StopThrusterSound(float FadeOutTime = 0.1f);
	void UpdateThrusterRotation() const;
	void UpdateLean(float DeltaTime, float TargetPitch);
	void ResetLean();

	UFUNCTION()
	void OnEndMontageFinished();

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION()
	void OnMeleeHitEvent(FGameplayEventData Payload);

	/**
	 * (2026-10-20 사용자 결정 '점프 공격은 1타만') 이번 발동에서 이미 맞힌 대상. 시작 베기(MeleeTrace)와 착지 충격(MeleeArea)이
	 * 같은 적을 각각 맞혀 두 번 들어가던 것을 막는다. 먼저 맞힌 쪽만 들어간다.
	 */
	TSet<TWeakObjectPtr<AActor>> HitActorsThisAttack;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> ThrusterComponents;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ActiveThrusterAudio;

	// 이번 발동에서 쓰는 몽타주 (발동할 때 스탠스로 고른다).
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveStartMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveLoopMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveEndMontage;

	FTimerHandle PhaseTimer;
	FTimerHandle TickTimer;
	FTimerHandle ImpactTimer;
	EPhase Phase = EPhase::None;
	double PhaseStartTime = 0.0;
	FVector DiveHorizontalVelocity = FVector::ZeroVector;
	bool bThrustersReversed = false;

	// 바꾼 이동 값.
	bool bMovementSaved = false;
	float SavedGravityScale = 1.f;
	float SavedAirControl = 0.2f;

	// 몸 기울기: 시작할 때 메시의 상대 위치·회전.
	bool bLeanActive = false;
	FVector MeshBaseLocation = FVector::ZeroVector;
	FQuat MeshBaseRotation = FQuat::Identity;
	float CurrentLeanPitch = 0.f;
};
