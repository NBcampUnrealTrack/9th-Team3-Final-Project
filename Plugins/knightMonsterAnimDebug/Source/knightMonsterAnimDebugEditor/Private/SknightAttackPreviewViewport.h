#pragma once

#include "CoreMinimal.h"
#include "EditorViewportClient.h"
#include "SEditorViewport.h"

class FknightAttackPreviewModel;
class FPreviewScene;

/** 미리보기 뷰포트 클라이언트: 매 프레임 모델의 시간을 진행하고, 판정 궤적·구체·더미를 그린다. */
class FknightAttackPreviewViewportClient : public FEditorViewportClient
{
public:
	FknightAttackPreviewViewportClient(const TSharedRef<FknightAttackPreviewModel>& InModel, const TSharedRef<SEditorViewport>& InViewport);

	virtual void Tick(float DeltaSeconds) override;
	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;

	/** 몬스터 앞쪽을 비스듬히 내려다보는 시점으로 돌아간다. */
	void ResetCamera();

private:
	/** 강한 참조: 기반 클래스가 장면 포인터를 쓰므로, 클라이언트가 남아 있는 동안 장면(모델이 소유)도 남아야 한다. */
	TSharedRef<FknightAttackPreviewModel> Model;
};

class SknightAttackPreviewViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SknightAttackPreviewViewport) {}
		SLATE_ARGUMENT(TSharedPtr<FknightAttackPreviewModel>, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void ResetCamera();

protected:
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;

private:
	TSharedPtr<FknightAttackPreviewModel> Model;
	TSharedPtr<FknightAttackPreviewViewportClient> Client;
};
