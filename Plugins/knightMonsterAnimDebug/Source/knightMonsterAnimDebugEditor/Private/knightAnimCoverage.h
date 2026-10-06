#pragma once

#include "CoreMinimal.h"

class UAnimMontage;
class UGameplayAbility;

enum class EknightAnimCoverageSeverity : uint8
{
	Ok,
	Warning,
	Error,
};

/** 커버리지 보드의 한 행: 몬스터 하나의 어빌리티 하나의 몽타주 슬롯 하나. 몬스터 자체(메시, AnimBP) 행은 Ability가 비어 있다. */
struct FknightAnimCoverageRow
{
	FString Monster;
	FString Ability;
	/** 어빌리티 종류(설정의 PlayableAbilityClasses 중 맞는 기반 클래스 이름). */
	FString Kind;
	/** 몽타주 프로퍼티 이름(Montage, HoldMontage...). 몬스터 행은 "메시/AnimBP". */
	FString Slot;
	FString MontageName;
	float MontageLength = 0.f;
	/** "예고 0.80 / 후딜 1.00". */
	FString Timing;
	/** 판정 노티파이 구간 "MeleeTrace 0.42-0.61", 없으면 빈 문자열. */
	FString HitNotify;

	EknightAnimCoverageSeverity Severity = EknightAnimCoverageSeverity::Ok;
	TArray<FString> Issues;

	/** 더블클릭하면 여는 에셋: 몽타주가 있으면 몽타주, 없으면 어빌리티나 몬스터 Blueprint. */
	TWeakObjectPtr<UObject> OpenTarget;

	/** 공격 미리보기로 열 때 쓴다. 몬스터 행은 AbilityClass가 비어 있다. */
	TWeakObjectPtr<UClass> MonsterClass;
	TWeakObjectPtr<UClass> AbilityClass;

	void AddIssue(EknightAnimCoverageSeverity InSeverity, const FString& Issue);
	FString GetIssueText() const { return FString::Join(Issues, TEXT(" / ")); }
};

struct FknightAnimCoverageResult
{
	TArray<TSharedPtr<FknightAnimCoverageRow>> Rows;
	int32 MonsterCount = 0;
	/** 데이터를 못 모았을 때의 안내(설정 오류, 에셋 레지스트리 로딩 중). */
	FString Notice;

	int32 CountSeverity(EknightAnimCoverageSeverity Severity) const;
};

namespace knightAnimCoverage
{
	/**
	 * 설정의 TrackedPawnClass를 상속한 몬스터 Blueprint를 모두 찾아 검사한다.
	 * 어빌리티는 몬스터 기본 객체에서 데이터 에셋(MonsterDefinition → AbilitySets ...)을 따라가며 찾은
	 * TSubclassOf<UGameplayAbility> 값이므로, 게임 모듈의 구조를 몰라도 된다.
	 */
	FknightAnimCoverageResult Gather();

	/** 표 전체와 애니메이터 할 일 목록을 Markdown으로 만든다. */
	FString ToMarkdown(const FknightAnimCoverageResult& Result);

	const TCHAR* SeverityLabel(EknightAnimCoverageSeverity Severity);
}
