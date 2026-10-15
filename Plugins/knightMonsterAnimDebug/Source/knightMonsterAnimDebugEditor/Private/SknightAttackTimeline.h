#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "knightAnimAbilityInfo.h"

class FknightAttackPreviewModel;

/**
 * 공격 미리보기 타임라인. 위에서부터 어빌리티 구간(예고·후딜 등, 빨간 선 = 판정), 몽타주(섹션 경계와 노티파이,
 * 판정 구간은 빨강), 구간 속도, 더미 명중 표시, 눈금 순서로 그린다. 모든 가로축은 어빌리티 시간(실제 초)이다.
 * 클릭하거나 끌면 그 시간으로 이동한다.
 *
 * 구간 속도 줄: 빈 곳을 끌면 구간을 만들고, 구간 안을 끌면 옮기고, 양 끝을 끌면 길이를 바꾼다. 휠로 배율을 바꾸고
 * (Shift는 크게), 우클릭으로 배율 프리셋과 삭제 메뉴를 연다. 끄는 동안에는 끌기 시작할 때의 시간축을 그대로 써서
 * 구간 끝이 마우스를 따라오게 한다(구간이 바뀌면 시간축 자체가 늘거나 줄기 때문이다).
 */
class SknightAttackTimeline : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SknightAttackTimeline) {}
		SLATE_ARGUMENT(TSharedPtr<FknightAttackPreviewModel>, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

private:
	enum class EDrag : uint8
	{
		None,
		Scrub,
		/** 빈 곳을 눌렀다. 조금 움직이면 새 구간을 만든다. */
		Create,
		Move,
		ResizeStart,
		ResizeEnd
	};

	/** 마우스가 가리키는 구간 속도와 그 부분. */
	struct FRateHit
	{
		FGuid Guid;
		EDrag Part = EDrag::None;
	};

	const FknightMontageTimeMap& GetPaintMap() const;
	float GetPaintDuration() const;
	float LocalXToRealTime(const FGeometry& MyGeometry, float LocalX) const;
	bool IsInRateRow(const FGeometry& MyGeometry, const FVector2f& Local) const;
	FRateHit HitTestRate(const FGeometry& MyGeometry, const FVector2f& Local) const;
	void ScrubTo(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const;
	void UpdateRateDrag(const FGeometry& MyGeometry, const FVector2f& Local);
	void FinishDrag();
	void OpenRateMenu(const FPointerEvent& MouseEvent, const FGuid& Guid);

	TSharedPtr<FknightAttackPreviewModel> Model;

	EDrag Drag = EDrag::None;
	FGuid DragGuid;
	/** 끌기 시작 시점의 시간축(실제 초 ↔ 몽타주 시간)과 전체 길이. 끄는 동안 그리기와 마우스 변환에 쓴다. */
	FknightMontageTimeMap DragMap;
	float DragDuration = 1.f;
	float DragStartX = 0.f;
	float DragAnchor = 0.f;
	float DragOriginalStart = 0.f;
	float DragOriginalEnd = 0.f;
};
