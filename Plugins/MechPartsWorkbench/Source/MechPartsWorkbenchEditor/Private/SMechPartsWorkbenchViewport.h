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
 * (10-13) 원본 부위 메시: 미리보기 메카 메시로 나눈 세트(UMechBodyRegionSet)가 있으면 게임과 똑같이 본체 대신 부위 메시를 그리고,
 *   보이는 파츠들의 Replace Regions에 든 부위를 숨긴다. 세트가 없으면 본체가 그대로 보인다(Replace Regions는 보이지 않음).
 * (10-13) 착용 미리보기: 고른 파츠 말고도 여러 파츠(SetLoadout)를 같이 붙이고, 원본을 통째로 숨길 수 있다(SetHideWholeMech).
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

	/**
	 * (10-12) 파츠 컴포넌트를 새로 만든다. 다시 굽기처럼 메시 에셋은 같은데 내용(천 데이터)이 바뀌었을 때 쓴다.
	 * 천 계산은 컴포넌트를 만들 때 준비되므로, 그대로 두면 예전 천으로 계속 보인다.
	 */
	void RecreatePart();

	/** 미리보기 설정(메카 메시·자세)을 다시 읽고 파츠도 다시 붙인다. */
	void RefreshMech();

	/** (10-13) 착용 미리보기로 같이 보여 줄 파츠들. 고른 파츠와 같은 부위인 것은 부르는 쪽이 빼고 넘긴다. */
	void SetLoadout(const TArray<UObject*>& InParts);

	/** (10-13) 착용 미리보기 파츠들의 값(메시·Attachment)이 바뀌었을 때 다시 붙인다. */
	void RefreshLoadout();

	/** (10-13) 원본 메카(본체·부위 메시)를 통째로 숨긴다. 파츠만으로 몸이 다 가려지는지 볼 때 쓴다. */
	void SetHideWholeMech(bool bHide);

	/** (10-13) 미리보기 메카 메시용 부위 메시 세트를 다시 찾아 붙인다. 나누기 직후·메카 메시가 바뀌었을 때. */
	void RefreshBodyRegions();

	/** (10-13) 지금 미리보기 메카에 부위 메시 세트가 붙어 있는지. 없으면 Replace Regions가 미리보기에 보이지 않는다. */
	bool HasBodyRegions() const { return !RegionComponents.IsEmpty(); }

protected:
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual void OnFocusViewportToSelection() override;

private:
	void RemovePartComponent();

	/** 파츠 메시(스켈레탈·스태틱)로 컴포넌트를 만들어 장면에 넣는다. 메시가 없거나 다른 종류면 null. */
	UMeshComponent* CreateMeshComponent(UObject* Mesh);

	/** (10-13) 보이는 파츠들의 Replace Regions와 '원본 전부 숨기기'로 본체·부위 메시를 켜고 끈다. */
	void UpdateMechVisibility();

	TSharedPtr<FAdvancedPreviewScene> PreviewScene;
	TSharedPtr<FMechPartsWorkbenchViewportClient> ViewportClient;

	/** 미리보기 장면에 넣은 컴포넌트. GC 참조는 PreviewScene이 잡고 있다. */
	TObjectPtr<USkeletalMeshComponent> MechComponent;
	TObjectPtr<UMeshComponent> PartComponent;

	TWeakObjectPtr<UObject> PartAsset;

	/** (10-13) 착용 미리보기 파츠 하나와 그 컴포넌트. */
	struct FLoadoutPart
	{
		TWeakObjectPtr<UObject> Asset;
		TObjectPtr<UMeshComponent> Component;
	};
	TArray<FLoadoutPart> LoadoutParts;

	/** (10-13) 원본 부위 메시 컴포넌트와, 같은 순서의 부위 비트(EMechBodyRegion). */
	TArray<TObjectPtr<USkeletalMeshComponent>> RegionComponents;
	TArray<int32> RegionFlags;

	bool bHideWholeMech = false;
};
