#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "knightAnimNotifyState_MonsterTurn.generated.h"

/**
 * 몽타주의 이 구간 동안 몬스터가 타깃 쪽으로 몸을 돌린다. 연타 중 타격마다 방향을 다시 잡을 때 쓴다.
 *
 * YawOffset만큼 비켜서 바라본다. 정면이 아닌 쪽을 치는 타격이 타깃을 판정 안에 두게 하려는 것으로, 왼발처럼 왼쪽 앞을 덮는
 * 타격이면 +(오른쪽으로 몸을 틀어 타깃이 왼쪽 앞에 옴), 오른쪽 앞이면 -다. 공격 어빌리티의 StartTurnYaw와 같은 규칙이다.
 * 타깃이 움직여도 매 프레임 방향을 다시 잡는다. 발을 들어 올리는 동안에만 두고, 내려오기 시작하면 끝내야 착지 지점이 예고된다.
 * 몬스터 이동 컴포넌트의 회전 가속·감속과 잠금 태그를 그대로 따른다.
 */
UCLASS(meta=(DisplayName="Knight Monster Turn"))
class KNIGHT_API UknightAnimNotifyState_MonsterTurn : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	UPROPERTY(EditAnywhere, Category="Turn", meta=(ClampMin=-180, ClampMax=180, Units="deg"))
	float YawOffset = 0.f;

	UPROPERTY(EditAnywhere, Category="Turn", meta=(ClampMin=1, Units="DegreesPerSecond"))
	float TurnRate = 90.f;
};
