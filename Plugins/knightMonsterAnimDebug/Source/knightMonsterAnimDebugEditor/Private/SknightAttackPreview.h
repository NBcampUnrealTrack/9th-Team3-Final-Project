#pragma once

#include "CoreMinimal.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"

class FknightAttackPreviewModel;
struct FPropertyChangedEvent;
class IDetailsView;
class SknightAttackPreviewViewport;
class SVerticalBox;

/**
 * 공격 미리보기 탭 (도구 > Knight > Monster Attack Preview, 또는 커버리지 보드 행 우클릭).
 *
 * 게임을 실행하지 않고 몬스터의 공격 몽타주를 재생·스크럽하며 근접 판정 궤적, 범위 공격 구체, 플레이어 더미 명중을 본다.
 * 공격 선택 시뮬레이터가 등록돼 있으면, 더미 위치와 몬스터 상태에서 어떤 공격이 어떤 확률로 뽑히는지와 공백 지도를 보여주고
 * 선택 조건(사거리, 각도, 가중치 등)을 바로 고칠 수 있다.
 * 타임라인의 구간 속도 줄에서 몽타주 구간별 재생 속도(완급)를 고치고, 어빌리티 판정 시각을 몽타주에 맞출 수 있다.
 * 왼쪽은 뷰포트와 타임라인, 오른쪽은 선택·재생·더미·공격 선택·표시 패널이다.
 */
class SknightAttackPreview : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SknightAttackPreview) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** 커버리지 보드에서 연다. AbilityClass가 없으면 몬스터만 고른다. */
	void SelectAbility(const UClass* MonsterClass, const UClass* AbilityClass);

private:
	TSharedRef<SWidget> MakeSidePanel();
	TSharedRef<SWidget> MakeSelectionSection();
	/** 구간 속도(고른 구간의 시작·끝·배율)와 판정 시각 맞추기. */
	TSharedRef<SWidget> MakeRateSection();
	void RebuildMonsterOptions();
	void RebuildAbilityOptions();
	void RebuildSelectionRows();
	/** 어빌리티 선택이 바뀐 뒤 상세 패널 대상을 맞춘다. */
	void OnAbilityChanged();
	/** 상세 패널에서 선택 조건을 고친 뒤 결과를 다시 계산한다. */
	void OnSelectionEdited(const FPropertyChangedEvent& Event);

	FText GetInfoText() const;

	TSharedPtr<FknightAttackPreviewModel> Model;
	TSharedPtr<SknightAttackPreviewViewport> Viewport;

	TArray<TSharedPtr<int32>> MonsterOptions;
	TArray<TSharedPtr<int32>> AbilityOptions;
	TSharedPtr<SComboBox<TSharedPtr<int32>>> MonsterCombo;
	TSharedPtr<SComboBox<TSharedPtr<int32>>> AbilityCombo;

	TSharedPtr<SVerticalBox> SelectionRowsBox;
	TSharedPtr<IDetailsView> SelectionDetails;
};
