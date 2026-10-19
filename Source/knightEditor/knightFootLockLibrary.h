#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "knightFootLockLibrary.generated.h"

class UAnimSequence;

/**
 * 몬스터 발 고정(knightLegPlacement 노드)이 읽는 접지 커브 "FootLock_<발 뼈>"를 애니메이션에 굽는 도구.
 * 프레임마다 발 높이를 재서, 이 애니메이션에서 발이 가장 낮은 높이(= 땅) 근처에 있는 구간을 1, 나머지를 0으로 쓴다.
 * 여유는 다리 길이(발 ~ 조부모 뼈, 기준 포즈)에 비례하므로 몬스터 크기와 상관없이 같은 값을 쓴다.
 * Python에서 부른다(unreal.knightFootLockLibrary). 저장은 호출한 쪽.
 *
 * 미러 애니는 미러로 구운 뒤에 굽는다(미러 굽기는 커브 이름의 좌우를 바꾸지 않는다).
 */
UCLASS()
class KNIGHTEDITOR_API UknightFootLockLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * FootBones마다 접지 커브를 다시 쓴다(같은 이름의 커브는 지우고 새로 씀). 쓴 커브 수를 돌려준다.
	 * @param HeightToleranceRatio  땅(최저 높이)에서 다리 길이의 이 비율 안이면 디딘 것으로 본다. 뗄 때는 1.5배까지 허용(떨림 방지).
	 * @param MinPhaseTime          이보다 짧은 디딤·들기 구간은 앞뒤 구간에 합친다(초).
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Foot Lock")
	static int32 BakeFootLockCurves(UAnimSequence* Sequence, const TArray<FName>& FootBones, float HeightToleranceRatio = 0.1f, float MinPhaseTime = 0.1f);

	/** BakeFootLockCurves가 쓰는 커브 이름. 런타임 노드와 같은 규칙. */
	UFUNCTION(BlueprintPure, Category="Knight|Foot Lock")
	static FName GetFootLockCurveName(FName FootBone);
};
