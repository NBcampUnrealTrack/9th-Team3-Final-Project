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

	/** 0보다 크면 LungeStartTime ~ LungeEndTime(발동부터 실제 초) 동안 타깃 쪽으로 돌진한다. 구체 중심이 타깃에 오는 거리까지, 최대 이만큼. */
	UPROPERTY(EditAnywhere, Category="Area", meta=(EditCondition="Type == EknightRecipeAttackType::Area", EditConditionHides, ClampMin=0, Units="cm"))
	float LungeMaxDistance = 0.f;

	UPROPERTY(EditAnywhere, Category="Area", meta=(EditCondition="Type == EknightRecipeAttackType::Area && LungeMaxDistance > 0", EditConditionHides, ClampMin=0, Units="s"))
	float LungeStartTime = 0.4f;

	UPROPERTY(EditAnywhere, Category="Area", meta=(EditCondition="Type == EknightRecipeAttackType::Area && LungeMaxDistance > 0", EditConditionHides, ClampMin=0, Units="s"))
	float LungeEndTime = 0.8f;

	/** 고를 수 있는 조건과 확률. BlockedOwnerTags는 발동 차단 태그에도 들어간다(부위 파괴 후 봉인). */
	UPROPERTY(EditAnywhere, Category="Selection")
	FknightMonsterAttackSelection Selection;

	/** 공중 턴(AirGroundTurnTime > 0)에만 쓰는 공격. 아니면 지상 턴에만 쓴다. 공중 턴이 없는 몬스터에서는 보지 않는다. */
	UPROPERTY(EditAnywhere, Category="Selection")
	bool bAirAttack = false;

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

/** 날개 하나. 어깨 뼈를 돌려 날개 끝들을 바닥 위에 둔다. */
USTRUCT()
struct FknightRecipeWing
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Wing")
	FName RootBone;

	UPROPERTY(EditAnywhere, Category="Wing")
	TArray<FName> TipBones;
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

	/**
	 * 늘 떠 있게 만든 애니메이션(날짐승)을 바닥 가까이 내리는 최대 높이(knightMonsterHoverComponent). 발 뼈가 디딘 높이
	 * 아래로는 내리지 않으므로 쓰러지는 애니메이션은 원래 높이 그대로다. 0이면 내리지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hover", meta=(ClampMin=0, Units="cm"))
	float HoverDrop = 0.f;

	/** 바닥 아래로 들어가면 안 되는 발 뼈. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hover", meta=(EditCondition="HoverDrop > 0"))
	TArray<FName> HoverFootBones;

	/** 디뎠을 때 발 뼈의 바닥 위 높이(cm, 메시 배율 적용). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hover", meta=(ClampMin=0, Units="cm", EditCondition="HoverDrop > 0"))
	float HoverFootHeight = 0.f;

	/** 바닥 아래로 들어가면 안 되는 몸 뼈. 쓰러질 때 몸이 바닥에 처박히면 그만큼 메시를 올린다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hover")
	TArray<FName> HoverBodyBones;

	/** 몸 뼈의 바닥 위 최소 높이(cm, 메시 배율 적용). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hover", meta=(ClampMin=0, Units="cm"))
	float HoverBodyClearance = 0.f;

	/**
	 * 레시피 항목으로 만들지 않는 어빌리티(공중 패턴처럼 전용 클래스로 직접 만든 공격). Build가 어빌리티 세트에 그대로 넣고 건드리지 않는다.
	 * 공중 턴 공격이면 그 어빌리티의 Selection에 State.Monster.Airborne 필요 태그를 직접 둔다.
	 */
	UPROPERTY(EditAnywhere, Category="Attacks")
	TArray<TSubclassOf<UGameplayAbility>> ExtraAbilities;

	/**
	 * 지상 턴 시간(knightMonsterAirTurnComponent). 0보다 크면 지상 턴 → 이륙 → 공중 공격 AirAttacksPerTurn번 → 착지를 반복한다.
	 * 지상 공격은 공중 턴에 막히고, bAirAttack 공격만 공중 턴에 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn", meta=(ClampMin=0, Units="s"))
	float AirGroundTurnTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn", meta=(ClampMin=1, EditCondition="AirGroundTurnTime > 0"))
	int32 AirAttacksPerTurn = 2;

	/** 공중 공격을 다 쓰지 못해도 이 시간이 지나면 착지한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn", meta=(ClampMin=0, Units="s", EditCondition="AirGroundTurnTime > 0"))
	float AirTurnMaxTime = 20.f;

	/** 공중 턴에 타깃과 이 거리(수평)보다 가까우면 뒤로 물러난다. 0이면 물러나지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn", meta=(ClampMin=0, Units="cm", EditCondition="AirGroundTurnTime > 0"))
	float AirKeepDistance = 0.f;

	/** 공중 턴에 애니메이션 원래 높이(HoverDrop을 빼지 않은 높이)보다 더 띄우는 높이. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn", meta=(ClampMin=0, Units="cm", EditCondition="AirGroundTurnTime > 0"))
	float AirLift = 0.f;

	/** 공중 턴에 AirCeilingBone이 바닥 위 이 높이를 넘지 않는다(이륙 애니메이션이 몸을 크게 들어 올릴 때). 0이면 끈다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn", meta=(ClampMin=0, Units="cm", EditCondition="AirGroundTurnTime > 0"))
	float AirCeiling = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn", meta=(EditCondition="AirGroundTurnTime > 0 && AirCeiling > 0"))
	FName AirCeilingBone = TEXT("pelvis");

	/** 이륙 애니메이션과 쓸 구간(초). End가 0이면 끝까지. 구간 길이 동안 메시가 공중 높이로 오른다. */
	UPROPERTY(EditAnywhere, Category="Air Turn", meta=(EditCondition="AirGroundTurnTime > 0"))
	TObjectPtr<UAnimSequence> TakeOff;

	UPROPERTY(EditAnywhere, Category="Air Turn", meta=(ClampMin=0, Units="s", EditCondition="AirGroundTurnTime > 0"))
	float TakeOffStart = 0.f;

	UPROPERTY(EditAnywhere, Category="Air Turn", meta=(ClampMin=0, Units="s", EditCondition="AirGroundTurnTime > 0"))
	float TakeOffEnd = 0.f;

	/** 착지 애니메이션과 쓸 구간(초). 구간 길이 동안 메시가 지상 높이로 내려온다. */
	UPROPERTY(EditAnywhere, Category="Air Turn", meta=(EditCondition="AirGroundTurnTime > 0"))
	TObjectPtr<UAnimSequence> Landing;

	UPROPERTY(EditAnywhere, Category="Air Turn", meta=(ClampMin=0, Units="s", EditCondition="AirGroundTurnTime > 0"))
	float LandingStart = 0.f;

	UPROPERTY(EditAnywhere, Category="Air Turn", meta=(ClampMin=0, Units="s", EditCondition="AirGroundTurnTime > 0"))
	float LandingEnd = 0.f;

	/** 바닥 가까이에서 날개 끝이 바닥을 뚫지 않게 들어 올린다(애님 그래프의 Knight Wing Clearance 노드). 비우면 노드를 넣지 않는다. */
	UPROPERTY(EditAnywhere, Category="Hover", meta=(TitleProperty="RootBone"))
	TArray<FknightRecipeWing> HoverWings;

	/** 날개 끝 뼈를 바닥에서 띄우는 높이(cm). */
	UPROPERTY(EditAnywhere, Category="Hover", meta=(ClampMin=0, Units="cm"))
	float HoverWingClearance = 30.f;

	/** 바닥 가까이에서 늘어뜨린 꼬리가 바닥을 뚫지 않게 뼈마다 들어 올린다(Knight Wing Clearance 노드). 비우면 꼬리는 보지 않는다. */
	UPROPERTY(EditAnywhere, Category="Hover")
	FName HoverTailRootBone;

	/** 꼬리 끝 뼈. HoverTailRootBone의 자손이어야 한다. */
	UPROPERTY(EditAnywhere, Category="Hover", meta=(EditCondition="HoverTailRootBone != None"))
	FName HoverTailTipBone;

	/** 꼬리 뼈를 바닥에서 띄우는 높이(cm). */
	UPROPERTY(EditAnywhere, Category="Hover", meta=(ClampMin=0, Units="cm"))
	float HoverTailClearance = 20.f;

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

	/** 다운·탈진 중에는 이 뼈와 그 아래 뼈의 메시 바디로 플레이어를 밀어내지 않는다(날개 어깨). 비우면 모든 바디로 민다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body", meta=(EditCondition="bPushOutOfMesh"))
	TArray<FName> PushOutPassThroughBones;

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

	/** 최대 체력의 이 비율만큼 피해를 받으면 Rage가 가득 차 분노한다. 0이면 피해로 Rage가 오르지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats", meta=(ClampMin=0, ClampMax=1))
	float RageFullDamageRatio = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats", meta=(ClampMin=0, Units="s"))
	float ExhaustDuration = 8.f;

	/** 퍼펙트 가드를 당할 때마다 깎이는 경직도(최대 경직도 비율). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats", meta=(ClampMin=0, ClampMax=1))
	float ParriedPoiseDamageRatio = 0.25f;

	/** 플레이어 타격이 깎는 경직도 배율(받는 경직 배율). 0이면 타격으로는 그로기가 쌓이지 않고 퍼펙트 가드로만 쌓인다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats", meta=(ClampMin=0))
	float PoiseDamageTakenMultiplier = 1.f;

	/** 이 태그 중 하나라도 있으면 퍼펙트 가드로도 경직도가 깎이지 않고, 붙는 순간 게이지가 비워진다(공명해방). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats", meta=(Categories="State"))
	FGameplayTagContainer ParriedPoiseBlockedTags;

	/** 락온 카메라가 몬스터를 담는 반지름. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Targeting", meta=(ClampMin=0, Units="cm"))
	float TargetRadius = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Targeting")
	FName FallbackLockOnSocket = TEXT("spine_03");

	/** 바로 전에 쓴 공격은 다음 선택에서 뺀다(그것밖에 쓸 수 없을 때만 다시 쓴다). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI")
	bool bAvoidRepeatingLastAttack = false;

	/** 공용 StateTree의 추격 정지 거리. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI", meta=(ClampMin=0, Units="cm"))
	float ChaseStopDistance = 900.f;

	/** 공중 턴 동안의 추격 정지 거리. 0이면 ChaseStopDistance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI", meta=(ClampMin=0, Units="cm"))
	float AirChaseStopDistance = 0.f;

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
