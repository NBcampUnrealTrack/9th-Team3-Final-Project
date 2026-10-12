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
 *
 * 주의: 위치는 항상 메카 '기본 자세'(애니메이션 없음) 기준으로 굽는다. 애니메이션 자세에서 맞춘 위치는 기본 자세에서 다를 수 있다.
 * 한계: 자락이 흔들리지는 않는다(몸을 딱딱하게 따라감). 흔들림은 다음 단계의 천 시뮬레이션으로 붙인다.
 */
namespace MechPartSkinBaker
{
	/** 만들 스켈레탈 메시의 패키지 경로. 원본 옆, 이름 SM_X → SK_X (SM_로 시작하지 않으면 SK_ + 이름). */
	FString GetSkinnedMeshPackagePath(const UStaticMesh* StaticMesh);

	/** 위 순서대로 스켈레탈 메시를 만들고 저장한다. 실패하면 null이고 OutError에 이유가 담긴다. */
	USkeletalMesh* Bake(UStaticMesh* PartMesh, const FMechPartAttachment& Attachment, USkeletalMesh* MechMesh,
		const TArray<FName>& ExcludedRootBones, FText& OutError);
}
