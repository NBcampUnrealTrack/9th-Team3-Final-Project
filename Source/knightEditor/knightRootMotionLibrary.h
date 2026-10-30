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

	/**
	 * 제자리 걷기 애니메이션(옆걸음, 뒷걸음)에 디딘 발이 땅에서 미끄러지지 않는 만큼의 수평 이동을 루트로 넣어 PackagePath에 굽는다.
	 * 프레임마다 디딘 발(FootLock_<발 뼈> 커브가 0.5 이상, 먼저 BakeFootLockCurves로 굽는다)의 평균 수평 이동을 반대로 루트에 더하므로
	 * 걸음마다 빨라지고 느려지는 속도가 애니메이션과 정확히 맞는다. 디딘 발이 없는 프레임은 앞 프레임 속도를 이어 쓴다.
	 * SmoothKeys만큼 앞뒤 프레임 속도를 평균해 발이 바뀌는 순간의 튐을 줄인다. 처음이면 Source를 복제하고, 이미 있으면 루트 트랙만 다시 쓴다.
	 * bLoop면 끝과 처음을 이어서 계산한다(달리기처럼 반복 재생하는 애니). 그러지 않으면 반복할 때마다 경계에서 속도가 한 번 꺼진다.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Root Motion")
	static UAnimSequence* BakeRootMotionFromFeet(UAnimSequence* Source, const TArray<FName>& FootBones, int32 SmoothKeys, const FString& PackagePath, bool bLoop = false);
};
