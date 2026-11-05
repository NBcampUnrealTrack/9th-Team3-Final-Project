#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "knightPartShellBaker.generated.h"

class USkeletalMesh;
class UknightMonsterPartData;

/**
 * 부위 와이어프레임 셸 굽기.
 *
 * 몬스터 메시 LOD0에서 파괴 가능한 부위의 삼각형만 떼어 내 부위마다 단순화하고, 스켈레탈 메시 하나로 만든다.
 * 몬스터와 같은 뼈 가중치를 갖고 있어 런타임에 리더 포즈로 따라간다.
 *
 * 셸 메시 정점 데이터(머티리얼 M_PartShell이 읽는다):
 * - UV0.x: 부위 길이 축 위의 0~1 위치(몸 쪽 뿌리 0 → 끝 1). 흐르는 스캔 빛이 쓴다.
 * - UV0.y: 길이 축 둘레 위치(띠 단위). UV1.y와 함께 부위를 원통처럼 펼친 좌표(육각 같은 둘레 무늬용, 지금 셸 머티리얼은 쓰지 않음).
 * - UV1.x: 부위 번호(PartData.PartShellParts 인덱스). UV1.y: 길이 축 거리(띠 단위). 등고선이 정수마다 그어진다.
 * - 정점 색 RGB: 각 꼭짓점 맞은편 모서리가 꺾인 모서리면 1. 삼각형 무늬용(지금 셸 머티리얼은 쓰지 않음).
 * - 법선: 부드러운 법선. 머티리얼이 이 방향으로 셸을 몸 밖으로 살짝 띄운다.
 */
UCLASS()
class KNIGHTEDITOR_API UknightPartShellBaker : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * @param SourceMesh 몬스터 스켈레탈 메시.
	 * @param PartData 부위 데이터. PartShellMesh와 PartShellParts를 채우고 더티 표시한다(저장은 호출한 쪽).
	 * @param AssetPath 만들 셸 메시 경로(예: /Game/Knight/Creatures/Janyeol/Mesh/SK_Janyeol_PartShell). 있으면 덮어쓴다.
	 * @param TrianglesPerPart 부위 하나를 단순화할 삼각형 수. 적을수록 면이 크고 각지다.
	 * @param FeatureAngle 이웃 면과 이 각도(도) 이상 꺾인 모서리를 정점 색에 표시한다.
	 * @param BandsPerPart 평균 길이 부위에 들어갈 등고선 수. 간격은 모든 부위에 같다.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Editor")
	static USkeletalMesh* BakePartShell(USkeletalMesh* SourceMesh, UknightMonsterPartData* PartData, const FString& AssetPath,
		int32 TrianglesPerPart = 1200, float FeatureAngle = 22.f, int32 BandsPerPart = 8);
};
