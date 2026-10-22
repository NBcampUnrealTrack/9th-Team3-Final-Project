#pragma once

#include "CoreMinimal.h"

/**
 * 몬스터 돌진·몸통 박치기에 맞은 대상을 몬스터 진행 경로 밖(대상이 있던 쪽 옆, 정면에서 약 63도)으로 날려 보낸다.
 * 몬스터가 계속 앞으로 오므로 앞쪽 성분만으로는 다시 몸에 걸린다. 한쪽이 벽에 막히면 반대쪽으로 한 번 바꾼다.
 *
 * 플레이어 피격 몽타주의 루트 모션이 캐릭터 무브먼트의 속도(LaunchCharacter, 루트 모션 소스)보다 우선하므로
 * 위치를 직접 옮긴다. 쓰는 쪽이 Start 뒤 매 틱 Tick을 부른다.
 */
struct FknightMonsterKnockback
{
	void Start(const AActor& Monster, AActor& Target, float Speed, float Time);
	void Tick(float DeltaTime);
	void Reset() { Remaining = 0.f; }

private:
	TWeakObjectPtr<AActor> Actor;
	FVector Velocity = FVector::ZeroVector;
	FVector Side = FVector::ZeroVector;
	float Remaining = 0.f;
	bool bFlipped = false;
};
