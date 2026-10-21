#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "knightMovementComponent.generated.h"

class UCameraShakeBase;
class UCurveFloat;
struct FGameplayAttribute;

/**
 * 나이트 이동. 메카의 무게감을 살리도록 템플릿보다 무겁게 조정되어 있다
 * (느린 가속·회전, 강한 중력).
 *
 * 부스트 비행은 엔진의 MOVE_Flying 물리를 쓴다. 비행 어빌리티가 비행을 시작·종료하고 게이지를 소모한다.
 * 부스트 대시는 이동 모드가 아니다: 대시 어빌리티가 루트 모션 소스를 건다.
 *
 * 이동 상태는 소유자 ASC에 루즈 태그(State.Flying, State.Airborne, State.Sprinting)로 반영되므로,
 * 어빌리티가 무브먼트 컴포넌트에 묻지 않고도 이 상태를 조건으로 쓸 수 있다.
 * State.Sprinting (2026-10-12): 달리기를 원하고, 땅 위에서, 걷기 최대 속도보다 빠르게 움직이는 동안.
 *   달리기 공격(GA_Knight_RunAttack)이 이 태그를 요구하고, 일반 1타(GA_Knight_Attack_1)는 이 태그가 있으면 막힌다.
 *   예전에는 Sprint 어빌리티가 붙였는데, Shift 달리기를 InputTag.Run 네이티브 입력으로 옮기면서 여기로 옮겼다.
 *
 * 속도 스탯 연결 (2026-10-04):
 *   걷기 속도 = BaseWalkSpeed x WalkSpeed 배율 x 중량 계수
 *   달리기 속도 = BaseRunSpeed x RunSpeed 배율 x 중량 계수
 *   점프 힘 = BaseJumpZVelocity x JumpSpeed 배율 (중량은 점프에 영향 없음)
 * 배율·중량·적재 한도는 메카 속성 세트(UknightAttributeSet)에서 매번 읽으므로,
 * 파츠 교체나 부위 파괴로 스탯이 바뀌면 바로 반영된다.
 *
 * 출발·정지 Easing (2026-10-09):
 *   출발: 땅 위의 가속 = MaxAcceleration x 출발 곡선(지금 속도 / 최고 속도) x 중량 계수.
 *         기본 곡선은 느리게 떼고(0.35배) 중간에 붙었다가(1.0배) 최고 속도 근처에서 부드럽게 닿는(0.6배) S자.
 *   정지: 방향키를 떼면 StopBrakingFriction·StopBrakingDeceleration으로 미끄러지며 멈춘다(속도가 줄수록 감속도 약해짐).
 *         이동 잠금(가드·퀵스텝 경직) 중에는 엔진 값(GroundFriction·BrakingDecelerationWalking)으로 지금처럼 빨리 멈춘다.
 *   중량 계수는 가속과 정지 감속에도 곱한다. 최고 속도도 같은 비율로 줄어서 출발·정지에 걸리는 시간은 가벼울 때와 같다.
 *   PIE에서 값 바꾸기·측정: knight.Player.Easing, knight.Player.MoveLog (knightDev 치트)
 */
UCLASS()
class KNIGHT_API UknightMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UknightMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 부스트 비행에 들어간다. 비행할 수 없으면(이동할 컴포넌트가 없거나 이미 비행 중) false. */
	UFUNCTION(BlueprintCallable, Category="Knight|Movement")
	bool StartFlight();

	/** 부스트 비행을 끝내고 낙하한다. */
	UFUNCTION(BlueprintCallable, Category="Knight|Movement")
	void StopFlight();

	UFUNCTION(BlueprintPure, Category="Knight|Movement")
	bool IsInFlight() const { return MovementMode == MOVE_Flying; }

	/** 비행 중 상하 입력을 더한다. Scale은 -1..1 (하강..상승). */
	void AddFlightVerticalInput(float Scale);

	/**
	 * 달리기 상태를 켜고 끈다. 지상 이동 속도가 걷기 대신 달리기 기준으로 바뀐다.
	 * 조작은 '누르고 있는 동안 달리기'(LeftShift, 캐릭터의 SetRunning이 부른다). 토글 설정은 2026-10-08에 뺐다.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Movement")
	void SetWantsToRun(bool bNewWantsToRun) { bWantsToRun = bNewWantsToRun; }

	UFUNCTION(BlueprintPure, Category="Knight|Movement")
	bool GetWantsToRun() const { return bWantsToRun; }

	/**
	 * (2026-10-13) 지금 실제로 달리는 중인지: 달리기 키를 누르고, 달리기·이동이 막히지 않았고, 땅 위에서 이동 입력이 있을 때.
	 * 달리기 키만 누르고 서 있거나 공중이면 아니다. 달리는 동안 행동 에너지 회복을 멈추는 데 쓴다(UknightAbilitySystemComponent).
	 */
	UFUNCTION(BlueprintPure, Category="Knight|Movement")
	bool IsRunning() const;

	/**
	 * 중량 계수. 적재율(중량 / 적재 한도)이 0이면 1.0, 1이면 MinWeightSpeedFactor.
	 * WeightSpeedCurve가 있으면 직선 대신 그 커브를 쓴다. 퀵스텝 등 다른 이동에서도 쓰도록 공개한다.
	 */
	UFUNCTION(BlueprintPure, Category="Knight|Movement")
	float GetWeightSpeedFactor() const;

	/**
	 * 퀵스텝 거리용 중량 계수 (2026-10-21). 적재율이 DodgeWeightStartRatio 이하면 1.0이고,
	 * 그 뒤 적재율 1에서 MinDodgeWeightFactor가 되도록 직선으로 줄어든다. 이동용 GetWeightSpeedFactor보다 늦게·약하게 걸린다.
	 */
	UFUNCTION(BlueprintPure, Category="Knight|Movement")
	float GetDodgeWeightFactor() const;

	/**
	 * 이동 입력으로 가속하지 못하게 막는다. 입력 값은 그대로 들어오므로 방향은 읽을 수 있고,
	 * 가속이 0이라 지금 속도에서 제동으로 미끄러지며 멈춘다(이동 방향으로 몸도 돌지 않는다). 퀵스텝 경직·가드에서 쓴다.
	 * (2026-10-08) 잠금 요청 수를 센다: true로 켠 쪽이 반드시 false로 한 번 끈다. 퀵스텝 경직 중 가드를 올리는 것처럼
	 * 겹쳐도 먼저 끝난 쪽이 다른 쪽의 잠금을 풀지 않게 하려는 것.
	 */
	void SetMoveInputLocked(bool bLocked) { MoveInputLockCount = FMath::Max(0, MoveInputLockCount + (bLocked ? 1 : -1)); }

	/** 이동 잠금 중인지. 잠금 중에는 정지에 엔진 값(빨리 멈춤)을 쓴다. */
	bool IsMoveInputLocked() const { return MoveInputLockCount > 0; }

	/**
	 * 달리기를 막는다 (2026-10-09, 수리 키트 사용 중). 달리기 키를 누르고 있어도 걷기 속도(줄지 않음)로 움직이고,
	 * 풀리면 키 상태대로 돌아간다. 이동 잠금처럼 요청 수를 센다: true로 켠 쪽이 반드시 false로 한 번 끈다.
	 */
	void SetRunLocked(bool bLocked) { RunLockCount = FMath::Max(0, RunLockCount + (bLocked ? 1 : -1)); }

	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;

protected:
	/** 착지 충격 (2026-10-12): 떨어지던 속도로 카메라를 흔들고, 세게 떨어지면 잠깐 발이 묶인다. */
	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;
	void HandleLandingImpact(float ImpactSpeed);
	void EndLandingRecovery();

	/** 착지할 때 카메라 흔들림. 비워 두면 흔들지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Movement|Landing")
	TSubclassOf<UCameraShakeBase> LandingShake;

	/** 이 속도(아래로) 이상으로 떨어져야 흔들린다. 제자리 점프 착지는 약 600. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Movement|Landing", meta=(ClampMin=0, Units="CentimetersPerSecond"))
	float LandingShakeMinSpeed = 450.f;

	/** 이 속도에서 흔들림이 최대(LandingShakeMaxScale)가 된다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Movement|Landing", meta=(ClampMin=1, Units="CentimetersPerSecond"))
	float LandingShakeMaxSpeed = 1600.f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Movement|Landing", meta=(ClampMin=0))
	float LandingShakeMinScale = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Movement|Landing", meta=(ClampMin=0))
	float LandingShakeMaxScale = 1.2f;

	/** 이 속도 이상으로 떨어지면 착지 경직: 잠깐 이동 입력으로 가속하지 못한다. 0이면 경직 없음. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Movement|Landing", meta=(ClampMin=0, Units="CentimetersPerSecond"))
	float LandingRecoveryMinSpeed = 1100.f;

	/** 착지 경직 시간. LandingRecoveryMinSpeed에서 0, LandingShakeMaxSpeed에서 이 값이 된다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Movement|Landing", meta=(ClampMin=0, Units="s"))
	float LandingRecoveryMaxTime = 0.3f;

	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;

	/** 지금 State.Sprinting을 붙일 상태인지: 달리기를 원하고, 땅 위에서, 걷기 최대 속도 x SprintTagSpeedRatio보다 빠르다. */
	bool ShouldHaveSprintTag() const;

	/** 달리기 태그가 붙는 속도. 걷기 최대 속도에 대한 비율이다. 1보다 조금 크게 둬서 걷기와 섞이지 않게 한다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Movement|Speed", meta=(ClampMin=0.5, ClampMax=2))
	float SprintTagSpeedRatio = 1.05f;
	virtual bool DoJump(bool bReplayingMoves, float DeltaTime) override;
	virtual void ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration) override;

	void SyncMovementTags() const;

	/** 메카 속성 값을 읽는다. 소유자에게 ASC나 해당 속성 세트가 없으면 Fallback을 돌려준다. */
	float GetMechAttributeValue(const FGameplayAttribute& Attribute, float Fallback) const;

	/** 적재율 = 중량 / 적재 한도 (0~1로 자름). 중량 계수 두 개가 같이 쓴다. */
	float GetLoadRatio() const;

	/**
	 * 배율 1.0일 때의 걷기 속도. 바뀔 수 있는 값.
	 * (2026-10-14) 600 → 540 (-10%): '게임이 너무 스피디하다'는 의견으로 시험. MaxAcceleration도 같이 낮춤(생성자, 1200 → 1100).
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Movement|Speed", meta=(ClampMin=0, Units="CentimetersPerSecond"))
	float BaseWalkSpeed = 540.f;

	/**
	 * 배율 1.0일 때의 달리기 속도. 무거운 메카 느낌을 위해 걷기의 1.5배로 시작한다. 바뀔 수 있는 값.
	 * (2026-10-14) 900 → 810 (-10%), 걷기와 같은 이유.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Movement|Speed", meta=(ClampMin=0, Units="CentimetersPerSecond"))
	float BaseRunSpeed = 810.f;

	/** 배율 1.0일 때의 점프 힘(위쪽 초기 속도). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Movement|Speed", meta=(ClampMin=0, Units="CentimetersPerSecond"))
	float BaseJumpZVelocity = 600.f;

	/** 적재 한도까지 꽉 채웠을 때의 중량 계수. 커브가 없을 때만 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Movement|Weight", meta=(ClampMin=0, ClampMax=1))
	float MinWeightSpeedFactor = 0.7f;

	/** 선택 사항. X = 적재율(0~1), Y = 중량 계수. 비워 두면 1.0 → MinWeightSpeedFactor 직선. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Movement|Weight")
	TObjectPtr<UCurveFloat> WeightSpeedCurve;

	/**
	 * (2026-10-21 사용자 결정) 퀵스텝 거리는 이동보다 늦게·약하게 무게를 탄다. 적재율이 이 값 이하면 퀵스텝 거리가 줄지 않는다.
	 * 이동과 같은 계수(0%부터 최대 30%)를 쓸 때 잔열 보통 풀세트(적재율 43%)로 2페이즈에 맞은 횟수가 기본 메카의 두 배였다.
	 * 50%면 보통 풀세트는 영향이 없고, 소켓까지 끼운 무거운 조합(67%)부터 조금 짧아진다. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Movement|Weight", meta=(ClampMin=0, ClampMax=0.99))
	float DodgeWeightStartRatio = 0.5f;

	/** 적재 한도까지 꽉 채웠을 때의 퀵스텝 거리 계수(0.85 = 15% 짧아짐). 바뀔 수 있는 값. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Movement|Weight", meta=(ClampMin=0, ClampMax=1))
	float MinDodgeWeightFactor = 0.85f;

	/**
	 * 출발 곡선. X = 지금 속도 / 최고 속도(0~1), Y = 가속 배율(MaxAcceleration에 곱함). 땅 위에서만 쓴다.
	 * 기본값(2026-10-09 사용자 결정 'S자, 조금 더 묵직하게', 바뀔 수 있는 값): 0 → 0.35, 0.5 → 1.0, 1.0 → 0.6 (직선 보간).
	 * 이때 최고 속도까지 걷기 약 0.73초, 달리기 약 1.09초.
	 * (2026-10-13 사용자 요청 '초반 움직임 더 느리게, 로봇 느낌') 0 → 0.15, 0.2 → 0.45 로 출발을 더 늦췄다. 나머지는 같다. 키를 모두 지우면 일정한 가속(배율 1)으로 돌아간다.
	 * (2026-10-14) 걷기·달리기 속도 -10%, MaxAcceleration 1200 → 1100(생성자). 위 곡선과 합쳐 최고 속도까지 원래보다 약 20% 더 걸린다:
	 * 걷기 약 0.89초, 달리기 약 1.33초. (MaxAcceleration 900으로 곡선 변경과 겹쳤을 때는 +50%라 1100으로 맞춤)
	 * 배율이 0이면 출발할 수 없으므로 0.05 아래로는 내려가지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Movement|Easing")
	FRuntimeFloatCurve StartAccelerationCurve;

	/**
	 * 방향키를 뗐을 때(달리기 → 걷기로 속도가 줄 때 포함)의 마찰. 속도에 비례하는 감속이라 멈추는 끝이 부드럽다. 클수록 빨리 멈춘다.
	 * 엔진이 BrakingFrictionFactor(기본 2)를 곱해서 쓴다.
	 * 기본값 2(바뀔 수 있는 값): StopBrakingDeceleration 600과 함께 걷기 약 0.40초·86cm, 달리기 약 0.48초·145cm에 멈춘다
	 * (걷기 600·달리기 900 기준. 10-14에 속도를 10% 낮춰서 지금은 조금 더 짧게 멈춘다).
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Movement|Easing", meta=(ClampMin=0))
	float StopBrakingFriction = 2.f;

	/** 방향키를 뗐을 때 마찰과 따로 더하는 일정한 감속(cm/s²). 0이면 느린 속도에서 오래 미끄러진다. 기본값 600(바뀔 수 있는 값). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Movement|Easing", meta=(ClampMin=0))
	float StopBrakingDeceleration = 600.f;

private:
	bool bWantsToRun = false;
	int32 MoveInputLockCount = 0;
	int32 RunLockCount = 0;
	bool bSprintTagged = false;
	bool bLandingRecovery = false;
	FTimerHandle LandingRecoveryTimer;
};
