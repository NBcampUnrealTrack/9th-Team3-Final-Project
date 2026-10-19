#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "Combat/knightCombatTypes.h"
#include "Engine/DataAsset.h"
#include "Monster/knightMonsterPartData.h"
#include "knightMonsterRecipe.generated.h"

class UAnimSequence;
class UBlendSpace;
class USkeletalMesh;

UENUM()
enum class EknightRecipeAttackType : uint8
{
	/** 몽타주의 판정 구간(Knight Melee Trace)이 소켓 사이를 훑는다. 할퀴기, 물기, 꼬리 휘두르기. */
	Melee,
	/** 어빌리티가 예고 → 구체 판정 → 후딜 시간을 잡는다. 내려찍기, 몸통 박치기, 브레스. */
	Area
};

/** 근접 판정 구간 하나. 시간은 몽타주 시간(초, 원본 애니메이션 기준)이다. */
USTRUCT()
struct FknightRecipeHitWindow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Hit", meta=(ClampMin=0, Units="s"))
	float Start = 0.f;

	UPROPERTY(EditAnywhere, Category="Hit", meta=(ClampMin=0, Units="s"))
	float End = 0.f;

	UPROPERTY(EditAnywhere, Category="Hit", meta=(ShowOnlyInnerProperties))
	FknightMeleeTraceParams Trace;
};

/** 이 구간만 PlayRate배로 재생한다(Knight Play Rate). 원본 애니메이션을 고치지 않고 완급을 만든다. */
USTRUCT()
struct FknightRecipePlayRateWindow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Play Rate", meta=(ClampMin=0, Units="s"))
	float Start = 0.f;

	UPROPERTY(EditAnywhere, Category="Play Rate", meta=(ClampMin=0, Units="s"))
	float End = 0.f;

	UPROPERTY(EditAnywhere, Category="Play Rate", meta=(ClampMin=0.1, ClampMax=10))
	float PlayRate = 2.f;
};

/** 공격 하나. GA_<몬스터>_<Name>과 AM_<몬스터>_<Name>이 만들어진다. */
USTRUCT()
struct FknightRecipeAttack
{
	GENERATED_BODY()

	/** 에셋 이름에 붙는다(영문, 공백 없이). */
	UPROPERTY(EditAnywhere, Category="Attack")
	FName Name;

	UPROPERTY(EditAnywhere, Category="Attack")
	EknightRecipeAttackType Type = EknightRecipeAttackType::Melee;

	/** Ability.Attack.Ranged를 붙인다. 공용 StateTree가 멀리 있는 타깃에게 이 공격을 따로 고른다(브레스). */
	UPROPERTY(EditAnywhere, Category="Attack")
	bool bRanged = false;

	UPROPERTY(EditAnywhere, Category="Attack")
	TObjectPtr<UAnimSequence> Animation;

	UPROPERTY(EditAnywhere, Category="Attack")
	TArray<FknightRecipePlayRateWindow> PlayRateWindows;

	UPROPERTY(EditAnywhere, Category="Melee", meta=(EditCondition="Type == EknightRecipeAttackType::Melee", EditConditionHides))
	TArray<FknightRecipeHitWindow> HitWindows;

	/** 발동부터 타격까지(실제 초). Play Rate 구간이 있으면 줄어든 시간으로 적는다. */
	UPROPERTY(EditAnywhere, Category="Area", meta=(EditCondition="Type == EknightRecipeAttackType::Area", EditConditionHides, ClampMin=0, Units="s"))
	float WindupTime = 0.8f;

	UPROPERTY(EditAnywhere, Category="Area", meta=(EditCondition="Type == EknightRecipeAttackType::Area", EditConditionHides, ClampMin=0, Units="s"))
	float ActiveTime = 0.f;

	UPROPERTY(EditAnywhere, Category="Area", meta=(EditCondition="Type == EknightRecipeAttackType::Area", EditConditionHides, ClampMin=0, Units="s"))
	float RecoveryTime = 1.f;

	/** 몬스터 로컬 공간 기준 구체 중심(X가 정면). */
	UPROPERTY(EditAnywhere, Category="Area", meta=(EditCondition="Type == EknightRecipeAttackType::Area", EditConditionHides))
	FVector AreaOffset = FVector(350.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, Category="Area", meta=(EditCondition="Type == EknightRecipeAttackType::Area", EditConditionHides, ClampMin=1, Units="cm"))
	float AreaRadius = 300.f;

	UPROPERTY(EditAnywhere, Category="Area", meta=(EditCondition="Type == EknightRecipeAttackType::Area", EditConditionHides, Categories="GameplayCue"))
	FGameplayTag TelegraphCue;

	/** 고를 수 있는 조건과 확률. BlockedOwnerTags는 발동 차단 태그에도 들어간다(부위 파괴 후 봉인). */
	UPROPERTY(EditAnywhere, Category="Selection")
	FknightMonsterAttackSelection Selection;

	UPROPERTY(EditAnywhere, Category="Damage")
	FknightMeleeHitParams HitParams;

	/** 발동하면 타깃 방향에서 이 각도만큼 틀어진 쪽으로 몸을 돌린다. 양수는 오른쪽. */
	UPROPERTY(EditAnywhere, Category="Turn", meta=(ClampMin=-180, ClampMax=180, Units="deg"))
	float StartTurnYaw = 0.f;

	UPROPERTY(EditAnywhere, Category="Turn", meta=(ClampMin=1, EditCondition="StartTurnYaw != 0"))
	float StartTurnRate = 180.f;

	UPROPERTY(EditAnywhere, Category="Turn", meta=(ClampMin=0, Units="s", EditCondition="StartTurnYaw != 0"))
	float StartTurnMaxTime = 0.5f;
};

/** 공격이 아닌 반응. 비어 있는 애니메이션의 반응 어빌리티는 만들지 않는다. */
USTRUCT()
struct FknightRecipeReactions
{
	GENERATED_BODY()

	/** 퍼펙트 가드(GA_Parried)와 부위 파괴(GA_BreakFlinch) 경직에 같이 쓴다. */
	UPROPERTY(EditAnywhere, Category="Stagger")
	TObjectPtr<UAnimSequence> Hit;

	/** 경직 시간. 0이면 애니메이션 길이. */
	UPROPERTY(EditAnywhere, Category="Stagger", meta=(ClampMin=0, Units="s"))
	float StaggerTime = 0.f;

	/** 경직도가 깨질 때의 다운. DownLoop가 있으면 Down 몽타주의 루프 구간이 된다. */
	UPROPERTY(EditAnywhere, Category="Down")
	TObjectPtr<UAnimSequence> DownStart;

	UPROPERTY(EditAnywhere, Category="Down")
	TObjectPtr<UAnimSequence> DownLoop;

	UPROPERTY(EditAnywhere, Category="Down")
	TObjectPtr<UAnimSequence> GetUp;

	/** 쓰러져 있는 시간(일어나기 전까지). */
	UPROPERTY(EditAnywhere, Category="Down", meta=(ClampMin=0, Units="s"))
	float DownTime = 4.f;

	/** 다운할 때마다 최대 경직도가 이 비율만큼 늘어난다. */
	UPROPERTY(EditAnywhere, Category="Down", meta=(ClampMin=0, ClampMax=2))
	float MaxPoiseGrowth = 0.25f;

	/** 처음 플레이어를 발견했을 때(Event.Monster.Engaged). */
	UPROPERTY(EditAnywhere, Category="Roar")
	TObjectPtr<UAnimSequence> EngageRoar;

	/** 분노가 시작될 때(State.Monster.Enraged). */
	UPROPERTY(EditAnywhere, Category="Roar")
	TObjectPtr<UAnimSequence> EnrageRoar;

	/** 사망 몽타주는 마지막 포즈를 유지하도록 자동 블렌드 아웃을 끈다. */
	UPROPERTY(EditAnywhere, Category="Death")
	TObjectPtr<UAnimSequence> Death;
};

/**
 * 몬스터 하나를 만드는 레시피. Build를 누르면 같은 폴더 아래에 GA, 몽타주, 데이터, GE, ABP, BP를 만든다.
 * 다시 Build하면 에셋을 지우지 않고 레시피에 있는 값만 덮어쓴다. 레시피에 없는 값(몽타주의 사운드 노티파이,
 * BP에 직접 붙인 연출 컴포넌트 등)은 그대로 남는다.
 *
 * 공용 에셋(/Game/Knight/Creatures/Shared): ST_Monster_Default, BP_MonsterAIController, ABP_Monster_Base,
 * GE_Monster_Enrage/Exhaust/RageReset/StaminaRestore. 몬스터별 차이는 이 레시피와 몬스터 DA에만 둔다.
 *
 * 에디터 전용 에셋이라 패키징에 포함되지 않는다.
 */
UCLASS(BlueprintType)
class KNIGHTEDITOR_API UknightMonsterRecipe : public UDataAsset
{
	GENERATED_BODY()

public:
	virtual bool IsEditorOnly() const override { return true; }

	/** 에셋 이름에 쓰는 몬스터 이름(영문). BP_<Name>, GA_<Name>_Bite처럼 붙는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Monster")
	FName MonsterName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Monster")
	FText DisplayName;

	/** 비워 두면 이 레시피가 있는 폴더에 만든다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Monster", meta=(ContentDir))
	FDirectoryPath OutputFolder;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body")
	TObjectPtr<USkeletalMesh> Mesh;

	/** 메시 크기 배율. 캡슐과 거리 값은 따로 적는다(아래 Resize로 한꺼번에 키울 수 있다). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body", meta=(ClampMin=0.01))
	float MeshScale = 1.f;

	/** 메시가 정면(X)을 보도록 돌리는 yaw. 대부분의 캐릭터 메시는 -90. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body")
	float MeshYaw = -90.f;

	/** 이동용 캡슐. 피격은 메시의 피직스 바디가 받으므로 몸보다 작게 둔다. 메시는 캡슐 바닥에 맞춘다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body", meta=(ClampMin=1, Units="cm"))
	float CapsuleRadius = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body", meta=(ClampMin=1, Units="cm"))
	float CapsuleHalfHeight = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body", meta=(ClampMin=0, Units="cm/s"))
	float MaxWalkSpeed = 600.f;

	/** 몸 아래 차단 상자의 반 크기. 0이면 만들지 않는다. 플레이어가 몸 아래로 파고들지 못하게 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body")
	FVector BodyBlockerExtent = FVector::ZeroVector;

	/** 캡슐 중심 기준 차단 상자 위치. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body")
	FVector BodyBlockerOffset = FVector::ZeroVector;

	/** 겹친 플레이어를 밀어낼 때 쓰는 몸 축의 반 길이(knightMonsterPushOutComponent). 0이면 기본값. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body", meta=(ClampMin=0, Units="cm"))
	float PushOutBodyAxisHalfLength = 0.f;

	/** 메시 바디와 겹친 플레이어도 밀어낸다. 머리·꼬리가 캡슐 밖으로 크게 나오는 몬스터에서 켠다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body")
	bool bPushOutOfMesh = true;

	/** 이동 속도(GroundSpeed)로 걷기·달리기를 섞는 1D 블렌드스페이스. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UBlendSpace> Locomotion;

	/** 지형에 맞출 발 뼈(발목). 비우면 다리 맞추기를 하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
	TArray<FName> FootBones;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
	FName PelvisBone = TEXT("pelvis");

	/**
	 * Build할 때 Locomotion 샘플 애니메이션에 발 고정용 접지 커브(FootLock_<발 뼈>)를 굽는다. 디딘 발이 미끄러지지 않고,
	 * 코드로 몸을 돌리면 발을 옮겨 딛는다. 다른 애니메이션(회전, 공격 등)은 knightFootLockLibrary로 따로 굽는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation", meta=(EditCondition="!FootBones.IsEmpty()"))
	bool bBakeFootLockCurves = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats", meta=(ClampMin=1))
	float MaxHealth = 1000.f;

	/** 0이면 속성 기본값을 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats", meta=(ClampMin=0))
	float AttackPower = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats", meta=(ClampMin=0))
	float MaxPoise = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats", meta=(ClampMin=0))
	float MaxRage = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats", meta=(ClampMin=0))
	float MaxStamina = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats", meta=(ClampMin=0, Units="s"))
	float EnrageDuration = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats", meta=(ClampMin=0, Units="s"))
	float ExhaustDuration = 8.f;

	/** 퍼펙트 가드를 당할 때마다 깎이는 경직도(최대 경직도 비율). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats", meta=(ClampMin=0, ClampMax=1))
	float ParriedPoiseDamageRatio = 0.25f;

	/** 락온 카메라가 몬스터를 담는 반지름. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Targeting", meta=(ClampMin=0, Units="cm"))
	float TargetRadius = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Targeting")
	FName FallbackLockOnSocket = TEXT("spine_03");

	/** 공용 StateTree의 추격 정지 거리. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI", meta=(ClampMin=0, Units="cm"))
	float ChaseStopDistance = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI", meta=(ClampMin=0, Units="cm"))
	float RangedMinDistance = 533.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI", meta=(ClampMin=0, Units="cm"))
	float RangedMaxDistance = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Parts", meta=(TitleProperty="PartTag"))
	TArray<FknightMonsterPartDefinition> Parts;

	UPROPERTY(EditAnywhere, Category="Attacks", meta=(TitleProperty="Name"))
	TArray<FknightRecipeAttack> Attacks;

	UPROPERTY(EditAnywhere, Category="Reactions")
	FknightRecipeReactions Reactions;

	/** Resize가 곱하는 배율. 메시, 캡슐, 속도, 거리, 반경, 판정 굵기를 한꺼번에 키운다. */
	UPROPERTY(EditAnywhere, Category="Tools", meta=(ClampMin=0.01))
	float ResizeFactor = 1.f;

	/** 레시피로 에셋을 만들거나 고친다. 결과는 메시지 로그(Knight Monster Recipe)에 남는다. */
	UFUNCTION(CallInEditor, Category="Tools")
	void Build();

	/** 만들기 전에 빠진 값과 지원하지 않는 설정을 검사한다. */
	UFUNCTION(CallInEditor, Category="Tools")
	void Validate();

	/** 크기와 관련된 값을 ResizeFactor배로 바꾸고 ResizeFactor를 1로 되돌린다. */
	UFUNCTION(CallInEditor, Category="Tools")
	void Resize();

	/** 레시피를 만들 에셋들이 들어갈 폴더(/Game/...). */
	FString GetOutputPath() const;
};
