#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "knightMonsterRecipeLibrary.generated.h"

class AknightMonsterCharacterBase;
class UAnimMontage;
class UAnimSequence;
class UBlendSpace;
class UknightMonsterRecipe;

/**
 * 몬스터 레시피(UknightMonsterRecipe)로 에셋 세트를 만드는 도구. 레시피 에셋의 Build 버튼이 부르고,
 * 헤드리스 확인을 위해 Python에서도 부를 수 있다(unreal.knightMonsterRecipeLibrary).
 */
class UStateTree;

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

	/**
	 * StateTree에서 StateName 상태의 Knight Face Target 태스크의 옆·뒤 공격 건너뛰기(bSkipForSideAttacks, SideAttackTags)를
	 * 바꾸고 컴파일한다(저장은 호출한 쪽). 원거리 상태의 조준(Aim)은 SideAttackTags를 Ability.Attack.Ranged로 둔다.
	 * 상태나 태스크가 없으면 false.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Monster Recipe")
	static bool SetFaceTargetSideAttacks(UStateTree* StateTree, FName StateName, bool bSkip, const FGameplayTagContainer& SideAttackTags);

	/**
	 * 시퀀스마다 섹션 하나인 몽타주를 만들거나 고친다(돌진의 Start, Loop, Stop처럼). 섹션은 순서대로 이어지고,
	 * LoopSection은 자기 자신으로 이어져 어빌리티가 다음 섹션을 정할 때까지 반복한다. 노티파이는 남긴다. 저장은 호출한 쪽.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Monster Recipe")
	static UAnimMontage* BuildSectionMontage(const FString& PackagePath, const TArray<UAnimSequence*>& Sequences, const TArray<FName>& SectionNames, FName LoopSection);

	/**
	 * 블렌드스페이스의 샘플 검사와 블렌드 구간 데이터를 다시 만든다. Python에서 샘플이나 축을 바꾸면 이 계산이 돌지 않아
	 * 런타임이 옛 구간으로 섞으므로, 바꾼 뒤 부른다. 저장은 호출한 쪽.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Monster Recipe")
	static bool ResampleBlendSpace(UBlendSpace* BlendSpace);

	/**
	 * 몽타주 첫 슬롯의 SegmentIndex번 구간이 시퀀스의 StartTime~EndTime(초)만 재생하게 자르고, 뒤 구간과 섹션 위치를 다시 맞춘다.
	 * 애니 앞뒤의 필요 없는 동작을 원본을 고치지 않고 뺄 때 쓴다. 섹션은 BuildSectionMontage처럼 구간마다 하나라고 본다.
	 * 노티파이 위치는 그대로이므로 자른 뒤 다시 놓는다. 저장은 호출한 쪽.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Monster Recipe")
	static bool SetMontageSegmentRange(UAnimMontage* Montage, int32 SegmentIndex, float StartTime, float EndTime);

	/**
	 * 몽타주 첫 슬롯의 SegmentIndex번 구간을 Sequence 전체로 바꾸고, 뒤 구간과 섹션 위치를 다시 맞춘다(SetMontageSegmentRange와 같음).
	 * 노티파이는 몽타주 시간 그대로 남는다. 저장은 호출한 쪽.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Monster Recipe")
	static bool SetMontageSegmentSequence(UAnimMontage* Montage, int32 SegmentIndex, UAnimSequenceBase* Sequence);
};
