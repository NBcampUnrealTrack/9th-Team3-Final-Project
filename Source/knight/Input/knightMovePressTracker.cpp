#include "Input/knightMovePressTracker.h"

#include "GameFramework/Character.h"

void FknightMovePressTracker::Reset(const ACharacter& Character)
{
	bFreshPress = false;
	Baseline = GetLocalMoveInput(Character);
}

void FknightMovePressTracker::Update(const ACharacter& Character)
{
	const FVector2D Input = GetLocalMoveInput(Character);
	if (Input.IsNearlyZero())
	{
		// 키를 뗐다 → 다음에 들어오는 입력은 새 누름.
		Baseline = FVector2D::ZeroVector;
		return;
	}
	if (Baseline.IsNearlyZero())
	{
		// 뗐다가 다시 누름 (또는 시작 때 안 누르고 있다가 누름).
		bFreshPress = true;
		Baseline = Input;
		return;
	}
	// 누른 채로 다른 키를 더 누르거나 바꿈(W → W+D, W → D 등): 처음 방향에서 크게 바뀌면 새 누름.
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(FreshPressAngleDegrees));
	if (FVector2D::DotProduct(Input.GetSafeNormal(), Baseline.GetSafeNormal()) < MinDot)
	{
		bFreshPress = true;
	}
}

FVector2D FknightMovePressTracker::GetLocalMoveInput(const ACharacter& Character)
{
	// 월드 기준 입력을 컨트롤 회전(yaw) 기준으로 되돌린다. 행동 중 카메라를 돌려도 같은 키는 같은 방향으로 읽힌다.
	const FRotator YawRotation(0.f, Character.GetControlRotation().Yaw, 0.f);
	const FVector LocalInput = YawRotation.UnrotateVector(Character.GetLastMovementInputVector());
	return FVector2D(LocalInput.X, LocalInput.Y);
}
