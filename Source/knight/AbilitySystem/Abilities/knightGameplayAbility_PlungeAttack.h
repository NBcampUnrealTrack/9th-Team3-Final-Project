#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "Combat/knightCombatTypes.h"
#include "knightGameplayAbility_PlungeAttack.generated.h"

class UAnimMontage;
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

	/** 이 거리 안의 대상(락온, 없으면 앞쪽 가까운 적) 근처로 떨어진다. 0이면 노리지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Aim", meta=(ClampMin=0, Units="cm"))
	float MaxAimDistance = 900.f;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Aim", meta=(ClampMin=0, Units="cm"))
	float AimStopDistance = 150.f;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Aim", meta=(ClampMin=0, Units="cm"))
	float SoftTargetRange = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category="Plunge|Aim", meta=(ClampMin=1, ClampMax=180, Units="deg"))
	float SoftTargetAngle = 60.f;

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

	/** 착지 타격 (Knight Melee Area가 보내는 Event.Melee.Hit에 쓴다). */
	UPROPERTY(EditDefaultsOnly, Category="Plunge|Hit")
	FknightMeleeHitParams HitParams;

private:
	enum class EPhase : uint8 { None, Boost, Dive, Land };

	void StartDive();
	void TickPlunge();
	void StartLand();
	void OnImpact();
	void FinishRecovery();
	void RestoreMovement();
	void PlayMontage(UAnimMontage* Montage, bool bEndOnFinish);
	void SpawnThrusters(float Scale);
	void StopThrusters();
	void UpdateThrusterRotation() const;
	void UpdateLean(float DeltaTime, float TargetPitch);
	void ResetLean();

	UFUNCTION()
	void OnEndMontageFinished();

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION()
	void OnMeleeHitEvent(FGameplayEventData Payload);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> ThrusterComponents;

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
