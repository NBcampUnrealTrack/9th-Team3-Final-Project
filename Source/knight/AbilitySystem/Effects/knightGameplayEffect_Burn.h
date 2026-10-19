#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "knightGameplayEffect_Burn.generated.h"

/**
 * 화상: 일정 시간 동안 주기마다 피해(Damage 메타 속성)를 준다. 방어력·가드 계산 없이 체력만 깎고, 지속 효과의 피해라
 * 피격 반응(움찔)은 일으키지 않는다(UknightHitReactionComponent). 다시 걸리면 겹치지 않고 남은 시간만 처음부터 센다.
 * 몬스터 영역 공격의 HitTargetEffect로 쓴다(잔열 브레스). 값은 블루프린트 하위 클래스에서 조정한다.
 */
UCLASS()
class KNIGHT_API UknightGameplayEffect_Burn : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UknightGameplayEffect_Burn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
