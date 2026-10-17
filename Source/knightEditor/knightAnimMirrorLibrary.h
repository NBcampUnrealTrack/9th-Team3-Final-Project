#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "knightAnimMirrorLibrary.generated.h"

class UAnimSequence;
class UMirrorDataTable;
class USkeleton;

/**
 * 좌우를 뒤집은 애니메이션을 별도 시퀀스 에셋으로 굽는 도구. 런타임 Mirror 노드와 같은 계산(FAnimationRuntime::MirrorPose)을
 * 프레임마다 적용해 저장하므로, 결과는 루트 모션까지 뒤집힌 평범한 시퀀스가 되고 ABP를 건드릴 필요가 없다.
 * Python에서 부른다(unreal.knightAnimMirrorLibrary). 저장은 호출한 쪽.
 */
UCLASS()
class KNIGHTEDITOR_API UknightAnimMirrorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 스켈레톤의 미러 데이터 테이블을 만들거나 다시 채운다. 뼈 짝은 프로젝트 설정(Animation > Mirroring)의 이름 규칙(_L/_R 등)으로 찾고,
	 * 짝이 없는 가운데 뼈는 자기 자신과 짝짓는다. 축은 X(좌우). 짝을 못 찾은 뼈가 있으면 경고를 남긴다.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Anim Mirror")
	static UMirrorDataTable* BuildMirrorDataTable(const FString& PackagePath, USkeleton* Skeleton);

	/**
	 * Source를 좌우로 뒤집어 PackagePath에 굽는다. 처음이면 Source를 복제해 설정(루트 모션, 압축 등)을 그대로 가져가고,
	 * 이미 있으면 그 에셋의 뼈 트랙만 다시 써서 직접 단 노티파이를 남긴다. 커브 이름은 바꾸지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Anim Mirror")
	static UAnimSequence* BakeMirroredSequence(UAnimSequence* Source, UMirrorDataTable* MirrorTable, const FString& PackagePath);
};
