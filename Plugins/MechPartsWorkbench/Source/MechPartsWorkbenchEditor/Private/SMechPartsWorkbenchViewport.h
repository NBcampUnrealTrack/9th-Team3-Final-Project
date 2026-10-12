#pragma once

#include "CoreMinimal.h"
#include "SEditorViewport.h"

class FAdvancedPreviewScene;
class FMechPartsWorkbenchViewportClient;
class UMeshComponent;
class USkeletalMeshComponent;

/**
 * 워크벤치 가운데 3D 미리보기 (2026-10-10).
 * 메카(미리보기 설정의 PreviewMechMesh)를 세우고, 선택한 파츠 메시를 파츠 에셋의 FMechPartAttachment대로 붙인다.
 * 붙이는 코드는 게임과 같은 FMechPartAttachment::Apply라서 여기서 보이는 위치 = 게임에서 장착한 위치.
 *
 * 조작: 마우스 오른쪽 드래그 + WASD로 둘러보기, F = 파츠(없으면 메카)에 카메라 맞추기.
 */
class SMechPartsWorkbenchViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SMechPartsWorkbenchViewport) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SMechPartsWorkbenchViewport() override;

	/** 보여 줄 파츠 에셋을 바꾼다. null이면 메카만 보인다. */
	void SetPartAsset(UObject* InPartAsset);

	/** 파츠 에셋 값(메시·Attachment)이 바뀌었을 때 다시 붙인다. 메시가 그대로면 위치만 다시 적용한다. */
	void RefreshPart();

	/** 미리보기 설정(메카 메시·자세)을 다시 읽고 파츠도 다시 붙인다. */
	void RefreshMech();

protected:
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual void OnFocusViewportToSelection() override;

private:
	void RemovePartComponent();

	TSharedPtr<FAdvancedPreviewScene> PreviewScene;
	TSharedPtr<FMechPartsWorkbenchViewportClient> ViewportClient;

	/** 미리보기 장면에 넣은 컴포넌트. GC 참조는 PreviewScene이 잡고 있다. */
	TObjectPtr<USkeletalMeshComponent> MechComponent;
	TObjectPtr<UMeshComponent> PartComponent;

	TWeakObjectPtr<UObject> PartAsset;
};
