#pragma once

#include "CoreMinimal.h"

class ACharacter;

/**
 * '새로 누른' 이동 입력 판정. (공격 후딜 이동 캔슬(knightGameplayAbility_MeleeAttack)에서 꺼내 가드와 같이 쓴다)
 *
 * 사용법:
 *   1. 행동을 시작할 때 Reset(캐릭터) — 그때 누르고 있던 방향은 '새 누름'이 아니다.
 *   2. 짧은 간격으로 Update(캐릭터) — 입력을 읽어 새 누름이면 기록한다.
 *   3. HasFreshPress()로 확인한다. 한 번 기록되면 다음 Reset 전까지 유지된다.
 *
 * 새 누름: 키를 뗐다가 다시 누름(또는 시작 때 안 누르고 있다가 누름),
 *          누른 채로 다른 키를 더하거나 바꿔 처음 방향에서 FreshPressAngleDegrees보다 크게 바뀜(W → W+D 등).
 * 이동 입력은 컨트롤 회전(yaw) 기준으로 읽어서, 행동 중 카메라를 돌려도 같은 키는 같은 방향으로 본다.
 */
struct KNIGHT_API FknightMovePressTracker
{
	/** 누른 채로 방향이 이 각도보다 많이 바뀌면 새 누름으로 본다. W → W+D가 45도라 그보다 작게 둔다. */
	static constexpr float FreshPressAngleDegrees = 30.f;

	/** 지금 누르고 있는 방향을 기준으로 삼고 새 누름 기록을 지운다. */
	void Reset(const ACharacter& Character);

	/** 이동 입력을 읽어 새 누름이면 기록한다. */
	void Update(const ACharacter& Character);

	bool HasFreshPress() const { return bFreshPress; }

	/** 이동 입력을 컨트롤 회전 기준 2D 방향으로 읽는다. 입력이 없으면 영벡터. */
	static FVector2D GetLocalMoveInput(const ACharacter& Character);

private:
	/** 새 누름 판정 기준 방향: 시작 때 누르고 있던 방향. 키를 떼면 0이 된다. */
	FVector2D Baseline = FVector2D::ZeroVector;

	bool bFreshPress = false;
};
