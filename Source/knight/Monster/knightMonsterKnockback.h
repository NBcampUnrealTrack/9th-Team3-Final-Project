#pragma once

#include "CoreMinimal.h"

/**
 * 몬스터 돌진·몸통 박치기에 맞은 대상을 몬스터 진행 경로 밖(대상이 있던 쪽 옆)으로 날려 보낸다.
 * 앞쪽 성분을 크게 주면 대상이 달려오는 머리와 같은 속도로 밀려 몸 앞에 붙어 끌려가 보이므로 옆으로 주로 민다.
 * 세기가 약해도(가드) 몬스터 몸 옆(두 캡슐 반지름 + SideMargin)까지는 밀어 낸다. 한쪽이 벽에 막히면 반대쪽으로
 * 한 번 바꾸고, 남은 시간 안에 경로를 건너 반대쪽 몸 옆까지 가도록 옆 속도를 올린다.
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
	TWeakObjectPtr<const AActor> SourceMonster;
	TWeakObjectPtr<AActor> Actor;
	FVector Velocity = FVector::ZeroVector;
	FVector Side = FVector::ZeroVector;
	/** 몬스터 중심선에서 대상이 벗어나야 하는 옆 거리. */
	float Clearance = 0.f;
	float Remaining = 0.f;
	bool bFlipped = false;
};
