#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTagContainer.h"
#include "knightMonsterMovementComponent.generated.h"

class UAbilitySystemComponent;

/**
 * 거대 몬스터용 이동 컴포넌트. 움직임 대부분은 공격·이동 몽타주의 루트 모션에서 나오고,
 * 이 컴포넌트는 나머지에 쓸 느리고 묵직한 기본값과 AI용 회전 헬퍼를 제공한다.
 *
 * 몬스터에게 LockTags 중 하나라도 있으면(다운, 경직, 포효, 사망) StateTree가 무엇을 요청하든 걷지도
 * 돌지도 않는다. 비틀거리는 몬스터는 플레이어를 따라 돌지 않고 제자리에서 휘청인다. 반응 몽타주의
 * 루트 모션은 그대로 재생된다.
 *
 * 캡슐은 작게 유지하고(AknightMonsterCharacterBase 참고) 몸체 충돌은 피직스 에셋으로 부위마다 처리한다.
 * NavAgentProps는 몬스터 크기 등급에 맞는 내비게이션 에이전트로 설정한다.
 */
UCLASS()
class KNIGHT_API UknightMonsterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UknightMonsterMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * 이번 프레임에 소유자를 WorldLocation 쪽으로 초당 최대 TurnRate도만큼 회전시킨다.
	 * TurnAcceleration이 있으면 회전 속도를 서서히 올리고, 목표 각도에 가까워지면 서서히 줄여 멈춘다(큰 몸이 툭 돌았다 툭 서지 않게).
	 * ToleranceDegrees 이내로 바라보면 true를 반환한다. 그때 남은 회전 속도는 TickComponent가 관성으로 줄여 멈춘다
	 * (움직이는 타깃을 따라 도는 중에도 허용 각도에만 들어오면 끝나므로, 회전이 끝나지 않는 일이 없다). 잠긴 동안에는 아무것도 하지 않고 false를 반환한다.
	 * 몽타주가 있는 회전이라면 이 함수 대신 몽타주를 재생한다.
	 */
	bool TurnTowards(const FVector& WorldLocation, float DeltaTime, float TurnRate, float ToleranceDegrees);

	/** 소유자에게 LockTags 중 하나라도 있으면 true. */
	bool IsMovementLocked() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual float GetMaxSpeed() const override;
	virtual void PhysicsRotation(float DeltaTime) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category="Knight|Monster")
	FGameplayTagContainer LockTags;

	/** TurnTowards의 회전 가속·감속(도/초²). 0이면 처음부터 TurnRate로 돌고 바로 멈춘다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Knight|Monster", meta=(ClampMin=0))
	float TurnAcceleration = 480.f;

private:
	mutable TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	/** 지금 회전 속도(도/초, 부호 = 방향). TurnTowards가 회전을 끝냈거나 더 불리지 않으면 TickComponent가 0까지 줄인다. */
	float TurnSpeed = 0.f;
	double LastTurnTime = -1.0;
	/** TurnTowards가 허용 각도에 들어와 회전을 넘겼다. 남은 속도는 관성으로 감속한다. */
	bool bCoastingTurn = false;
};
