#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "knightMonsterRecipeLibrary.generated.h"

class AknightMonsterCharacterBase;
class UAnimMontage;
class UAnimSequence;
class UBlendSpace;
class UPhysicsAsset;
class USkeletalMesh;
class USoundAttenuation;
class USoundBase;
class USoundClass;
class USoundCue;
class USoundWave;
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
	 * 몬스터 StateTree의 공격 사이 상태(Recover)를 Combat 아래에 다시 만들고 컴파일한다(저장은 호출한 쪽). 예전 Rest 상태는 지운다.
	 *   Recover ─ Retreat: 확률(몬스터 정의 RecoverRetreatChance)과 Ability.Move.Retreat 어빌리티를 쓸 수 있으면 물러난다
	 *           ├ Turn:    타깃이 TurnAngle 이상 옆·뒤에 있으면 돌아본다(옆·뒤 공격을 쓸 수 있으면 돌지 않고 바로 공격으로)
	 *           └ Hold:    몬스터 정의의 AttackRestTime 동안 서서 천천히 노려본다
	 * Strike·Breath는 공격 Task 안에서 쉬지 않고(bRestAfterAttack 끔) 성공하면 Recover로 간다.
	 * Combat·Strike·React 상태나 Knight Monster Target 평가기가 없으면 false.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Monster Recipe")
	static bool SetupRecoverState(UStateTree* StateTree, float TurnAngle = 100.f);

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

	/**
	 * 몽타주에서 Sound를 재생하는 PlaySound 노티파이를 모두 Time(몽타주 시간)으로 옮긴다. 애니를 바꿔 소리가 동작과 어긋났을 때 쓴다.
	 * 옮긴 개수를 돌려준다. 저장은 호출한 쪽.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Monster Recipe")
	static int32 SetMontageSoundTime(UAnimMontage* Montage, const USoundBase* Sound, float Time);

	/**
	 * Waves를 번갈아(이미 나온 것은 빼고 무작위로) 재생하는 SoundCue를 PackagePath(/Game/...)에 만든다. 같은 소리가 연달아 나지 않는다.
	 * 이미 있으면 노드를 새로 짠다. 저장은 호출한 쪽.
	 */
	/**
	 * 애님 Blueprint의 Slot 노드 바로 뒤에 Knight Hit Flinch 노드(맞을 때 덧셈 움찔)를 끼우고 컴파일한다. 이미 있으면 아무것도 하지 않는다.
	 * 공용 ABP_Monster_Base에 넣어 두면 레시피로 새로 만드는 몬스터는 따라온다. 저장은 호출한 쪽.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Monster Recipe")
	static bool AddHitFlinchNode(UAnimBlueprint* AnimBlueprint);

	UFUNCTION(BlueprintCallable, Category="Knight|Monster Recipe")
	static USoundCue* CreateRandomSoundCue(const FString& PackagePath, const TArray<USoundWave*>& Waves, USoundClass* SoundClass, USoundAttenuation* Attenuation);

	/**
	 * 몽타주의 Start~End(몽타주 시간)에 재생 속도(Knight Play Rate) 노티파이를 넣는다. 같은 구간에 이미 있으면 속도만 바꾼다.
	 * 예비 동작을 늘이거나 도약 직전에 멈칫하게 할 때 쓴다. 어빌리티가 몽타주 위치로 재는 타이밍은 그대로다. 저장은 호출한 쪽.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Monster Recipe")
	static bool SetMontagePlayRateWindow(UAnimMontage* Montage, float Start, float End, float PlayRate);

	/**
	 * Source 몽타주에서 시작 시간이 SourceStart~SourceEnd(Source 몽타주 시간)인 노티파이를 복제해 Target의 TargetStart부터 같은 간격으로 넣는다.
	 * 노티파이 상태는 길이를 유지하되 Target 끝을 넘으면 자른다. 트랙은 이름으로 찾거나 만든다. OnlyClass가 있으면 그 클래스 노티파이만 복사한다.
	 * 단독 공격 몽타주들을 이어 붙인 연계 공격을 만들 때, 이미 맞춘 판정·소리·속도 구간을 그대로 옮기려고 쓴다. 복사한 개수를 돌려준다. 저장은 호출한 쪽.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Monster Recipe")
	static int32 CopyMontageNotifies(const UAnimMontage* Source, float SourceStart, float SourceEnd, UAnimMontage* Target, float TargetStart,
		TSubclassOf<UObject> OnlyClass = nullptr);

	/** 몽타주의 노티파이를 모두 지운다(트랙은 남긴다). 애니를 통째로 바꾼 뒤 CopyMontageNotifies로 다시 채우기 전에 쓴다. 저장은 호출한 쪽. */
	UFUNCTION(BlueprintCallable, Category="Knight|Monster Recipe")
	static int32 ClearMontageNotifies(UAnimMontage* Montage);

	/**
	 * 피격용 피직스 에셋을 만들거나 다시 만들고 메시에 연결한다. CapsuleBones·BoxBones의 뼈마다 바디 하나를 만들고, 그 뼈와
	 * 목록에 없는 자식 뼈(발가락, 귀 등)에 붙은 정점에 맞춰 크기를 잡는다. 날개 막처럼 넓적한 곳은 BoxBones에 넣는다.
	 * 바디는 모두 Kinematic(애니메이션을 따라감)이고 제약은 만들지 않는다. 기존 바디는 지운다. 저장은 호출한 쪽.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Monster Recipe")
	static UPhysicsAsset* CreateMonsterPhysicsAsset(USkeletalMesh* Mesh, const FString& PackagePath, const TArray<FName>& CapsuleBones,
		const TArray<FName>& BoxBones);
};
