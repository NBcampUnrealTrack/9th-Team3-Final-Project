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
	 * 끄면(기본) 팔에 실린 가중치를 가장 가까운 몸통 쪽 뼈로 옮긴다 → 망토 옆면이 팔에 끌려가지 않는다(팔은 망토 밑으로 나온다).
	 * 나이트는 시작 뼈가 쇄골이라 가슴 뼈(spine_05)로 간다. 장갑·소매처럼 팔에 입는 파츠는 켠다.
	 */
	UPROPERTY(config, EditAnywhere, Category="Skin")
	bool bSkinFollowArms = false;

	/** 위와 같고 다리용. 끄면(기본) 다리 가중치를 골반 쪽 뼈로 옮긴다 → 치마가 허벅지에 들러붙지 않는다. 바지·다리 갑옷은 켠다. */
	UPROPERTY(config, EditAnywhere, Category="Skin")
	bool bSkinFollowLegs = false;

	/**
	 * 팔로 볼 뼈(이 뼈와 그 아래 전부). 기본값은 Config/DefaultEditorPerProjectUserSettings.ini (나이트: clavicle_l, clavicle_r).
	 * (10-13) 처음엔 upperarm이었는데, 그러면 팔 가중치가 쇄골로 가서 망토의 64%가 쇄골을 따라갔고, 팔을 들면 쇄골이 돌며
	 * 망토 옆면이 날개처럼 솟았다. 쇄골부터 빼서 가슴 뼈(spine_05)를 따라가게 했다.
	 */
	UPROPERTY(config, EditAnywhere, Category="Skin")
	TArray<FName> SkinArmRootBones;

	/** 다리로 볼 뼈(이 뼈와 그 아래 전부). 기본값은 위 ini (나이트: thigh_l, thigh_r → 다리 가중치는 pelvis로 간다). */
	UPROPERTY(config, EditAnywhere, Category="Skin")
	TArray<FName> SkinLegRootBones;

	/**
	 * (2026-10-13) '원본을 부위별로 나누기'에서 부위가 시작되는 뼈(이 뼈와 그 아래 전부). 삼각형마다 가장 많이 따라가는 부위로 나눈다.
	 * 어느 목록에도 안 걸리는 뼈(골반·척추·목 등)는 몸통이 된다. 기본값은 Config/DefaultEditorPerProjectUserSettings.ini
	 * (나이트: 머리 neck_01, 팔 clavicle_l / clavicle_r, 다리 thigh_l / thigh_r). 바꾸면 다시 나눠야 반영된다.
	 * 팔을 쇄골부터 잡은 이유: 풀암 파츠가 어깨까지 덮는 경우가 많아서. 가슴은 RegionArmSplitRatio로 몸통에 남긴다.
	 * (10-13 2차) 머리를 head로 잡았더니 머리 삼각형이 517개뿐이었다(나이트 머리는 head 뼈를 거의 따라가지 않음) → neck_01로.
	 * 그래도 얼굴이 가슴 뼈를 따라가는 부분은 RegionHeadCaptureRadius로 머리에 넣는다.
	 */
	UPROPERTY(config, EditAnywhere, Category="Body Regions")
	TArray<FName> RegionHeadRootBones;

	UPROPERTY(config, EditAnywhere, Category="Body Regions")
	TArray<FName> RegionArmLeftRootBones;

	UPROPERTY(config, EditAnywhere, Category="Body Regions")
	TArray<FName> RegionArmRightRootBones;

	UPROPERTY(config, EditAnywhere, Category="Body Regions")
	TArray<FName> RegionLegLeftRootBones;

	UPROPERTY(config, EditAnywhere, Category="Body Regions")
	TArray<FName> RegionLegRightRootBones;

	/**
	 * (2026-10-13 2차) 팔 시작 뼈(쇄골) 자체를 가장 많이 따라가는 삼각형은 그 뼈의 몸 쪽 관절 → 어깨 관절 사이에서
	 * 이 비율보다 몸 쪽이면 몸통에 남긴다. 기본 0.5.
	 * 나이트는 가슴판 절반이 쇄골을 따라가서, 0이면(가중치만으로 나누면) 팔을 숨길 때 가슴 절반이 같이 사라진다.
	 * 어깨가 덜 빠지면 낮추고(0.3), 가슴이 아직 빠지면 올린다(0.7). 바꾸면 다시 나눠야 반영된다.
	 */
	UPROPERTY(config, EditAnywhere, Category="Body Regions", meta=(ClampMin="0", ClampMax="1"))
	float RegionArmSplitRatio = 0.5f;

	/**
	 * (2026-10-13 3차) 위와 같고 다리(허벅지 뼈)용. 기본 0 = 자르지 않음.
	 * 허벅지는 뼈 전체가 다리라서, 0.5로 자르니 다리를 숨겨도 허벅지 위쪽이 몸통에 남았다(사용자 확인).
	 */
	UPROPERTY(config, EditAnywhere, Category="Body Regions", meta=(ClampMin="0", ClampMax="1"))
	float RegionLegSplitRatio = 0.f;

	/**
	 * (2026-10-13 3차) 몸통으로 나뉜 삼각형 중 머리 부위 관절의 가장 낮은 높이(나이트 head 169cm)보다 위이고,
	 * 머리 관절들을 지나는 수직축에서 이 반지름(cm) 안에 있으면 머리로 옮긴다. 0이면 끈다.
	 * 나이트는 얼굴·머리 위쪽이 가슴 뼈(spine_05, 삼각형 높이 162~190cm)를 따라가게 가중치가 잡혀 있어서
	 * 가중치만으로 나누면 머리를 숨겨도 얼굴이 남고 몸통을 숨겨야 사라졌다.
	 * (4~5차) 뼈 방향 대신 위쪽 고정(나이트는 neck_01이 head보다 위), 실제 반지름은 이 값과 머리 자체 폭
	 * (머리 뼈 삼각형 수평 거리 95% 지점, 나이트 8.3cm) 중 작은 쪽 → 머리 옆 목 가드는 몸통에 남는다. 이 값은 상한이다.
	 */
	UPROPERTY(config, EditAnywhere, Category="Body Regions", meta=(ClampMin="0", Units="cm"))
	float RegionHeadCaptureRadius = 20.f;

	/** (2026-10-13 3차) 위 규칙의 시작 높이: 머리 부위 관절의 가장 낮은 높이에서 이만큼(cm) 올라간 곳부터. 목 장식이 머리로 빠지면 올린다. */
	UPROPERTY(config, EditAnywhere, Category="Body Regions", meta=(Units="cm"))
	float RegionHeadCaptureOffset = 0.f;

	/**
	 * (2026-10-12) '뼈대에 입히기'를 할 때 천 시뮬레이션도 만든다 (망토·치마 자락이 흔들림).
	 * 파츠를 감싼 매끈한 천 계산용 메시(LOD1)로 천을 계산하고 원래 메시가 그걸 따라간다. 다시 구우면 천도 새로 만든다. 갑옷처럼 딱딱한 파츠는 끈다.
	 */
	UPROPERTY(config, EditAnywhere, Category="Cloth")
	bool bMakeCloth = false;

	/**
	 * (10-12) 켜면 천 계산용 메시에서 몸 쪽 면을 지워 바깥 한 장만 쓴다.
	 * (10-13) 기본 끔: 파츠를 감싸 부풀린 닫힌 껍데기를 그대로 쓰는 쪽이, 보이는 망토(안·밖 두 겹과 가시 장식)를 빈틈없이 덮어
	 * 점이 튀지 않는다. 한 장으로 자르면 구멍과 가장자리가 많이 생겨, 그 위의 보이는 점이 천 밖으로 크게 외삽되어 길게 튀었다
	 * (진단: 한 장 5천여 개 vs 닫힌 껍데기 40개).
	 */
	UPROPERTY(config, EditAnywhere, Category="Cloth", meta=(EditCondition="bMakeCloth"))
	bool bClothOuterLayerOnly = false;

	/**
	 * (10-12) 천 계산용 메시를 만들 때 파츠를 감싸는 복셀 칸 크기(cm). 3D 생성 도구로 만든 망토(두께 있는 껍데기 + 가시 장식)는
	 * 그대로 천으로 쓰면 조각조각 따로 떨어져서, 이 크기로 감싸고 원본과 함께 칸 1.5개만큼 부풀린 매끈한 껍데기를 천 계산에 쓴다
	 * (보이는 메시는 원본 그대로).
	 * 작을수록 원래 모양에 가깝지만 가는 틈·가시까지 따라간다. 크면 뭉툭해지고 망토와 몸 사이 틈이 메워질 수 있다.
	 */
	UPROPERTY(config, EditAnywhere, Category="Cloth", meta=(ClampMin="0.5", ClampMax="10", Units="cm", EditCondition="bMakeCloth"))
	float ClothProxyCellSize = 2.f;

	/** 천 계산용 메시의 정점 수. 많을수록 자연스럽지만 무겁다. 망토 하나에 2000~4000 정도. */
	UPROPERTY(config, EditAnywhere, Category="Cloth", meta=(ClampMin="300", ClampMax="20000", EditCondition="bMakeCloth"))
	int32 ClothSimVertexCount = 3000;

	/** 위에서부터 이 비율만큼은 고정(흔들리지 않음). 망토는 후드·어깨가 고정되도록 0.3 정도. */
	UPROPERTY(config, EditAnywhere, Category="Cloth", meta=(ClampMin="0", ClampMax="0.95", EditCondition="bMakeCloth"))
	float ClothFixedTopRatio = 0.3f;

	/**
	 * 맨 아래 자락이 원래 자리에서 벗어날 수 있는 최대 거리(cm). 고정 구간 아래부터 맨 아래까지 0 → 이 값으로 늘어난다.
	 * (10-12) 30 → 60으로 올렸었는데, 그땐 천이 실제로 돌지 않던 때였다. (10-13) 천이 돈 뒤 시험에서 40이 더 망토 같아 기본 40.
	 * 크게 하면 중력 방향으로 멀리 쏟아져 몸 앞을 덮거나 한쪽에 뭉친다.
	 */
	UPROPERTY(config, EditAnywhere, Category="Cloth", meta=(ClampMin="1", Units="cm", EditCondition="bMakeCloth"))
	float ClothMaxDistance = 40.f;

	/**
	 * (10-12) 몸의 움직임(속도·회전)을 천에 물려주는 비율 (Chaos 천 Linear/Angular Velocity Scale). 엔진 기본은 0.75.
	 * 낮을수록 자락이 늦게 따라와 휘날리고, 높을수록 몸에 붙어 같이 움직인다.
	 * (10-13) 시험 값 0.8을 기본으로: 낮으면 빠른 동작에서 천이 뒤처지며 망토가 깨져 보였다.
	 */
	UPROPERTY(config, EditAnywhere, Category="Cloth", meta=(ClampMin="0", ClampMax="1", EditCondition="bMakeCloth"))
	float ClothFollowBody = 0.8f;

	/**
	 * (10-12) 천이 접히는 것에 버티는 정도 (Chaos 천 Bending Stiffness). 엔진 기본은 1(뻣뻣함). 낮을수록 부드럽게 접힌다.
	 * (10-13) 시험 값 0.8을 기본으로: 낮으면 구겨져 덩어리로 뭉친다.
	 */
	UPROPERTY(config, EditAnywhere, Category="Cloth", meta=(ClampMin="0", ClampMax="1", EditCondition="bMakeCloth"))
	float ClothBendingStiffness = 0.8f;

	/**
	 * (10-13) 형태 유지 - 위쪽 (Chaos 천 Anim Drive Stiffness). 천을 원래 모양(뼈를 따라가는 스킨 모양) 쪽으로 당기는 힘.
	 * 0이면 진짜 천처럼 중력에 자유롭게 쏠리고, 1에 가까울수록 원래 모양을 지킨다. 게임 망토처럼 모양은 지키고 자락만 휘날리게 하려는 값.
	 * 높이에 따라 고정 구간(어깨·후드)에서 이 값 → 자락 끝에서 Cloth Shape Keep Hem 값으로 줄어든다.
	 * 기본 0.7 (Hem 0.3과 함께): 10-13 시험에서 0.4 / 0.05일 땐 역동적인 동작에서 망토가 깨졌고, 이 값에서 사라졌다.
	 */
	UPROPERTY(config, EditAnywhere, Category="Cloth", meta=(ClampMin="0", ClampMax="1", EditCondition="bMakeCloth"))
	float ClothShapeKeepTop = 0.7f;

	/** (10-13) 형태 유지 - 자락 끝. 낮을수록 자락 끝이 자유롭게 휘날린다. 위 값보다 작게 둔다. 기본 0.3 (위 설명 참고). */
	UPROPERTY(config, EditAnywhere, Category="Cloth", meta=(ClampMin="0", ClampMax="1", EditCondition="bMakeCloth"))
	float ClothShapeKeepHem = 0.3f;

	/** (10-13) 천에 거는 중력 배율 (Chaos 천 Gravity Scale). 1 = 실제 중력. 낮추면 덜 처지고 등 뒤에서 가볍게 날린다. */
	UPROPERTY(config, EditAnywhere, Category="Cloth", meta=(ClampMin="0", ClampMax="2", EditCondition="bMakeCloth"))
	float ClothGravityScale = 1.f;
};
