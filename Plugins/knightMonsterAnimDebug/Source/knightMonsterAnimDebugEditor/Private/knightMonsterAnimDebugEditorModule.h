#pragma once

#include "CoreMinimal.h"

namespace knightAnimDebugEditor
{
	/** 공격 미리보기 탭을 열고 몬스터와 어빌리티를 고른다. 커버리지 보드가 쓴다. */
	void OpenAttackPreview(const UClass* MonsterClass, const UClass* AbilityClass);
}
