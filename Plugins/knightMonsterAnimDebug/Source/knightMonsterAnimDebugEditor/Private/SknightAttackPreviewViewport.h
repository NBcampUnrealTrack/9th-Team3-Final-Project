#pragma once

#include "CoreMinimal.h"
#include "EditorViewportClient.h"
#include "SEditorViewport.h"
#include "Settings/EditorViewportSettings.h"

class FknightAttackPreviewModel;
class FPreviewScene;

/** 미리보기 카메라 시점. 모두 몬스터 메시 크기에 맞춰 거리를 정하므로 몬스터 크기가 바뀌어도 그대로 쓸 수 있다. */
enum class EknightPreviewCamera : uint8
{
	/** 몬스터 왼쪽 뒤에서 앞쪽 공격 범위를 비스듬히 내려다본다. */
	Overview,
	/** 바로 위에서 내려다본다(화면 위쪽이 몬스터 정면). 사거리·각도 확인용. */
	Top,
	/** 왼쪽 옆에서 수평으로 본다. 내려찍기 높이와 타이밍 확인용. */
	Side
};

/** 미리보기 뷰포트 클라이언트: 매 프레임 모델의 시간을 진행하고, 판정 궤적·구체·더미를 그린다. */
class FknightAttackPreviewViewportClient : public FEditorViewportClient
{
public:
	FknightAttackPreviewViewportClient(const TSharedRef<FknightAttackPreviewModel>& InModel, const TSharedRef<SEditorViewport>& InViewport);

	virtual void Tick(float DeltaSeconds) override;
	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;

	/** 지금 시점을 몬스터 크기에 맞춰 다시 잡는다. 몬스터(메시)가 바뀌면 자동으로 부른다. */
	void ResetCamera();
	void SetCameraPreset(EknightPreviewCamera Preset);
	EknightPreviewCamera GetCameraPreset() const { return Preset; }

private:
	/** 강한 참조: 기반 클래스가 장면 포인터를 쓰므로, 클라이언트가 남아 있는 동안 장면(모델이 소유)도 남아야 한다. */
	TSharedRef<FknightAttackPreviewModel> Model;
	EknightPreviewCamera Preset = EknightPreviewCamera::Overview;
	int32 FramedMeshRevision = INDEX_NONE;
	/** 처음 받은 카메라 이동 속도. 몬스터가 클수록 이 값에 배율을 곱한다. */
	FEditorViewportCameraSpeedSettings BaseSpeedSettings;
};

class SknightAttackPreviewViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SknightAttackPreviewViewport) {}
		SLATE_ARGUMENT(TSharedPtr<FknightAttackPreviewModel>, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void ResetCamera();
	void SetCameraPreset(EknightPreviewCamera Preset);
	EknightPreviewCamera GetCameraPreset() const;

protected:
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;

private:
	TSharedPtr<FknightAttackPreviewModel> Model;
	TSharedPtr<FknightAttackPreviewViewportClient> Client;
};
