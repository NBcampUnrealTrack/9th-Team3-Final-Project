#include "knightMonsterAttackSelectionSimulator.h"

#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "GameplayTagsManager.h"
#include "System/knightGameplayTags.h"

bool FknightMonsterAttackSelectionSimulator::Supports(const UClass* AbilityClass) const
{
	return AbilityClass && AbilityClass->IsChildOf(UknightGameplayAbility_MonsterAttack::StaticClass());
}

FknightSelectionResult FknightMonsterAttackSelectionSimulator::Evaluate(const UGameplayAbility& AbilityCDO, const FknightSelectionQuery& Query) const
{
	FknightSelectionResult Result;
	const UknightGameplayAbility_MonsterAttack* const Attack = Cast<UknightGameplayAbility_MonsterAttack>(&AbilityCDO);
	if (!Attack)
	{
		Result.Reason = TEXT("몬스터 공격이 아니다");
		return Result;
	}
	const FknightMonsterAttackSelection& Selection = Attack->GetSelection();

	// 시뮬레이터의 조건 검사: GetSelectionWeight와 같은 순서로 보되, 태그는 ASC 대신 Query.MonsterTags로 본다.
	// 이유를 보여주기 위한 것이고, 통과 여부의 최종 판단은 아래 게임 함수가 한다.
	if (Selection.Weight <= 0.f)
	{
		Result.Reason = TEXT("가중치 0");
	}
	else if (!Query.MonsterTags.HasAll(Selection.RequiredOwnerTags))
	{
		Result.Reason = FString::Printf(TEXT("필요 상태 없음 (%s)"), *Selection.RequiredOwnerTags.ToStringSimple());
	}
	else if (Query.MonsterTags.HasAny(Selection.BlockedOwnerTags))
	{
		Result.Reason = FString::Printf(TEXT("차단 상태 (%s)"), *Selection.BlockedOwnerTags.ToStringSimple());
	}
	else if (Query.Distance < Selection.MinRange)
	{
		Result.Reason = FString::Printf(TEXT("너무 가깝다 (%.0f < 최소 %.0f)"), Query.Distance, Selection.MinRange);
	}
	else if (Query.Distance > Selection.MaxRange)
	{
		Result.Reason = FString::Printf(TEXT("너무 멀다 (%.0f > 최대 %.0f)"), Query.Distance, Selection.MaxRange);
	}
	else if (FMath::Abs(Query.AngleToTarget) > Selection.MaxAngle)
	{
		Result.Reason = FString::Printf(TEXT("각도 밖 (%.0f도 > %.0f도)"), FMath::Abs(Query.AngleToTarget), Selection.MaxAngle);
	}
	else if (FMath::Abs(Query.AngleToTarget) < Selection.MinAngle)
	{
		Result.Reason = FString::Printf(TEXT("너무 정면 (%.0f도 < 최소 %.0f도)"), FMath::Abs(Query.AngleToTarget), Selection.MinAngle);
	}
	else if (Selection.TargetSide == EknightTargetSide::Left && Query.AngleToTarget > 0.f)
	{
		Result.Reason = TEXT("왼쪽 타깃 전용");
	}
	else if (Selection.TargetSide == EknightTargetSide::Right && Query.AngleToTarget < 0.f)
	{
		Result.Reason = TEXT("오른쪽 타깃 전용");
	}
	else if (Selection.AirborneRule == EknightTargetAirborneRule::OnlyAirborne && !Query.bTargetAirborne)
	{
		Result.Reason = TEXT("공중 타깃 전용");
	}
	else if (Selection.AirborneRule == EknightTargetAirborneRule::OnlyGrounded && Query.bTargetAirborne)
	{
		Result.Reason = TEXT("지상 타깃 전용");
	}

	// 게임 판정: 기본 객체는 발동한 적이 없어 쿨다운이 비어 있고, ASC가 없으니 태그 검사는 건너뛴다(위에서 대신 했다).
	FknightMonsterAttackContext Context;
	Context.MonsterASC = nullptr;
	Context.bHasTarget = true;
	Context.Distance = Query.Distance;
	Context.AngleToTarget = Query.AngleToTarget;
	Context.bTargetAirborne = Query.bTargetAirborne;
	Context.WorldTime = 0.0;
	const float GameWeight = Attack->GetSelectionWeight(Context);

	const bool bSimulatorPassed = Result.Reason.IsEmpty();
	if (bSimulatorPassed && GameWeight <= 0.f)
	{
		Result.bUnknownCondition = true;
		Result.Reason = TEXT("게임 판정이 거절했다: 시뮬레이터가 모르는 조건이 있다 (knightMonsterAttackSelectionSimulator 확인)");
	}
	Result.bAvailable = bSimulatorPassed && GameWeight > 0.f;
	Result.Weight = Result.bAvailable ? GameWeight : 0.f;
	return Result;
}

FknightSelectionShape FknightMonsterAttackSelectionSimulator::GetShape(const UGameplayAbility& AbilityCDO) const
{
	FknightSelectionShape Shape;
	const UknightGameplayAbility_MonsterAttack* const Attack = Cast<UknightGameplayAbility_MonsterAttack>(&AbilityCDO);
	if (!Attack)
	{
		return Shape;
	}
	const FknightMonsterAttackSelection& Selection = Attack->GetSelection();
	Shape.MinRange = Selection.MinRange;
	Shape.MaxRange = Selection.MaxRange;
	Shape.MaxAngle = Selection.MaxAngle;
	Shape.MinAngle = Selection.MinAngle;
	Shape.Side = Selection.TargetSide == EknightTargetSide::Left ? -1 : Selection.TargetSide == EknightTargetSide::Right ? 1 : 0;
	Shape.Weight = Selection.Weight;
	Shape.Cooldown = Selection.Cooldown;
	Shape.AirborneText = Selection.AirborneRule == EknightTargetAirborneRule::OnlyAirborne ? TEXT("공중 전용")
		: Selection.AirborneRule == EknightTargetAirborneRule::OnlyGrounded ? TEXT("지상 전용") : TEXT("");
	Shape.RequiredTagsText = Selection.RequiredOwnerTags.ToStringSimple();
	Shape.BlockedTagsText = Selection.BlockedOwnerTags.ToStringSimple();
	return Shape;
}

TArray<FGameplayTag> FknightMonsterAttackSelectionSimulator::GetSimulatedStateTags() const
{
	TArray<FGameplayTag> Tags = { knightGameplayTags::State_Monster_Enraged, knightGameplayTags::State_Monster_Exhausted };
	// 부위 파괴 태그(Monster.State.Broken.<몬스터>.<부위>)도 켜 보며 어떤 공격이 봉인되는지 본다.
	const FGameplayTag BrokenRoot = FGameplayTag::RequestGameplayTag(TEXT("Monster.State.Broken"), false);
	if (BrokenRoot.IsValid())
	{
		const FGameplayTagContainer BrokenTags = UGameplayTagsManager::Get().RequestGameplayTagChildren(BrokenRoot);
		for (const FGameplayTag& Tag : BrokenTags)
		{
			// 몬스터 단위 중간 태그(Monster.State.Broken.Janyeol)는 빼고 부위 태그만.
			const TSharedPtr<FGameplayTagNode> Node = UGameplayTagsManager::Get().FindTagNode(Tag);
			if (Node.IsValid() && Node->GetChildTagNodes().IsEmpty())
			{
				Tags.Add(Tag);
			}
		}
	}
	return Tags;
}

FName FknightMonsterAttackSelectionSimulator::GetSelectionPropertyName() const
{
	return TEXT("Selection");
}
