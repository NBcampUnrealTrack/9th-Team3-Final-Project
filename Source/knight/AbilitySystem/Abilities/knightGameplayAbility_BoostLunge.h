#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "Combat/knightCombatTypes.h"
#include "knightGameplayAbility_BoostLunge.generated.h"

class ACharacter;
class UAnimMontage;
class UCameraShakeBase;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;

/**
 * 부스트 돌진 베기 (2026-10-12). 락온한 적이 멀리 있을 때 공격 키를 누르면, 등 부스터를 뿜으며 적 앞까지 날아가 벤다.
 *
 * 발동 조건 (BP에서 태그로 정한다):
 *   ActivationRequiredTags: State.LockOn.Far — 락온 컴포넌트(knightTargetLockComponent)가 락온 대상까지 남은 거리가
 *     FarTagDistance보다 멀 때 붙인다. 같은 공격 키의 약공격 1타·달리기 공격은 이 태그로 막아서, 멀 때는 이것만 나간다.
 *   ActivationBlockedTags: 공중(State.Airborne, 내려찍기), 콤보 중(Combo.Greatsword), 패리 반격 창(State.ParryCounter).
 *
 * 흐름:
 *   1. 돌진: 락온 대상 표면에서 StopDistance 앞 지점(ComputeAttackWarp, 최대 MaxLungeDistance)까지 루트 모션 힘으로 이동한다.
 *            시간 = 거리 / LungeSpeed (MinLungeTime~MaxLungeTime). 등 부스터(BackThrusterEffect)를 뿜고 DashMontage를 재생한다.
 *   2. 베기: 도착하면 StrikeMontage를 재생한다. 몽타주의 Knight Melee Trace가 보내는 Event.Melee.Hit를 HitParams로 처리한다.
 *            시작할 때 모션 워프 타깃(WarpTargetName)을 락온 대상 쪽으로 맞춰 몽타주의 워프 구간이 방향을 돌린다.
 *   몽타주가 끝나면 어빌리티가 끝난다. 피격 등으로 끊기면 부스터·이동도 같이 멈춘다.
 *   (2026-10-29) 0. 예열(선딜레이): 돌진 전에 StartupDelay초 동안 대상 쪽으로 돌아 돌진 자세로 멈추고 등 부스터를 먼저 켠다.
 *            이동 입력은 막고, 돌진 방향·거리는 예열이 끝날 때 다시 잰다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_BoostLunge : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_BoostLunge(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** (2026-10-22) bRequireWeaponInHand이면 칼이 등에 있을 때 발동하지 않는다. */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	/**
	 * (2026-10-22 사용자 요청) 칼이 손(WeaponHandSocket)에 있을 때만 돌진 베기를 쓴다. 칼이 등에 있으면 같은 공격 키의
	 * 1타(DrawIgnoredBlockedTags = State.LockOn.Far)가 먼저 칼을 뽑는다. 예전에는 멀리 락온하면 뽑지 않고 바로 돌진해 베었다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Lunge|Weapon")
	bool bRequireWeaponInHand = true;

	UPROPERTY(EditDefaultsOnly, Category="Lunge|Weapon", meta=(EditCondition="bRequireWeaponInHand"))
	FName WeaponHandSocket = TEXT("weapon_r");

	UPROPERTY(EditDefaultsOnly, Category="Lunge|Weapon", meta=(EditCondition="bRequireWeaponInHand"))
	FName WeaponComponentTag = TEXT("Weapon");

	/**
	 * 돌진할 수 있는 최대 거리(대상 표면 기준 도착 지점까지). 더 멀면 이 거리만큼만 간다.
	 * (2026-10-18 사용자 결정 '돌진 사거리 감소') 1800 → 900, (2026-10-19 '조금 더') 900 → 700. 바뀔 수 있는 값(BP에서 시험 후 조절).
	 */
	UPROPERTY(EditDefaultsOnly, Category="Lunge|Motion", meta=(ClampMin=0, Units="cm"))
	float MaxLungeDistance = 700.f;

	/**
	 * (2026-10-29 사용자 요청) 선딜레이: 돌진하기 전에 돌진 자세로 멈춰 등 부스터를 예열하는 시간(초, 실제 시간). 0이면 예전처럼 바로 돌진.
	 * 공중 돌진(bAirLunge)에는 쓰지 않는다. 바뀔 수 있는 값. (같은 날 사용자 결정) 0.4 → 0.2.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Lunge|Motion", meta=(ClampMin=0, Units="s"))
	float StartupDelay = 0.2f;

	/** 대상 표면(부위 지점이면 그 지점)에서 이만큼 앞에 멈춘다. 칼이 닿는 거리. */
	UPROPERTY(EditDefaultsOnly, Category="Lunge|Motion", meta=(ClampMin=0, Units="cm"))
	float StopDistance = 170.f;

	/**
	 * 돌진 속도. (2026-10-29 사용자 결정) GA_Knight_BoostLunge 2600 → 1300(절반) → 1000('좀 더'):
	 * 최대 7m를 0.27초 → 0.7초에 간다(MaxLungeTime도 0.6 → 0.75로 올려 먼 거리도 느려지게). 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Lunge|Motion", meta=(ClampMin=100, Units="CentimetersPerSecond"))
	float LungeSpeed = 2600.f;

	UPROPERTY(EditDefaultsOnly, Category="Lunge|Motion", meta=(ClampMin=0.05, Units="s"))
	float MinLungeTime = 0.18f;

	/** 돌진 시간 상한. (2026-10-29) GA_Knight_BoostLunge 0.6 → 0.75: 돌진 속도를 1000으로 줄여도 7m(0.7초)가 잘리지 않게. */
	UPROPERTY(EditDefaultsOnly, Category="Lunge|Motion", meta=(ClampMin=0.05, Units="s"))
	float MaxLungeTime = 0.6f;

	/** 돌진하는 동안 재생하는 자세 (예: AM_Knight_Dash). 비워 두면 재생하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Lunge|Montage")
	TObjectPtr<UAnimMontage> DashMontage;

	UPROPERTY(EditDefaultsOnly, Category="Lunge|Montage", meta=(ClampMin=0.01))
	float DashPlayRate = 1.5f;

	/** 도착해서 베는 동작. Knight Melee Trace(판정)를 넣는다 (예: AM_Knight_RunAttack). */
	UPROPERTY(EditDefaultsOnly, Category="Lunge|Montage")
	TObjectPtr<UAnimMontage> StrikeMontage;

	/**
	 * 베기 몽타주 배속(행동 속도·공격 배속·스탠스를 곱함). (2026-10-29 사용자 결정) GA_Knight_BoostLunge 1.15 → 1.25 → 1.37:
	 * 한 번에 약 0.2초씩, 베기가 끝나는 시점이 처음보다 한손 약 0.37초·양손 약 0.45초 빨라졌다(2.5초 몽타주, 공격 배속 0.75 기준). 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Lunge|Montage", meta=(ClampMin=0.01))
	float StrikePlayRate = 1.f;

	/** (2026-10-15) 한손 스탠스일 때의 돌진 자세. 비어 있으면 DashMontage를 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category="Lunge|Montage")
	TObjectPtr<UAnimMontage> OneHandedDashMontage;

	/** (2026-10-15) 한손 스탠스일 때의 베기. 비어 있으면 StrikeMontage를 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category="Lunge|Montage")
	TObjectPtr<UAnimMontage> OneHandedStrikeMontage;

	UPROPERTY(EditDefaultsOnly, Category="Lunge|Montage")
	FName WarpTargetName = TEXT("AttackTarget");

	UPROPERTY(EditDefaultsOnly, Category="Lunge|Hit")
	FknightMeleeHitParams HitParams;

	/** 등 부스터 (대시와 같은 방식: 로컬 -Z로 뿜는 시스템, NS_Knight_BackBoost). 비우면 나오지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Lunge|Back Booster")
	TObjectPtr<UNiagaraSystem> BackThrusterEffect;

	UPROPERTY(EditDefaultsOnly, Category="Lunge|Back Booster")
	FName BackThrusterBone = TEXT("spine_03");

	/** 액터 기준 위치(X 앞, Y 오른쪽, Z 위). 부스터 하나에 하나씩. */
	UPROPERTY(EditDefaultsOnly, Category="Lunge|Back Booster")
	TArray<FVector> BackThrusterOffsets;

	UPROPERTY(EditDefaultsOnly, Category="Lunge|Back Booster", meta=(ClampMin=0.1))
	float BackThrusterScale = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Lunge|Back Booster", meta=(ClampMin=0, ClampMax=80, Units="deg"))
	float BackThrusterDownAngle = 35.f;

	/** 돌진이 끝난 뒤 분사를 이만큼 더 유지한다. */
	UPROPERTY(EditDefaultsOnly, Category="Lunge|Back Booster", meta=(ClampMin=0, Units="s"))
	float BackThrusterLinger = 0.1f;

	/**
	 * 공중 돌진 (2026-10-12). 켜면 락온 지점 높이 쪽으로도 날아가고(최대 MaxAirHeightChange), 발 추진기도 뿜으며,
	 * 도착해서 베는 동안 AirHangTime초 멈췄다가 떨어진다. 공중용 BP(GA_Knight_BoostLunge_Air)에서 켠다.
	 * 체공(Ability.Boost.Flight)은 발동할 때 끝난다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Lunge|Air")
	bool bAirLunge = false;

	UPROPERTY(EditDefaultsOnly, Category="Lunge|Air", meta=(ClampMin=0, Units="cm", EditCondition="bAirLunge"))
	float MaxAirHeightChange = 600.f;

	/** 락온 지점 높이에 더하는 값. 음수면 지점보다 아래로(칼이 지점에 닿도록) 날아간다. */
	UPROPERTY(EditDefaultsOnly, Category="Lunge|Air", meta=(Units="cm", EditCondition="bAirLunge"))
	float AirTargetHeightOffset = -80.f;

	UPROPERTY(EditDefaultsOnly, Category="Lunge|Air", meta=(ClampMin=0, Units="s", EditCondition="bAirLunge"))
	float AirHangTime = 0.35f;

	/** 발 추진기 (체공·내려찍기와 같은 NS_Knight_Thruster). 비우면 나오지 않는다. 공중 돌진에서만 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category="Lunge|Air", meta=(EditCondition="bAirLunge"))
	TObjectPtr<UNiagaraSystem> FootThrusterEffect;

	UPROPERTY(EditDefaultsOnly, Category="Lunge|Air", meta=(EditCondition="bAirLunge"))
	TArray<FName> FootThrusterSockets;

	UPROPERTY(EditDefaultsOnly, Category="Lunge|Air", meta=(ClampMin=0.1, EditCondition="bAirLunge"))
	float FootThrusterScale = 1.3f;

	UPROPERTY(EditDefaultsOnly, Category="Lunge|Effects")
	TObjectPtr<USoundBase> LungeSound;

	UPROPERTY(EditDefaultsOnly, Category="Lunge|Effects")
	TSubclassOf<UCameraShakeBase> LungeShake;

	UPROPERTY(EditDefaultsOnly, Category="Lunge|Effects", meta=(ClampMin=0))
	float LungeShakeScale = 0.4f;

	/** 부스트 돌진 이동 동안 메카 홀로그램 잔상을 남길지 여부. */
	UPROPERTY(EditDefaultsOnly, Category="Lunge|Ghost Trail")
	bool bSpawnGhostTrail = true;

	UPROPERTY(EditDefaultsOnly, Category="Lunge|Ghost Trail", meta=(EditCondition="bSpawnGhostTrail"))
	TObjectPtr<UMaterialInterface> GhostMaterial;

	UPROPERTY(EditDefaultsOnly, Category="Lunge|Ghost Trail", meta=(ClampMin=0.02, Units="s", EditCondition="bSpawnGhostTrail"))
	float GhostSpawnInterval = 0.06f;

	UPROPERTY(EditDefaultsOnly, Category="Lunge|Ghost Trail", meta=(ClampMin=0.05, Units="s", EditCondition="bSpawnGhostTrail"))
	float GhostLifetime = 0.28f;

	UPROPERTY(EditDefaultsOnly, Category="Lunge|Ghost Trail", meta=(ClampMin=0.05, ClampMax=1.0, EditCondition="bSpawnGhostTrail"))
	float GhostInitialOpacity = 0.65f;

	UPROPERTY(EditDefaultsOnly, Category="Lunge|Ghost Trail", meta=(EditCondition="bSpawnGhostTrail"))
	FLinearColor OneHandedGhostColor = FLinearColor(0.15f, 1.1f, 3.2f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category="Lunge|Ghost Trail", meta=(EditCondition="bSpawnGhostTrail"))
	FLinearColor TwoHandedGhostColor = FLinearColor(3.2f, 0.22f, 0.08f, 1.f);

	/**
	 * (2026-10-26 사용자 요청 '몬스터와 붙어 있을 때는 잔상 없이') 돌진이 멈출 지점까지 남은 거리가 이 값 이하면 잔상을 남기지 않는다
	 * (이미 기본 공격이 닿는 거리). 돌진하다 이 안으로 들어와도 멈춘다. 점프 공격·대쉬공격(FknightGhostTrailSettings)과 같은 값. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Lunge|Ghost Trail", meta=(ClampMin=0, Units="cm", EditCondition="bSpawnGhostTrail"))
	float GhostInRangeDistance = 100.f;

private:
	/** (2026-10-29) 예열이 끝나면(또는 예열 없이 바로) 돌진 방향을 다시 재고 돌진을 시작한다. */
	void StartLunge();

	/** (2026-10-29) 예열 동안 이동 입력을 막는다(컨트롤러 SetIgnoreMoveInput, 내가 건 만큼만 푼다). */
	void SetStartupMoveLocked(bool bLock);

	FTimerHandle StartupDelayTimer;
	bool bStartupMoveLocked = false;

	UFUNCTION()
	void OnLungeFinished();

	UFUNCTION()
	void OnStrikeFinished();

	UFUNCTION()
	void OnMeleeHitEvent(FGameplayEventData Payload);

	void StartBackThrusters(const ACharacter& Character);
	void UpdateBackThrusters();
	void StopBackThrusters();
	void StartFootThrusters(const ACharacter& Character);
	void StopFootThrusters();
	void RestoreGravity();
	void SpawnGhostStep();
	void StopGhostTrail();

	/** 지금 스탠스(한손/양손)에 맞는 몽타주. 한손용이 비어 있으면 양손용. */
	UAnimMontage* PickStanceMontage(UAnimMontage* TwoHanded, UAnimMontage* OneHanded) const;

	/** 이번 발동에서 재생한 돌진 몽타주 (끊길 때 멈추려고). */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveDashMontage;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> BackThrusters;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> FootThrusters;

	FTimerHandle AirHangTimer;
	bool bGravitySaved = false;
	float SavedGravityScale = 1.f;

	FTimerHandle BackThrusterTimer;
	FTimerHandle BackThrusterStopTimer;
	FTimerHandle GhostTrailTimer;
	/** (2026-10-26) 돌진이 멈출 지점. 잔상은 여기까지 GhostInRangeDistance 안으로 들어오면 멈춘다. */
	FVector GhostStopLocation = FVector::ZeroVector;

	/** 이번 돌진 방향(수평 단위 벡터). */
	FVector LungeDirection = FVector::ZeroVector;
	bool bStrikeStarted = false;

};
