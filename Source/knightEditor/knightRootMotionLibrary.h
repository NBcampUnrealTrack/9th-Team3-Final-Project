#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "knightRootMotionLibrary.generated.h"

class UAnimSequence;

/**
 * 몸 전체가 골반으로만 움직이고 루트는 제자리인 애니메이션을, 그 수평 이동을 루트로 옮긴 루트 모션 애니메이션으로 굽는 도구.
 * 루트 모션을 켜면 캡슐이 몸을 따라가서, 애니메이션이 끝나거나 다음 애니메이션으로 넘어갈 때 몸이 캡슐 자리로 끌려 돌아오지 않는다.
 * Python에서 부른다(unreal.knightRootMotionLibrary). 저장은 호출한 쪽.
 */
UCLASS()
class KNIGHTEDITOR_API UknightRootMotionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Source를 PackagePath에 굽는다. 루트는 Bone(루트의 바로 아래 뼈, 보통 pelvis)의 수평 이동을 따라가고, Bone은 첫 프레임의
	 * 수평 위치에 머문다(높이·회전은 원본 그대로). 처음이면 Source를 복제하고, 이미 있으면 루트와 Bone 트랙만 다시 쓴다.
	 * 결과는 루트 모션을 켠다(Root Lock = 애니메이션 첫 프레임).
	 * @param HoldAfterTime  이 시각 뒤로는 루트가 따라가지 않고 머문다. 다음 애니메이션과 맞추려고 끝에서 몸을 원점으로 되돌리는
	 *                       구간을 버릴 때 쓴다(그 구간의 수평 이동은 사라지고 그 자리에 머문다). 0보다 작으면 끝까지 따라간다.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Root Motion")
	static UAnimSequence* BakeRootMotionFromBone(UAnimSequence* Source, FName Bone, float HoldAfterTime, const FString& PackagePath);
};
