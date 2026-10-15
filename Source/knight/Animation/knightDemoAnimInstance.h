#pragma once

#include "CoreMinimal.h"
#include "Animation/knightAnimInstance.h"
#include "knightDemoAnimInstance.generated.h"

/**
 * (2026-10-15) EssentialAnimation 데모 애님 BP(AnimBP_EssentialGreatSword_UE4)를 기사 v2로 리타겟한 ABP_Knight_Demo의 부모.
 *
 * 데모 애님 BP는 BP_ThirdPersonCharacter로 캐스트해서 변수를 받는데, 우리 캐릭터는 그 클래스가 아니라 캐스트가 실패한다.
 * 그래서 데모 애님 BP의 변수(이름 그대로: IsInAir, Speed_X, Speed(Degree), isRun, isOneHanded, isBattleState, Shield_Blend 등)를
 * 여기서 이름으로 찾아 매 프레임 채운다. 데모 이벤트 그래프의 계산식(속도 각도, 보간 속도)을 그대로 옮겼다.
 *
 * 전투(공격·회피·구르기·피격)는 GAS 몽타주가 맡으므로 상태 머신의 그 상태들로 가는 변수(isAttack, isDodge, isRoll, isHurt 등)는
 * 항상 false로 둔다(사용자 결정: 애님 BP·이동은 데모 그대로, 전투 로직은 GAS 유지).
 *   isOneHanded   = 한손 스탠스 (State.Stance.TwoHanded 없음)
 *   isBattleState = 검을 든 전투 자세 (State.Combat)
 *   Shield_Blend  = 가드 중 (State.Guarding)이고 달리지 않을 때 1로 보간
 *   isRun         = 달리는 중 (UknightMovementComponent::IsRunning)
 *   isLocomotion  = 땅 위이고 몽타주가 없을 때
 */
UCLASS()
class KNIGHT_API UknightDemoAnimInstance : public UknightAnimInstance
{
	GENERATED_BODY()

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** 데모 이벤트 그래프의 Speed(Length) 보간 속도. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Demo")
	float SpeedLengthInterpSpeed = 20.f;

	/** 데모 이벤트 그래프의 Shield_Blend 보간 속도. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Demo")
	float ShieldBlendInterpSpeed = 10.f;

	/** LastSpeed(Degree)를 갱신하는 최소 속도 (데모 200). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Demo")
	float LastSpeedDegreeMinSpeed = 200.f;

	/**
	 * (2026-10-15) 재생 중이면 양손 스탠스여도 왼손 IK(bTwoHandedStance)를 끄는 몽타주.
	 * 패링 반격처럼 왼손이 칼날을 잡는 동작은 데모 애니메이션의 손 위치를 그대로 쓴다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Demo")
	TArray<TObjectPtr<UAnimMontage>> MontagesWithoutTwoHandIK;

private:
	void SetBoolVar(FName Name, bool bValue);
	void SetFloatVar(FName Name, double Value);
	double GetFloatVar(FName Name, double Fallback = 0.0) const;
};
