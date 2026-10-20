#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "knightBlueprintGraphLibrary.generated.h"

class UBlueprint;

/**
 * Blueprint 그래프를 복사·붙여넣기 텍스트로 다루는 도구. 이미 있는 노드 묶음을 본떠 비슷한 로직을 추가할 때
 * Python에서 부른다(unreal.knightBlueprintGraphLibrary). 바꾼 뒤 컴파일과 저장은 부르는 쪽이 한다.
 */
UCLASS()
class KNIGHTEDITOR_API UknightBlueprintGraphLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 그래프의 모든 노드를 에디터 복사(Ctrl+C)와 같은 텍스트로 돌려준다. 그래프가 없으면 빈 문자열. */
	UFUNCTION(BlueprintCallable, Category="Knight|Blueprint Graph")
	static FString ExportGraphNodes(UBlueprint* Blueprint, FName GraphName);

	/**
	 * 붙여넣기 텍스트를 그래프에 넣고 새로 생긴 노드 수를 돌려준다. 실패하면 -1.
	 * bReplace면 기존 노드를 모두 지우고 넣는다. 내보낸 텍스트를 고쳐 다시 넣을 때 쓰며, 노드 이름과 GUID가 유지된다.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Blueprint Graph")
	static int32 ImportGraphNodes(UBlueprint* Blueprint, FName GraphName, const FString& Text, bool bReplace = false);

	/** Source 멤버 변수와 같은 타입으로 NewName 변수를 만든다. 이미 있으면 true. */
	UFUNCTION(BlueprintCallable, Category="Knight|Blueprint Graph")
	static bool DuplicateMemberVariable(UBlueprint* Blueprint, FName Source, FName NewName);
};
