#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "knightLocalizationEditorLibrary.generated.h"

/**
 * 번역 수집에서 빠지는 텍스트를 찾아 고치는 에디터 도구. Python(unreal.knightLocalizationEditorLibrary)에서 부른다.
 *
 * 스크립트·MCP·INVTEXT로 넣은 FText는 '번역 안 함(culture-invariant)'으로 저장돼서 GatherText가 건너뛴다.
 * 이 도구는 에셋 패키지 안의 모든 객체(위젯 트리 위젯, IMC 키 설정 같은 하위 객체 포함)의 FText를 훑어,
 * 한글이 들어 있는데 번역 수집 대상이 아닌 텍스트에 새 번역 키를 붙인다. 글자는 바뀌지 않는다.
 * 영어 장식 문구(GRAPHICS 등)처럼 한글이 없는 텍스트와 String Table 텍스트는 건드리지 않는다.
 *
 * 사용 예 (에디터 콘솔 `py`):
 *   lib = unreal.knightLocalizationEditorLibrary
 *   lines = lib.find_unlocalizable_texts(asset, False)   # 목록만
 *   lines = lib.find_unlocalizable_texts(asset, True)    # 고침 → 위젯 블루프린트면 컴파일, 저장은 호출한 쪽이 한다
 */
UCLASS()
class KNIGHTEDITOR_API UknightLocalizationEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Asset이 든 패키지 안의 번역 안 되는 한글 FText를 "객체 경로.속성: 글자" 줄로 돌려준다.
	 * bApply면 그 텍스트에 패키지 네임스페이스 + 새 키를 붙여 번역 가능하게 바꾸고 패키지를 수정됨으로 표시한다.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Localization Editor")
	static TArray<FString> FindUnlocalizableTexts(UObject* Asset, bool bApply);
};
