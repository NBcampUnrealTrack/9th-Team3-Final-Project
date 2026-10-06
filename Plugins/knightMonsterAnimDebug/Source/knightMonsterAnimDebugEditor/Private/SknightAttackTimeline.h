#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class FknightAttackPreviewModel;

/**
 * 공격 미리보기 타임라인. 위에서부터 어빌리티 구간(예고·후딜 등, 빨간 선 = 판정), 몽타주(섹션 경계와 노티파이,
 * 판정 구간은 빨강), 더미 명중 표시, 눈금 순서로 그린다. 모든 가로축은 어빌리티 시간이다.
 * 클릭하거나 끌면 그 시간으로 이동한다.
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

private:
	void ScrubTo(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const;

	TSharedPtr<FknightAttackPreviewModel> Model;
};
