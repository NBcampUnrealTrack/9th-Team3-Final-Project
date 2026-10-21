#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "knightAnimSpliceLibrary.generated.h"

class UAnimSequence;

/**
 * 두 애니메이션을 이어 붙여 한 시퀀스로 굽는 도구. 앞 애니의 일부(예: 예전 구르기의 구르는 구간)에 다른 애니(예: V2 일어나기)를
 * 이어서, 끝 자세가 다른 애니와 맞지 않는 문제를 몽타주 하나로 해결한다. 판정·루트 모션·되감기가 시퀀스 하나 기준으로 그대로 동작한다.
 * Python에서 부른다(unreal.knightAnimSpliceLibrary). 저장은 호출한 쪽.
 */
UCLASS()
class KNIGHTEDITOR_API UknightAnimSpliceLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * First의 0~FirstEndTime 뒤에 Second의 SecondStartTime~끝을 이어 PackagePath에 굽는다. 이음매에서 BlendTime 동안 First(계속 재생)와
	 * Second를 뼈마다 섞는다. 루트는 First를 따르다가, 이음매부터는 그 자리를 기준으로 Second의 루트 움직임만큼 움직인다.
	 * 처음이면 First를 복제하고(루트 모션 설정이 따라온다), 이미 있으면 뼈 트랙만 다시 쓴다. 프레임 레이트는 First를 따른다.
	 * 두 자세가 비슷한 시점을 고르는 것은 호출한 쪽의 일이다(뼈 위치 오차가 가장 작은 시점).
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Anim Splice")
	static UAnimSequence* BakeSplicedSequence(UAnimSequence* First, float FirstEndTime, UAnimSequence* Second, float SecondStartTime,
		float BlendTime, const FString& PackagePath);

	/**
	 * BakeSplicedSequence와 같되, 이음매에서 Second 전체를 SecondOffset(이음매 루트 기준 이동·회전)만큼 옮겨 두 자세를 맞춘다.
	 * 예: 옆으로 쓰러진 위치가 일어나기 애니의 시작 위치와 다를 때, 몸이 블렌드 동안 옆으로 미끄러지지 않게 한다.
	 * 몸은 맞춘 자리에 그대로 보이고, 루트만 이음매부터 끝까지 천천히 따라가 끝에서는 Second 원래 자세와 루트가 일치한다.
	 * 루트 모션을 켜면 캡슐이 그만큼 따라 움직여 다음 애니로 넘어갈 때 몸이 끌려가지 않는다(켜는 것은 호출한 쪽).
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Anim Splice")
	static UAnimSequence* BakeAlignedSplicedSequence(UAnimSequence* First, float FirstEndTime, UAnimSequence* Second, float SecondStartTime,
		float BlendTime, const FTransform& SecondOffset, const FString& PackagePath);
};
