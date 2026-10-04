#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UGameplayAbility;

/** 시뮬레이터에 넘기는 상황. 게임의 공격 선택이 실제 몬스터와 타깃에서 읽는 값을 미리보기의 더미로 대신한다. */
struct FknightSelectionQuery
{
	/** 몬스터 원점과 타깃 원점 사이의 수평 거리(cm). */
	float Distance = 0.f;
	/** 몬스터 정면 기준 타깃의 yaw(도, 오른쪽 +). */
	float AngleToTarget = 0.f;
	bool bTargetAirborne = false;
	/** 몬스터에 붙어 있다고 가정할 상태 태그(분노, 탈진 등). */
	FGameplayTagContainer MonsterTags;
};

struct FknightSelectionResult
{
	/** 이 공격을 고를 수 있는지. */
	bool bAvailable = false;
	/** 고를 수 있을 때의 가중치. 확률은 사용 가능한 공격들의 가중치 비율이다. */
	float Weight = 0.f;
	/** 고를 수 없는 이유, 또는 시뮬레이터가 흉내 내지 못한 조건에 대한 안내. */
	FString Reason;
	/** 시뮬레이터의 조건 검사로는 통과인데 게임 판정 함수가 거절했다: 시뮬레이터가 모르는 조건이 생겼다는 신호다. */
	bool bUnknownCondition = false;
};

/** 바닥 표시용 선택 조건. */
struct FknightSelectionShape
{
	float MinRange = 0.f;
	float MaxRange = 0.f;
	/** 정면 기준 좌우 최대 각도(도). 180이면 전 방향. */
	float MaxAngle = 180.f;
	float Weight = 0.f;
	float Cooldown = 0.f;
	/** "공중 전용", "지상 전용" 같은 짧은 설명. 제한이 없으면 빈 문자열. */
	FString AirborneText;
	FString RequiredTagsText;
	FString BlockedTagsText;
};

/**
 * 몬스터 공격 선택 시뮬레이터. 플러그인은 게임 코드를 모르므로, 게임 쪽 에디터 모듈(knightEditor)이 이 인터페이스를
 * 구현해 게임의 실제 판정 함수를 불러 주고 RegisterAttackSelectionSimulator로 등록한다. 등록된 것이 없으면
 * 미리보기의 '공격 선택' 패널은 안내만 보여준다.
 */
class IknightAttackSelectionSimulator
{
public:
	virtual ~IknightAttackSelectionSimulator() = default;

	/** 이 어빌리티 클래스가 선택 조건을 가진 공격인지. */
	virtual bool Supports(const UClass* AbilityClass) const = 0;

	/** 상황에서 이 공격을 고를 수 있는지, 가중치와 이유. */
	virtual FknightSelectionResult Evaluate(const UGameplayAbility& AbilityCDO, const FknightSelectionQuery& Query) const = 0;

	virtual FknightSelectionShape GetShape(const UGameplayAbility& AbilityCDO) const = 0;

	/** 미리보기 패널에 토글로 보여줄 몬스터 상태 태그(분노, 탈진 등). */
	virtual TArray<FGameplayTag> GetSimulatedStateTags() const = 0;

	/** 상세 패널에서 편집할 어빌리티 프로퍼티 이름(선택 조건 구조체). */
	virtual FName GetSelectionPropertyName() const = 0;
};

namespace knightAnimDebugEditor
{
	KNIGHTMONSTERANIMDEBUGEDITOR_API void RegisterAttackSelectionSimulator(const TSharedPtr<IknightAttackSelectionSimulator>& Simulator);
	KNIGHTMONSTERANIMDEBUGEDITOR_API void UnregisterAttackSelectionSimulator(const TSharedPtr<IknightAttackSelectionSimulator>& Simulator);
	KNIGHTMONSTERANIMDEBUGEDITOR_API TSharedPtr<IknightAttackSelectionSimulator> GetAttackSelectionSimulator();
}
