// 원본 메카 메시를 부위별 스켈레탈 메시로 나누기 (2026-10-13)
//
// 워크벤치 목록 우클릭 '원본을 부위별로 나누기'가 부른다. 미리보기 메카 메시(예: Knight)를 읽어
// 삼각형마다 가장 많이 따라가는 뼈의 부위(머리·몸통·팔 L/R·다리 L/R)로 나누고, 원본 메시 폴더/BodyRegions에
//   SK_<원본>_<부위> (원본과 같은 뼈대·머티리얼) + DA_<원본>_BodyRegions (UMechBodyRegionSet)
// 를 저장한다. 이미 있으면 덮어쓴다. 원본 메시는 바꾸지 않는다.
// 부위 시작 뼈는 미리보기 설정의 Body Regions (RegionHeadRootBones 등). 어느 목록에도 없는 뼈는 몸통.

#pragma once

#include "CoreMinimal.h"

class UMechBodyRegionSet;
class USkeletalMesh;

namespace MechBodyRegionSplitter
{
	/** 부위별 시작 뼈. 이 뼈와 그 아래 전부가 그 부위. */
	struct FRootBones
	{
		TArray<FName> Head;
		TArray<FName> ArmLeft;
		TArray<FName> ArmRight;
		TArray<FName> LegLeft;
		TArray<FName> LegRight;

		/**
		 * (10-13 2차) 시작 뼈 자체를 가장 많이 따라가는 삼각형은, 그 뼈의 몸 쪽 끝(관절)→바깥 끝(가장 먼 자식 관절) 사이에서
		 * 이 비율보다 몸 쪽이면 부모 부위(보통 몸통)에 남긴다. 예: 쇄골 0.5 → 가슴판은 몸통, 어깨는 팔. 0이면 자르지 않는다.
		 * (10-13 3차) 팔·다리 따로. 다리는 허벅지 뼈 전체가 다리라서 자르면 허벅지 위쪽이 몸통에 남았다 → 기본 0.
		 */
		float ArmSplitRatio = 0.5f;
		float LegSplitRatio = 0.f;

		/**
		 * (10-13 3차) 몸통으로 나뉜 삼각형 중 머리 부위 관절의 가장 낮은 높이 + HeadCaptureOffset(cm)보다 위에 있고,
		 * 머리 관절들을 지나는 수직축에서 HeadCaptureRadius(cm) 안에 있으면 머리로 옮긴다. 0이면 끈다.
		 * 나이트는 얼굴·머리 위쪽이 가슴 뼈(spine_05)를 따라가게 가중치가 잡혀 있어서, 가중치만으로는 머리를 숨겨도 얼굴이 남았다.
		 * (4차) 뼈 방향은 쓰지 않고 위쪽 고정 — 나이트 뼈대는 neck_01이 head보다 위에 있다.
		 * (5차) 반지름은 머리 자체의 폭(머리 뼈를 가장 많이 따라가는 삼각형 수평 거리의 95% 지점)을 넘지 않는다 → 머리 옆 목 가드는 몸통에 남는다.
		 * (6차 시도, 되돌림) 시작 높이를 머리 삼각형 아래쪽 끝으로 내리면 더 어색해서, 머리 관절 높이부터로 둔다.
		 */
		float HeadCaptureRadius = 20.f;
		float HeadCaptureOffset = 0.f;
	};

	/** 결과를 저장할 폴더 = 원본 메시 폴더/BodyRegions. */
	FString GetOutputFolder(const USkeletalMesh* SourceMesh);

	/** 나눠서 저장하고 세트를 돌려준다. 실패하면 null과 OutError. 부위별 삼각형 수는 로그(LogMechBodyRegionSplitter)에 남긴다. */
	UMechBodyRegionSet* Split(USkeletalMesh* SourceMesh, const FRootBones& Roots, FText& OutError);

	/** SourceMesh로 만든 세트. 에셋 목록에서 찾고, 없으면 null. */
	UMechBodyRegionSet* FindRegionSet(const USkeletalMesh* SourceMesh);
}
