#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "knightMontageEditLibrary.generated.h"

class UAnimMontage;
class UAnimSequenceBase;

/**
 * 몽타주 편집 도구(에디터 전용). Python에서 부른다(unreal.knightMontageEditLibrary). 저장은 호출한 쪽.
 *
 * 몽타주를 복제해 애니만 바꿔 새 공격을 만들 때 쓴다. MCP·Python으로는 몽타주 길이(SequenceLength)를 바꿀 수 없어서,
 * 길이가 다른 애니로 바꾸면 몽타주가 예전 길이에서 끝나 버렸다.
 */
UCLASS()
class KNIGHTEDITOR_API UknightMontageEditLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Montage의 첫 슬롯 트랙을 Animation 한 구간(처음부터 끝까지, 배속 1)으로 바꾸고 몽타주 길이를 그 애니 길이에 맞춘다.
	 * 슬롯 이름과 섹션·노티파이 시간은 그대로 두고, 그것들이 기억하는 애니만 새 애니로 다시 잇는다
	 * (노티파이 시간을 새 애니에 맞춰 옮기는 것은 호출한 쪽). 실패하면 false.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Montage")
	static bool SetMontageAnimation(UAnimMontage* Montage, UAnimSequenceBase* Animation);
};
