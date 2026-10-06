#pragma once

#include "CoreMinimal.h"
#include "knightAttackSelectionSimulator.h"

/**
 * 몬스터 공격 미리보기(knightMonsterAnimDebug 플러그인)의 '공격 선택' 패널에 게임의 선택 판정을 연결하는 어댑터.
 *
 * 판정은 게임 함수 UknightGameplayAbility_MonsterAttack::GetSelectionWeight를 그대로 부른다. 다만 미리보기에는 실제 몬스터가
 * 없어 ASC가 없으므로, 필요·차단 태그 검사만 이 어댑터가 Query.MonsterTags로 대신한다.
 *
 * 몬스터 공격 선택 조건(FknightMonsterAttackSelection, FknightMonsterAttackContext, GetSelectionWeight)을 바꾸면 이 파일도 확인한다.
 * 몬스터의 실제 상태(체력, 직전 공격 등)를 읽는 조건이 GetSelectionWeight에 추가되면, 미리보기는 그 행을 분홍색
 * ("시뮬레이터가 모르는 조건")으로 표시한다. 그 조건을 Evaluate에서 흉내 내도록 이 파일을 함께 고친다.
 */
class FknightMonsterAttackSelectionSimulator : public IknightAttackSelectionSimulator
{
public:
	virtual bool Supports(const UClass* AbilityClass) const override;
	virtual FknightSelectionResult Evaluate(const UGameplayAbility& AbilityCDO, const FknightSelectionQuery& Query) const override;
	virtual FknightSelectionShape GetShape(const UGameplayAbility& AbilityCDO) const override;
	virtual TArray<FGameplayTag> GetSimulatedStateTags() const override;
	virtual FName GetSelectionPropertyName() const override;
};
