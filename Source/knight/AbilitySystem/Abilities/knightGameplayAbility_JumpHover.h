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
 *   - 공중: 체공. 키를 누르고 있는 동안 떠 있고, 떼거나 부스트 게이지가 0이 되면 떨어진다.
 *
 * 체공 느낌 (2026-10-08, "아이언맨처럼"):
 *   - 이동 모드는 낙하(MOVE_Falling) 그대로 두고 중력만 끈다. 비행 모드로 바꾸면 애니메이션이 땅 위 이동으로 바뀌어
 *     공중에서 다리가 걷는다. 낙하 상태라 애님 블루프린트는 공중 자세를 유지한다.
 *   - 떨어지던 속도를 한 번에 0으로 만들지 않고 HoverCatchSpeed로 부드럽게 잡는다. 떨어지는 중에 켜면 살짝 위로 튄다.
 *   - 제자리에서는 HoverBobAmplitude만큼 천천히 오르내린다(추진기로 버티는 느낌).
 *   - 수평 이동은 관성이 있다: 입력으로 밀고(HoverAirControl), 손을 떼면 HoverDrag로 천천히 멈춘다.
 *   - 몸(메시)을 미는 방향으로 기울인다: 앞으로 밀면 앞으로 숙이고, 옆으로 밀면 옆으로 기운다(최대 HoverMaxTilt).
 *     기울기는 캡슐 가운데를 축으로 돌고, 체공이 끝나면 부드럽게 똑바로 돌아온다.
 *   - 연출: 체공하는 동안 ThrusterSockets(기본 양발)에 ThrusterEffect를 붙여 아래로 분사한다. 분사 방향은 몸 기울기를 따라간다.
 *     시작할 때 HoverStartSound와 카메라 흔들림(HoverStartShake), 체공 중에는 HoverLoopSound(있으면)를 낸다.
 * 체공 중에는 State.Boosting을 걸어 게이지 회복을 멈춘다. 과열(State.Overheated) 중에는 체공할 수 없다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_JumpHover : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_JumpHover(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** 체공 중 초당 부스트 게이지 소모량 (최대 게이지 100 기준 20이면 5초). */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Cost", meta=(ClampMin=0))
	float HoverDrainPerSecond = 20.f;

	/** 이 값보다 게이지가 적으면 체공을 시작하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Cost", meta=(ClampMin=0))
	float MinGaugeToHover = 5.f;

	/** 떨어지는 중에 체공을 켜면 이 속도로 살짝 위로 튄다. 0이면 튀지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Vertical", meta=(ClampMin=0, Units="CentimetersPerSecond"))
	float HoverStartLift = 180.f;

	/** 수직 속도를 목표값으로 맞추는 빠르기. 클수록 딱 멈추고, 작을수록 둥실하게 잡힌다. */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Vertical", meta=(ClampMin=0.1))
	float HoverCatchSpeed = 4.f;

	/** 제자리에서 오르내리는 폭(위아래 각각). 0이면 흔들리지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Vertical", meta=(ClampMin=0, Units="cm"))
	float HoverBobAmplitude = 8.f;

	/** 한 번 오르내리는 데 걸리는 시간. */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Vertical", meta=(ClampMin=0.2, Units="s"))
	float HoverBobPeriod = 2.4f;

	/** 체공 중 이동 입력이 먹는 정도 (낙하 AirControl 대신 씀. 기본 낙하는 0.2). */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Horizontal", meta=(ClampMin=0, ClampMax=1))
	float HoverAirControl = 0.75f;

	/** 입력이 없을 때 수평 속도가 줄어드는 정도. 작을수록 오래 미끄러진다. */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Horizontal", meta=(ClampMin=0))
	float HoverDrag = 1.5f;

	/** 미는 방향으로 몸이 기우는 최대 각도. 0이면 기울지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Tilt", meta=(ClampMin=0, ClampMax=45, Units="deg"))
	float HoverMaxTilt = 14.f;

	/** 기울기가 따라가는 빠르기. */
	UPROPERTY(EditDefaultsOnly, Category="Hover|Tilt", meta=(ClampMin=0.1))
	float HoverTiltSpeed = 5.f;

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

	// Mesh tilt: base relative transform captured when the tilt starts from upright.
	FVector MeshBaseLocation = FVector::ZeroVector;
	FQuat MeshBaseRotation = FQuat::Identity;
	FRotator CurrentTilt = FRotator::ZeroRotator;
	bool bTiltActive = false;

	bool bHovering = false;
	bool bJumped = false;
};
