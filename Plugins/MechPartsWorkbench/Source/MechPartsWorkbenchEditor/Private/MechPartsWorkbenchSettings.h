#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MechPartsWorkbenchSettings.generated.h"

class UAnimationAsset;
class USkeletalMesh;

/**
 * 워크벤치 미리보기 설정 (2026-10-10, 개인 설정).
 *
 * 사용법:
 *   - 워크벤치 창 오른쪽 아래 '미리보기 설정'에서 바꾼다. 바꾸면 미리보기가 바로 다시 그려진다.
 *     (Editor Preferences → Plugins → Mech Parts Workbench에도 같은 값이 보인다.)
 *   - 바꾼 값은 개인 설정 파일(Saved/Config/.../EditorPerProjectUserSettings.ini)에만 저장되어 팀원에게 퍼지지 않는다.
 *   - 프로젝트 기본값(나이트 메시)은 Config/DefaultEditorPerProjectUserSettings.ini에 있다.
 */
UCLASS(config=EditorPerProjectUserSettings, meta=(DisplayName="Mech Parts Workbench"))
class UMechPartsWorkbenchSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetContainerName() const override { return TEXT("Editor"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** 파츠를 붙여 볼 메카 스켈레탈 메시. 게임에서 파츠가 붙는 메시(나이트)와 같아야 위치가 맞는다. */
	UPROPERTY(config, EditAnywhere, Category="Preview")
	TSoftObjectPtr<USkeletalMesh> PreviewMechMesh;

	/** 미리보기 자세를 잡을 애니메이션. 비우면 메시 기본 자세. 움직이는 자세에서 파츠가 몸을 뚫는지 볼 때 쓴다. */
	UPROPERTY(config, EditAnywhere, Category="Preview")
	TSoftObjectPtr<UAnimationAsset> PreviewAnimation;

	/**
	 * (2026-10-11) 켜면 PreviewAnimation을 반복 재생한다(움직이는 동안 뚫림 확인).
	 * 끄면 PreviewAnimationTime 시점의 자세에서 멈춰 둔다(멈춘 자세로 미세 조정).
	 */
	UPROPERTY(config, EditAnywhere, Category="Preview")
	bool bPlayPreviewAnimation = true;

	/** 멈춘 자세로 볼 시점(초). Play Preview Animation을 껐을 때만 쓴다. 애니메이션 길이를 넘으면 마지막 프레임. */
	UPROPERTY(config, EditAnywhere, Category="Preview", meta=(ClampMin="0", Units="s", EditCondition="!bPlayPreviewAnimation"))
	float PreviewAnimationTime = 0.f;

	/**
	 * (2026-10-11) 카메라를 파츠에 맞출 때(파츠 선택·F 키) 최소 크기(cm). 투구처럼 작은 파츠도 이 크기 상자만큼
	 * 주변이 보이게 떨어져서 본다. 기본 50 = 카메라 약 0.5m 거리(창이 가로로 넓으면 조금 더 멀어짐).
	 * 처음엔 120(약 1m)이었는데 10-11에 써 보고 50으로 바꿨다. 머리·어깨까지 넓게 보려면 120 정도로 올린다.
	 */
	UPROPERTY(config, EditAnywhere, Category="Preview", meta=(ClampMin="10", Units="cm"))
	float FocusMinSize = 50.f;

	/**
	 * (2026-10-12) '뼈대에 입히기'에서 팔 뼈(SkinArmRootBones와 그 아래 전부)도 따라 휘게 할지.
	 * 끄면(기본) 팔에 실린 가중치를 가장 가까운 몸통 쪽 뼈(쇄골 등)로 옮긴다 → 망토 옆면이 팔에 끌려가지 않는다(팔은 망토 밑으로 나온다).
	 * 장갑·소매처럼 팔에 입는 파츠는 켠다.
	 */
	UPROPERTY(config, EditAnywhere, Category="Skin")
	bool bSkinFollowArms = false;

	/** 위와 같고 다리용. 끄면(기본) 다리 가중치를 골반 쪽 뼈로 옮긴다 → 치마가 허벅지에 들러붙지 않는다. 바지·다리 갑옷은 켠다. */
	UPROPERTY(config, EditAnywhere, Category="Skin")
	bool bSkinFollowLegs = false;

	/** 팔로 볼 뼈(이 뼈와 그 아래 전부). 기본값은 Config/DefaultEditorPerProjectUserSettings.ini (나이트: upperarm_l, upperarm_r). */
	UPROPERTY(config, EditAnywhere, Category="Skin")
	TArray<FName> SkinArmRootBones;

	/** 다리로 볼 뼈(이 뼈와 그 아래 전부). 기본값은 위 ini (나이트: thigh_l, thigh_r). */
	UPROPERTY(config, EditAnywhere, Category="Skin")
	TArray<FName> SkinLegRootBones;
};
