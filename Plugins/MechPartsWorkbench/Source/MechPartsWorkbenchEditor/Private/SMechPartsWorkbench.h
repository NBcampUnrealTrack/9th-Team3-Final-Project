#pragma once

#include "CoreMinimal.h"
#include "ContentBrowserDelegates.h"
#include "MechPartAttachment.h"
#include "UObject/TopLevelAssetPath.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;
class SMechPartsWorkbenchViewport;
class SVerticalBox;
class UAnimationAsset;
class UPackage;
struct FAssetData;

/**
 * Mech Parts Workbench 창 (2026-10-10, 1a 단계: 목록 + 미리보기 + 숫자 조절).
 *
 * 사용법 (Tools → Mech Parts Workbench):
 *   1. 왼쪽 목록에서 파츠 에셋을 고른다. 목록 = FMechPartAttachment를 가진 에셋(파츠 정의·무기 정의).
 *      목록 위 분류 버튼(10-11)으로 부위(Slot Tag)·세트(에셋이 든 폴더 이름)별로 걸러 볼 수 있다.
 *      줄마다 하나씩 고르고, 여러 줄을 고르면 모두 맞는 파츠만 보인다. 버튼은 지금 있는 파츠의 값으로 자동으로 생긴다.
 *   2. 가운데에 메카 + 파츠가 게임과 같은 방식으로 붙어 보인다.
 *   3. 오른쪽 속성 패널의 Attachment(소켓, Relative Transform의 위치·회전·크기)를 바꾸면 바로 반영된다.
 *      메시(Mesh / StaticMesh)를 바꿔도 바로 반영된다. 되돌리기(Ctrl+Z)도 반영된다.
 *   4. '저장'을 누르면 파츠 에셋 파일이 저장된다 (바뀐 게 있을 때만 눌린다).
 *   5. (10-12) 망토·옷처럼 여러 뼈에 걸친 스태틱 메시 파츠는 위치를 맞춘 뒤 '뼈대에 입히기'를 누른다.
 *      원본 옆에 SK_ 스켈레탈 메시가 생기고, 파츠의 스켈레탈 메시 칸 = 그 메시, 붙이는 방식 = LeaderPose로 바뀐다.
 *      위치를 다시 맞추려면 스켈레탈 메시 칸(Mesh)을 비우면 스태틱 메시로 돌아온다 → 맞춘 뒤 다시 누른다(덮어씀).
 *      위치는 반드시 기본 자세(Preview Animation 비움)에서 맞춘다. 굽기는 기본 자세 기준이다.
 *      팔·다리를 따라갈지는 미리보기 설정의 Skin Follow Arms / Legs (기본 끔: 망토가 팔·다리에 끌려가지 않음).
 *   오른쪽 아래 '미리보기 설정'에서 메카 메시와 자세(애니메이션 반복 재생, 또는 멈춘 시점)를 바꿀 수 있다 (개인 설정).
 *   파츠를 고르거나 F를 누르면 카메라가 파츠에 맞춰진다. 작은 파츠도 최소 크기(FocusMinSize)만큼 떨어져서 본다.
 *   (10-13) 목록에서 파츠를 우클릭하면 저장, 바뀐 파츠 모두 저장, 뼈대에 입히기, 스태틱 메시로 되돌리기,
 *   배치 복사·붙여넣기, 미리보기 애니메이션 끄기·다시 켜기를 쓸 수 있다.
 *   저장 안 된 파츠가 있으면 목록 위에 개수와 '모두 저장' 버튼이 나온다.
 *   (10-13) 착용 미리보기(우클릭): 여러 파츠를 같이 끼워 본다. 부위(Slot Tag)마다 하나, 같은 부위를 끼우면 바뀐다.
 *   '이 세트 전부 끼기'는 같은 폴더의 파츠를 부위마다 하나씩(이름순 첫 번째 = A) 끼운다. 고른 파츠는 같은 부위의 착용 파츠 대신 보인다.
 *   '원본 전부 숨기기'로 파츠만 남겨 볼 수 있다.
 *   (10-13) 원본 부위 교체: 파츠 Attachment의 Replace Regions에 고른 부위는 그 파츠가 보이는 동안 원본에서 숨는다.
 *   먼저 우클릭 '원본을 부위별로 나누기'로 미리보기 메카 메시를 부위 메시로 나눠 둬야 한다(MechBodyRegionSplitter).
 */
class SMechPartsWorkbench : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMechPartsWorkbench) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SMechPartsWorkbench() override;

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	/** 목록 위 분류 줄 하나 (10-11). Property가 null이면 폴더(세트) 줄. Selected가 비어 있으면 '전체'. */
	struct FFilterRow
	{
		FText Label;
		const FProperty* Property = nullptr;
		TArray<FString> Values;
		TOptional<FString> Selected;
	};

	TSharedRef<SWidget> MakePartPicker();
	void HandlePartSelected(const FAssetData& AssetData);

	/** 파츠들의 분류 값을 다시 읽고 분류 버튼을 다시 만든다. 고른 값은 남아 있으면 유지한다. */
	void RebuildFilters();
	TSharedRef<SWidget> MakeFilterButton(int32 RowIndex, const TOptional<FString>& Value, const FText& Label);

	/** 목록에서 숨길 파츠면 true. 고른 분류 값과 하나라도 다르면 숨긴다. */
	bool ShouldHidePart(const FAssetData& AssetData) const;

	/** 파츠가 추가·삭제·이름 변경·저장되면 다음 프레임에 분류를 다시 만든다. */
	void MarkFiltersDirty(const FAssetData& AssetData);

	/** 속성 패널·되돌리기·다른 도구로 바뀐 파츠 에셋 값을 미리보기에 반영한다. */
	void HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event);

	/**
	 * 미리보기 설정이 바뀌면 개인 설정 파일에 저장하고 메카를 다시 그린다.
	 * (10-11) 설정 클래스(UDeveloperSettings)는 일반 속성 변경 알림을 보내지 않아서 전용 알림(OnSettingChanged)을 듣는다.
	 */
	void HandleSettingsChanged(UObject* Settings, FPropertyChangedEvent& Event);

	FReply HandleSaveClicked();
	bool CanSave() const;

	/** (10-12) '뼈대에 입히기': 스태틱 메시 파츠를 메카 뼈대를 따라 휘는 스켈레탈 메시로 바꾼다 (MechPartSkinBaker). */
	FReply HandleBakeSkinClicked();
	bool CanBakeSkin() const;

	/** (10-13) 목록 우클릭 메뉴. 고른 파츠에 대한 기능은 오른쪽 버튼과 같은 함수를 쓴다. */
	TSharedPtr<SWidget> MakePartContextMenu(const TArray<FAssetData>& SelectedAssets);

	/** (10-13) 불러온 파츠 에셋 중 저장 안 된 것들의 패키지. 목록 위 알림 줄과 '모두 저장'이 쓴다. */
	TArray<UPackage*> GetDirtyPartPackages() const;
	void SaveAllDirtyParts();

	/** (10-13) 스켈레탈 메시 칸을 비우고 붙이는 방식을 Socket으로 돌린다 → 스태틱 메시로 위치를 다시 맞출 수 있다. 되돌리기 가능. */
	void RevertToStaticMesh();
	bool CanRevertToStaticMesh() const;

	/** (10-13) 배치(소켓·위치·회전·크기) 복사·붙여넣기. 붙여넣기는 대상의 붙이는 방식(Socket/LeaderPose)은 그대로 둔다. 되돌리기 가능. */
	void CopyPlacement();
	void PastePlacement();

	/** (10-13) 미리보기 애니메이션을 비워 기본 자세로 보거나, 이 창에서 비웠던 애니메이션을 다시 넣는다. */
	void TogglePreviewAnimation();

	/** (10-13) 파츠의 부위 키(첫 번째 분류 속성 = Slot Tag 값). 값이 없으면 에셋 경로(다른 파츠와 겹치지 않음). */
	FString GetSlotKey(const FSoftObjectPath& PartPath) const;
	/** (10-13) 파츠의 세트(폴더) 이름. */
	FString GetFolderKey(const FSoftObjectPath& PartPath) const;

	/** (10-13) 착용 미리보기에 끼운다. 같은 부위에 있던 파츠는 빠진다. */
	void AddToLoadout(const FSoftObjectPath& PartPath);
	void RemoveFromLoadout(const FSoftObjectPath& PartPath);
	bool IsInLoadout(const FSoftObjectPath& PartPath) const;
	/** (10-13) 같은 폴더의 파츠를 부위마다 하나씩(이름순 첫 번째) 끼운다. */
	void AddFolderToLoadout(const FString& FolderKey);
	void ClearLoadout();
	/** (10-13) 착용 미리보기 파츠 중 고른 파츠와 같은 부위인 것을 빼고 미리보기에 넘긴다. 고르기·착용 변경 때마다 부른다. */
	void PushLoadoutToViewport();

	/** (10-13) 미리보기 메카 메시를 부위 메시로 나눈다(확인 창 → MechBodyRegionSplitter). */
	void SplitBodyRegions();

	/** (10-13) 목록 위 착용 미리보기 요약(부위·파츠 이름, 원본 숨김 여부). */
	FText GetLoadoutSummary() const;
	/** (10-13) 보이는 파츠에 Replace Regions가 있는데 부위 메시가 없어서 미리보기에 반영되지 않는지. */
	bool IsReplaceRegionsUnavailable() const;

	TSharedPtr<SMechPartsWorkbenchViewport> Viewport;
	TSharedPtr<IDetailsView> PartDetails;
	TSharedPtr<IDetailsView> SettingsDetails;
	TWeakObjectPtr<UObject> PartAsset;
	FDelegateHandle PropertyChangedHandle;
	FDelegateHandle SettingsChangedHandle;

	/** FMechPartAttachment를 가진 클래스들 (목록 필터). */
	TArray<FTopLevelAssetPath> PartClassPaths;
	TArray<FFilterRow> FilterRows;
	/** 파츠 에셋 → 분류 값 (FilterRows 순서). */
	TMap<FSoftObjectPath, TArray<FString>> PartGroupValues;
	TSharedPtr<SVerticalBox> FilterBox;
	FRefreshAssetViewDelegate RefreshPartList;
	bool bFiltersDirty = false;

	/** (10-13) 배치 복사 값과 어디서 복사했는지. 창을 닫으면 사라진다. */
	TOptional<FMechPartAttachment> CopiedPlacement;
	FString CopiedPlacementSource;

	/** (10-13) '미리보기 애니메이션 끄기'로 비운 애니메이션. '다시 켜기'에 쓴다. 창을 닫으면 사라진다. */
	TSoftObjectPtr<UAnimationAsset> ClearedPreviewAnimation;

	/** (10-13) 착용 미리보기 파츠들(끼운 순서). 부위마다 하나. 창을 닫으면 사라진다. */
	TArray<FSoftObjectPath> LoadoutParts;

	/** (10-13) '원본 전부 숨기기'. 창을 닫으면 꺼진다. */
	bool bHideWholeMech = false;
};
