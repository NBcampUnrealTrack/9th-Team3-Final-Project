#pragma once

#include "CoreMinimal.h"
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
	 * 기본 조작은 '누르고 있는 동안 달리기'이고, 옵션에서 '토글'로 바꿀 수 있게 할 예정이다.
	 * (입력 키 연결은 키 매핑이 정해진 뒤에 캐릭터 쪽에서 한다)
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Movement")
	void SetWantsToRun(bool bNewWantsToRun) { bWantsToRun = bNewWantsToRun; }

	UFUNCTION(BlueprintPure, Category="Knight|Movement")
	bool GetWantsToRun() const { return bWantsToRun; }

	/**
	 * 중량 계수. 적재율(중량 / 적재 한도)이 0이면 1.0, 1이면 MinWeightSpeedFactor.
	 * WeightSpeedCurve가 있으면 직선 대신 그 커브를 쓴다. 퀵스텝 등 다른 이동에서도 쓰도록 공개한다.
	 */
	UFUNCTION(BlueprintPure, Category="Knight|Movement")
	float GetWeightSpeedFactor() const;

	/**
	 * 이동 입력으로 가속하지 못하게 막는다. 입력 값은 그대로 들어오므로 방향은 읽을 수 있고,
	 * 가속이 0이라 지금 속도에서 제동으로 미끄러지며 멈춘다. 퀵스텝 경직에서 쓴다.
	 */
	void SetMoveInputLocked(bool bLocked) { bMoveInputLocked = bLocked; }

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

	void SyncMovementTags() const;

	/** 메카 속성 값을 읽는다. 소유자에게 ASC나 해당 속성 세트가 없으면 Fallback을 돌려준다. */
	float GetMechAttributeValue(const FGameplayAttribute& Attribute, float Fallback) const;

	/** 배율 1.0일 때의 걷기 속도. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Movement|Speed", meta=(ClampMin=0, Units="CentimetersPerSecond"))
	float BaseWalkSpeed = 600.f;

	/** 배율 1.0일 때의 달리기 속도. 무거운 메카 느낌을 위해 걷기의 1.5배로 시작한다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Movement|Speed", meta=(ClampMin=0, Units="CentimetersPerSecond"))
	float BaseRunSpeed = 900.f;

	/** 배율 1.0일 때의 점프 힘(위쪽 초기 속도). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Movement|Speed", meta=(ClampMin=0, Units="CentimetersPerSecond"))
	float BaseJumpZVelocity = 600.f;

	/** 적재 한도까지 꽉 채웠을 때의 중량 계수. 커브가 없을 때만 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Movement|Weight", meta=(ClampMin=0, ClampMax=1))
	float MinWeightSpeedFactor = 0.7f;

	/** 선택 사항. X = 적재율(0~1), Y = 중량 계수. 비워 두면 1.0 → MinWeightSpeedFactor 직선. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Movement|Weight")
	TObjectPtr<UCurveFloat> WeightSpeedCurve;

private:
	bool bWantsToRun = false;
	bool bMoveInputLocked = false;
	bool bSprintTagged = false;
	bool bLandingRecovery = false;
	FTimerHandle LandingRecoveryTimer;
};
