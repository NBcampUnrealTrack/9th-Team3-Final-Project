#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "knightAnimCoverage.h"

/**
 * 몬스터 애니메이션 커버리지 보드 (도구 > Monster Anim Coverage).
 *
 * 몬스터 Blueprint × 어빌리티 × 몽타주 슬롯마다 한 행을 보여주고, 빠진 몽타주·스켈레톤 불일치·판정 노티파이 누락·
 * 길이와 판정 시각 불일치를 검사한다. 행을 더블클릭하면 몽타주(없으면 어빌리티나 몬스터 Blueprint)를 연다.
 * 결과는 Markdown으로 저장하거나 클립보드에 복사해 이슈나 애니메이터 할 일 목록으로 쓴다.
 */
class SknightAnimCoverageBoard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SknightAnimCoverageBoard) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void Refresh();
	void ApplyFilter();

	TSharedRef<ITableRow> GenerateRow(TSharedPtr<FknightAnimCoverageRow> Row, const TSharedRef<STableViewBase>& OwnerTable);
	void OnRowDoubleClicked(TSharedPtr<FknightAnimCoverageRow> Row);
	TSharedPtr<SWidget> OnContextMenuOpening();

	FReply OnRefreshClicked();
	FReply OnSaveClicked();
	FReply OnCopyClicked();

	FText GetSummaryText() const;

	FknightAnimCoverageResult Result;
	TArray<TSharedPtr<FknightAnimCoverageRow>> VisibleRows;
	TSharedPtr<SListView<TSharedPtr<FknightAnimCoverageRow>>> ListView;
	bool bProblemsOnly = false;
};
