#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "knightMonsterRecipeLibrary.generated.h"

class AknightMonsterCharacterBase;
class UknightMonsterRecipe;

/**
 * 몬스터 레시피(UknightMonsterRecipe)로 에셋 세트를 만드는 도구. 레시피 에셋의 Build 버튼이 부르고,
 * 헤드리스 확인을 위해 Python에서도 부를 수 있다(unreal.knightMonsterRecipeLibrary).
 */
UCLASS()
class KNIGHTEDITOR_API UknightMonsterRecipeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 레시피의 에셋 세트를 만들거나 고친다. 검사에서 오류가 나면 아무것도 만들지 않고 false. */
	UFUNCTION(BlueprintCallable, Category="Knight|Monster Recipe")
	static bool BuildMonsterRecipe(UknightMonsterRecipe* Recipe);

	/** 문제 목록을 돌려준다. "오류:"로 시작하는 항목이 있으면 Build할 수 없다. */
	UFUNCTION(BlueprintCallable, Category="Knight|Monster Recipe")
	static TArray<FString> ValidateMonsterRecipe(const UknightMonsterRecipe* Recipe);

	/**
	 * 공용 에셋(/Game/Knight/Creatures/Shared)이 없으면 잔열 에셋을 바탕으로 만든다. StateTree와 AI 컨트롤러는
	 * 몬스터 기반 클래스를 컨텍스트로 쓰도록 바꾸고, 거리 값은 몬스터 DA를 읽도록 바인딩한다.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Monster Recipe")
	static bool CreateSharedMonsterAssets();

	/** 이미 있는 몬스터 Blueprint에서 값을 읽어 레시피를 만든다(잔열 재생성 비교용). 같은 이름이 있으면 덮어쓴다. */
	UFUNCTION(BlueprintCallable, Category="Knight|Monster Recipe")
	static UknightMonsterRecipe* CreateRecipeFromMonster(TSubclassOf<AknightMonsterCharacterBase> MonsterClass, const FString& RecipePackagePath,
		FName MonsterName, const FString& OutputFolder);
};
