#pragma once

#include "CoreMinimal.h"

class USkeletalMesh;
class UStaticMesh;
struct FMechPartAttachment;

/**
 * 워크벤치 '뼈대에 입히기' (2026-10-12).
 *
 * 망토·옷처럼 여러 뼈에 걸친 스태틱 메시 파츠는 뼈 하나(예: spine_01)만 따라가서, 팔·다리가 움직이면 뚫거나
 * 뻣뻣하게 떠 보인다. 이 기능은 그 파츠를 메카 뼈대에 스킨을 입힌 스켈레탈 메시로 바꿔, 여러 뼈를 따라 휘게 한다.
 *
 * 순서:
 *   1. 파츠의 Attachment(뼈·소켓 + 위치·회전·크기)대로 메카 기본 자세 위에 스태틱 메시를 놓는다 (워크벤치에서 보던 자리).
 *   2. 메카 몸의 스킨 가중치를 파츠로 옮긴다. 겹쳐 입는 옷용 방식(InpaintWeights + LayeredMeshSupport):
 *      몸에 가까운 곳은 그대로 받고, 먼 곳(치마 끝 등)은 주변에서 부드럽게 채운다.
 *   3. (10-12) 따라가지 않을 뼈(ExcludedRootBones와 그 아래, 기본 = 팔·다리)의 가중치를 몸통 쪽 조상 뼈로 옮긴다.
 *      헐렁한 망토는 A 포즈에서 옆면·자락이 팔·허벅지에 더 가까워서, 그대로 두면 팔·다리에 끌려가 찢어진다.
 *   4. 원본 스태틱 메시 옆에 SK_<이름>으로 저장한다 (SM_X → SK_X, 이미 있으면 덮어쓴다).
 *   5. (10-12, 선택) 천 시뮬레이션: FOptions::bMakeCloth.
 *
 * 주의: 위치는 항상 메카 '기본 자세'(애니메이션 없음) 기준으로 굽는다. 애니메이션 자세에서 맞춘 위치는 기본 자세에서 다를 수 있다.
 * 다시 구우면(덮어쓰기) 예전 천 데이터는 먼저 지운다(모양이 바뀌면 예전 천은 맞지 않는다).
 */
namespace MechPartSkinBaker
{
	/** 굽기 옵션 (값은 워크벤치 미리보기 설정에서 온다). */
	struct FOptions
	{
		/** 따라가지 않을 뼈 (이 뼈와 그 아래). 가중치를 가장 가까운 조상 뼈로 옮긴다. */
		TArray<FName> ExcludedRootBones;

		/**
		 * (10-12) 천 시뮬레이션 만들기. 파츠를 감싼 매끈한 천 계산용 메시를 LOD1로 넣고, 그걸로 천 데이터를 만들어 LOD0에 연결한다.
		 * 흔들림 범위(MaxDistance): 위에서 FixedTopRatio만큼 0, 그 아래로 맨 아래까지 0 → MaxDistance.
		 * 몸과의 충돌은 메카 메시의 물리 에셋을 쓴다.
		 */
		bool bMakeCloth = false;
		/** 천 계산용 메시에서 몸 쪽을 향한 면을 빼고 바깥 한 장만 쓴다. (10-13) 기본 끔: 닫힌 껍데기가 보이는 망토를 더 잘 덮는다. */
		bool bClothOuterLayerOnly = false;
		/** (10-12) 천 계산용 메시를 만들 때 파츠를 감싸는 복셀 칸 크기(cm). 작을수록 원래 모양에 가깝지만 가는 틈·가시까지 따라간다. */
		float ClothProxyCellSize = 2.f;
		/**
		 * (10-12) 천이 충돌하지 않을 뼈 (이 뼈와 그 아래의 물리 바디). 팔을 따라가지 않는 망토는 팔을 넣는다:
		 * 기본 자세에서 팔이 망토 안에 있어서, 팔 바디와 충돌하면 팔을 움직일 때 천이 팔에 걸려 끌려간다.
		 * (10-12엔 날개처럼 솟는 원인이 이것이라고 봤는데, 실제 원인은 쇄골로 간 스킨 가중치였다.)
		 */
		TArray<FName> ClothNoCollisionRootBones;
		int32 ClothSimVertexCount = 3000;
		float ClothFixedTopRatio = 0.3f;
		/** (10-13) 기본 60 → 40: 천이 실제로 돌기 시작한 뒤 사용자 시험에서 40이 더 망토 같았다. */
		float ClothMaxDistance = 40.f;
		/** 몸 움직임을 천에 물려주는 비율 (Chaos Linear/Angular Velocity Scale). (10-13) 기본 0.3 → 0.8, 사용자 시험 값(빠른 동작에서 덜 뒤처짐). */
		float ClothFollowBody = 0.8f;
		/** 접힘 강도 (Chaos Bending Stiffness). (10-13) 기본 0.2 → 0.8, 사용자 시험 값(잘 안 접혀 덜 뭉친다). */
		float ClothBendingStiffness = 0.8f;
		/**
		 * (10-13) 형태 유지 (Chaos Anim Drive Stiffness): 천을 원래(스킨) 모양 쪽으로 당기는 힘. 0이면 진짜 천처럼 자유롭게 쏠리고,
		 * 1에 가까울수록 스킨 모양을 지킨다. 높이에 따라 Top(위·고정 구간) → Hem(자락 끝)으로 줄어든다.
		 * 기본 0.7 / 0.3: 사용자 시험(10-13)에서 역동적인 동작 때 망토가 깨지던 것이 이 값에서 사라졌다(0.4 / 0.05에선 깨짐).
		 */
		float ClothShapeKeepTop = 0.7f;
		float ClothShapeKeepHem = 0.3f;
		/** (10-13) 천에 거는 중력 배율 (Chaos Gravity Scale). 1보다 낮추면 덜 처지고 가볍게 날린다. */
		float ClothGravityScale = 1.f;
	};

	/** 만들 스켈레탈 메시의 패키지 경로. 원본 옆, 이름 SM_X → SK_X (SM_로 시작하지 않으면 SK_ + 이름). */
	FString GetSkinnedMeshPackagePath(const UStaticMesh* StaticMesh);

	/** 위 순서대로 스켈레탈 메시를 만들고 저장한다. 실패하면 null이고 OutError에 이유가 담긴다. */
	USkeletalMesh* Bake(UStaticMesh* PartMesh, const FMechPartAttachment& Attachment, USkeletalMesh* MechMesh,
		const FOptions& Options, FText& OutError);
}
