#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_JumpHover.generated.h"

class UAudioComponent;
class UCameraShakeBase;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;

/**
 * 점프 / 체공 (Space, InputTag.Jump). 누른 순간의 상태로 동작이 정해진다.
 *   - 땅 위: 점프. 누르고 있는 동안 엔진 점프 유지 시간만큼 더 높이 뜨고, 떼면 끝난다.
 *     몽타주(공격·검 뽑기·대시 등)가 재생 중이면 점프하지 않는다.
 *     (2026-10-26) 단, 가드·이동으로 끊을 수 있는 때(캔슬 창이 열려 있고 공격 후딜이 지난 뒤)에는 하던 동작을 끊고 뛴다.
 *     이 판단은 CanActivateAbility에서 해서, 미리 눌러 둔 점프도 입력 버퍼에 남아 있다가 그때 바로 나간다.
 *   - 공중: 체공. 키를 누르고 있는 동안 떠 있고, 떼거나 부스트 게이지가 0이 되면 떨어진다.
 *
 * 체공 느낌 (2026-10-08, "아이언맨처럼"):
 *   - 이동 모드는 낙하(MOVE_Falling) 그대로 두고 중력만 끈다. 비행 모드로 바꾸면 애니메이션이 땅 위 이동으로 바뀌어
 *     공중에서 다리가 걷는다. 낙하 상태라 애님 블루프린트는 공중 자세를 유지한다.
 *   - (2026-10-13 호버 느낌) 높이는 기준 높이에 스프링으로 끌린다: 떨어지다 잡히면 살짝 가라앉았다 되튀고,
 *     제자리에서도 수평으로 천천히 떠돈다. 출발할 때 추력이 천천히 올라오고, 멈출 때 몸을 뒤로 젖히며 오래 미끄러진다.
 *   - 제자리에서는 HoverBobAmplitude만큼 천천히 오르내린다(추진기로 버티는 느낌).
 *   - 수평 이동은 관성이 있다: 입력으로 밀고(HoverAirControl), 손을 떼면 HoverDrag로 천천히 멈춘다.
 *   - 몸(메시)을 미는 방향으로 기울인다: 앞으로 밀면 앞으로 숙이고, 옆으로 밀면 옆으로 기운다(최대 HoverMaxTilt).
 *     기울기는 캡슐 가운데를 축으로 돌고, 체공이 끝나면 부드럽게 똑바로 돌아온다.
 *   - 연출: 체공하는 동안 ThrusterSockets(기본 양발)에 ThrusterEffect를 붙여 아래로 분사한다. 분사 방향은 몸 기울기를 따라간다.
 *     시작할 때 HoverStartSound와 카메라 흔들림(HoverStartShake), 체공 중에는 HoverLoopSound(있으면)를 낸다.
 * 체공 중에는 State.Hovering을 건다(State.Boosting은 전투 자세를 켜서 쓰지 않음). 게이지 회복은 bCancelGaugeRecovery로 상쇄한다. 과열(State.Overheated) 중에는 체공할 수 없다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_JumpHover : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_JumpHover(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** 체공 중 초당 부스트 게이지 소모량 (최대 게이지 100 기준 20이면 5초). */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Cost", meta=(ClampMin=0))
	float HoverDrainPerSecond = 20.f;

	/** 체공 중 부스트 회복(BoostRecoveryRate)만큼 더 소모해서, 회복 GE가 돌고 있어도 게이지가 줄게 한다. */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Cost")
	bool bCancelGaugeRecovery = true;

	/** 이 값보다 게이지가 적으면 체공을 시작하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Cost", meta=(ClampMin=0))
	float MinGaugeToHover = 5.f;

	/**
	 * (2026-10-20 사용자 결정) 땅에서 실제로 점프할 때 쓰는 행동 에너지. 체공 시작에는 쓰지 않는다. 바뀔 수 있는 값.
	 * 0보다 크면 행동 에너지가 조금이라도 남아 있어야 뛴다(공격·가드와 같은 규칙). 0이면 소모 없음.
	 * (어빌리티의 StaminaCost는 체공에도 같이 빠져서 쓰지 않는다.)
	 */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Cost", meta=(ClampMin=0))
	float JumpStaminaCost = 10.f;

	/** 떨어지는 중에 체공을 켜면 낙하 속도를 여기까지만 남긴다(가라앉는 깊이). 그다음은 아래 스프링이 되돌린다. (2026-10-13) */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Vertical", meta=(ClampMin=0, Units="CentimetersPerSecond"))
	float HoverCatchMaxFallSpeed = 250.f;

	/**
	 * 호버 높이 스프링 (2026-10-13). 멈춘 높이에 몸을 고정하지 않고, 기준 높이에 용수철로 달린 것처럼 끌어당긴다.
	 * 떨어지다 잡히면 살짝 가라앉았다가 되튀고, 올라가던 중이면 조금 더 올라가다 내려와 자리 잡는다.
	 * 주파수(Hz)가 낮을수록 둥실하고, 감쇠(0~1)가 낮을수록 더 출렁인다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Vertical", meta=(ClampMin=0.1, ClampMax=5, Units="Hz"))
	float HoverSpringFrequency = 0.9f;

	UPROPERTY(EditDefaultsOnly, Category="Hover|Vertical", meta=(ClampMin=0.05, ClampMax=1.5))
	float HoverSpringDamping = 0.45f;

	/** 올라가던 중에 체공을 켜면 이 감속으로 멈출 때까지 더 올라간 높이를 기준으로 잡는다. */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Vertical", meta=(ClampMin=100))
	float HoverRiseDeceleration = 1800.f;

	/**
	 * 누르고 있으면 상승 (2026-10-13). 체공을 시작하고 HoverClimbDelay초 넘게 키를 계속 누르고 있으면
	 * 기준 높이가 천천히 올라간다(HoverClimbRampTime 동안 HoverClimbSpeed까지 빨라짐). 체공 시작 높이에서 최대 HoverMaxClimb까지.
	 * 짧게 눌렀다 떼면 지금처럼 그 높이에서 떠 있다 떨어진다. HoverClimbSpeed가 0이면 상승하지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Climb", meta=(ClampMin=0, Units="s"))
	float HoverClimbDelay = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category="Hover|Climb", meta=(ClampMin=0, Units="CentimetersPerSecond"))
	float HoverClimbSpeed = 180.f;

	UPROPERTY(EditDefaultsOnly, Category="Hover|Climb", meta=(ClampMin=0, Units="s"))
	float HoverClimbRampTime = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category="Hover|Climb", meta=(ClampMin=0, Units="cm"))
	float HoverMaxClimb = 900.f;

	/** 상승하는 동안 더 쓰는 부스트 게이지(초당, 상승 속도가 최대일 때). */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Climb", meta=(ClampMin=0))
	float HoverClimbExtraDrain = 8.f;

	/** 제자리에서 오르내리는 폭(위아래 각각). 0이면 흔들리지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Vertical", meta=(ClampMin=0, Units="cm"))
	float HoverBobAmplitude = 8.f;

	/** 한 번 오르내리는 데 걸리는 시간. */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Vertical", meta=(ClampMin=0.2, Units="s"))
	float HoverBobPeriod = 2.4f;

	/** 체공 중 이동 입력이 먹는 정도 (낙하 AirControl 대신 씀. 기본 낙하는 0.2). */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Horizontal", meta=(ClampMin=0, ClampMax=1))
	float HoverAirControl = 0.75f;

	/** 방향을 바꿀 때 속도가 새 방향으로 꺾이는 정도(낙하 마찰). 작을수록 미끄러지며 돌아간다. */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Horizontal", meta=(ClampMin=0))
	float HoverDrag = 0.5f;

	/** 입력이 없을 때의 감속(cm/s²). 작을수록 오래 미끄러진다. (2026-10-13) */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Horizontal", meta=(ClampMin=0))
	float HoverBrakingDeceleration = 90.f;

	/**
	 * 추력 올라오기 (2026-10-13). 방향키를 누른 직후에는 이동 입력이 HoverThrustStartScale배만 먹고,
	 * HoverThrustRampTime 동안 1배까지 올라간다. 추진기가 돌기 시작하는 느낌.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Horizontal", meta=(ClampMin=0, Units="s"))
	float HoverThrustRampTime = 0.45f;

	UPROPERTY(EditDefaultsOnly, Category="Hover|Horizontal", meta=(ClampMin=0, ClampMax=1))
	float HoverThrustStartScale = 0.25f;

	/** 제자리에서도 수평으로 천천히 떠도는 폭(cm). 0이면 떠돌지 않는다. (2026-10-13) */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Horizontal", meta=(ClampMin=0, Units="cm"))
	float HoverDriftAmplitude = 6.f;

	UPROPERTY(EditDefaultsOnly, Category="Hover|Horizontal", meta=(ClampMin=0.5, Units="s"))
	float HoverDriftPeriod = 3.3f;

	/**
	 * 몸이 기우는 최대 각도. 0이면 기울지 않는다.
	 * (2026-10-13) 입력이 아니라 실제 가속을 따른다: 출발할 때 앞으로 숙이고, 멈출 때는 뒤로 젖혀서 역분사하는 자세.
	 * 가속이 HoverTiltAccelReference면 최대 각도.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Tilt", meta=(ClampMin=0, ClampMax=45, Units="deg"))
	float HoverMaxTilt = 14.f;

	/** 기울기가 따라가는 빠르기. */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Tilt", meta=(ClampMin=0.1))
	float HoverTiltSpeed = 5.f;

	UPROPERTY(EditDefaultsOnly, Category="Hover|Tilt", meta=(ClampMin=1))
	float HoverTiltAccelReference = 700.f;

	/** 제자리에서 몸이 천천히 흔들리는 각도(앞뒤·좌우). 0이면 흔들리지 않는다. (2026-10-13) */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Tilt", meta=(ClampMin=0, ClampMax=10, Units="deg"))
	float HoverIdleSwayAngle = 1.5f;

	/** 체공 중 분사 이펙트. 로컬 -Z 방향으로 뿜는 시스템을 쓴다 (NS_Knight_Thruster). */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Effects")
	TObjectPtr<UNiagaraSystem> ThrusterEffect;

	/** 분사 이펙트를 붙일 소켓·뼈. */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Effects")
	TArray<FName> ThrusterSockets;

	/** 분사 이펙트 크기 배율. */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Effects", meta=(ClampMin=0.1))
	float ThrusterScale = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Hover|Effects")
	TObjectPtr<USoundBase> HoverStartSound;

	/** 체공하는 동안 반복 재생할 소리(루프 사운드). 끝날 때 서서히 줄인다. */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Effects")
	TObjectPtr<USoundBase> HoverLoopSound;

	/**
	 * 추진음 변화 (2026-10-12). 루프 소리에 로우패스 필터를 걸어 제자리에서는 낮게 웅웅거리고,
	 * 수평으로 빠르게 날수록 필터가 열리고 피치가 올라 "쉬이익" 하는 분사음이 된다.
	 * 게이지가 HoverLoopLowGauge 아래로 내려가면 소리가 떨려서(볼륨 흔들림) 연료가 바닥나는 느낌을 준다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Effects|Loop", meta=(ClampMin=20, ClampMax=20000, Units="Hz"))
	float HoverLoopLowPassIdle = 450.f;

	UPROPERTY(EditDefaultsOnly, Category="Hover|Effects|Loop", meta=(ClampMin=20, ClampMax=20000, Units="Hz"))
	float HoverLoopLowPassMoving = 2200.f;

	UPROPERTY(EditDefaultsOnly, Category="Hover|Effects|Loop", meta=(ClampMin=0.1, ClampMax=4))
	float HoverLoopPitchIdle = 0.9f;

	UPROPERTY(EditDefaultsOnly, Category="Hover|Effects|Loop", meta=(ClampMin=0.1, ClampMax=4))
	float HoverLoopPitchMoving = 1.15f;

	/** 이 수평 속도에서 "이동 중" 소리가 된다. */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Effects|Loop", meta=(ClampMin=1, Units="CentimetersPerSecond"))
	float HoverLoopFullSpeed = 600.f;

	/** 게이지가 이 값보다 적으면 소리가 떨린다. 0이면 떨지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Effects|Loop", meta=(ClampMin=0))
	float HoverLoopLowGauge = 25.f;

	UPROPERTY(EditDefaultsOnly, Category="Hover|Effects")
	TSubclassOf<UCameraShakeBase> HoverStartShake;

	UPROPERTY(EditDefaultsOnly, Category="Hover|Effects", meta=(ClampMin=0))
	float HoverStartShakeScale = 0.4f;

private:
	UFUNCTION()
	void OnReleased(float TimeHeld);

	bool StartHover();
	void StopHover();
	void TickHover();
	void UpdateTilt(float DeltaTime, const FRotator& Target);
	void EnsureTicking();
	void StartEffects();
	void StopEffects();
	void UpdateThrusterRotation() const;
	void UpdateLoopAudio(float DeltaTime, float HorizontalSpeed, float GaugeValue, double Now);

	// 추진음 필터·피치의 현재 값 (부드럽게 따라간다).
	float LoopBlend = 0.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> ThrusterComponents;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> LoopAudio;

	FTimerHandle TickTimer;
	double LastTickTime = 0.0;
	double HoverStartTime = 0.0;

	// Movement values replaced while hovering.
	float SavedGravityScale = 1.f;
	float SavedAirControl = 0.2f;
	float SavedLateralFriction = 0.f;
	float SavedBrakingFalling = 0.f;
	float SavedAirControlBoost = 2.f;

	// 호버 움직임 (2026-10-13).
	float HoverAnchorZ = 0.f;
	float HoverStartAnchorZ = 0.f;
	float CurrentClimbRate = 0.f;
	float ThrustHeldTime = 0.f;
	FVector PrevHorizontalVelocity = FVector::ZeroVector;
	FVector SmoothedAccel = FVector::ZeroVector;
	FVector PrevDriftVelocity = FVector::ZeroVector;

	// Mesh tilt: base relative transform captured when the tilt starts from upright.
	FVector MeshBaseLocation = FVector::ZeroVector;
	FQuat MeshBaseRotation = FQuat::Identity;
	FRotator CurrentTilt = FRotator::ZeroRotator;
	bool bTiltActive = false;

	bool bHovering = false;
	bool bJumped = false;
};
