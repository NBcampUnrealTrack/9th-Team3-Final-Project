#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_MonsterAttack.generated.h"

class UAbilityTask_Repeat;

class UAbilitySystemComponent;

UENUM(BlueprintType)
enum class EknightTargetAirborneRule : uint8
{
	Any,
	/** 대공 기술. */
	OnlyAirborne,
	/** 날고 있는 플레이어에게는 빗나가는 지상 기술. */
	OnlyGrounded
};

/** 타깃이 몬스터의 어느 쪽에 있어야 고를 수 있는지. 한쪽으로 움직이며 치는 기술(옆구르기)에 쓴다. */
UENUM(BlueprintType)
enum class EknightTargetSide : uint8
{
	Any,
	Left,
	Right
};

/** 몬스터가 공격을 고를 수 있는 조건과, 사용 가능한 공격 중에서 뽑힐 확률. */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightMonsterAttackSelection
{
	GENERATED_BODY()

	/** 몬스터와 타깃 원점 사이의 수평 거리. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Selection", meta=(ClampMin=0, Units="cm"))
	float MinRange = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Selection", meta=(ClampMin=0, Units="cm"))
	float MaxRange = 600.f;

	/** 몬스터 정면과 타깃 사이 |yaw|의 최댓값. 180이면 모든 방향을 허용한다(꼬리 휩쓸기, 회전 공격). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Selection", meta=(ClampMin=0, ClampMax=180, Units="deg"))
	float MaxAngle = 60.f;

	/** 몬스터 정면과 타깃 사이 |yaw|의 최솟값. 타깃이 옆이나 뒤에 있을 때만 쓰는 기술(꼬리 휩쓸기)에 쓴다. 0이면 정면도 허용. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Selection", meta=(ClampMin=0, ClampMax=180, Units="deg"))
	float MinAngle = 0.f;

	/**
	 * Any가 아니면 타깃이 이쪽에 있을 때만 고를 수 있다(MinAngle·MaxAngle은 그쪽 각도에 적용). 몸을 돌리면 의미가 없는 기술이라
	 * 각도를 무시하는 선택(bIgnoreAngle)에서도 각도와 방향을 검사한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Selection")
	EknightTargetSide TargetSide = EknightTargetSide::Any;

	/** 지금 사용 가능한 공격들 사이의 상대 확률. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Selection", meta=(ClampMin=0))
	float Weight = 1.f;

	/** 발동 후 이 공격을 다시 고를 수 있을 때까지의 시간(초). 기술 패턴이 단조롭지 않게 한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Selection", meta=(ClampMin=0, Units="s"))
	float Cooldown = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Selection")
	EknightTargetAirborneRule AirborneRule = EknightTargetAirborneRule::Any;

	/**
	 * 타깃이 등 뒤(타기팅 컴포넌트의 BehindAngle)에 이 시간(초) 이상 머물러야 고를 수 있다. 0이면 보지 않는다.
	 * 등 뒤에 숨어 있는 플레이어를 응징하는 뒤쪽 공격(꼬리 휩쓸기)에 쓴다. 바로 쓰지 않아서 돌아서는 동안 공격할 틈이 남는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Selection", meta=(ClampMin=0, Units="s"))
	float MinTargetBehindTime = 0.f;

	/** 몬스터가 마지막으로 맞은 뒤 이 시간(초)이 지나야 고를 수 있다. 0이면 보지 않는다. 공격하는 동안 맞지 않았을 때만 쓰는 행동(물러나기)에 쓴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Selection", meta=(ClampMin=0, Units="s"))
	float MinTimeSinceHit = 0.f;

	/** 켜면 AI가 직접 고르지 않고 다른 공격의 연계(FollowUpAttacks)로만 나온다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Selection")
	bool bOnlyAsFollowUp = false;

	/** 몬스터에 이 태그가 모두 있어야 한다(분노 전용 기술이면 State.Monster.Enraged). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Selection", meta=(Categories="State"))
	FGameplayTagContainer RequiredOwnerTags;

	/** 몬스터에 이 태그가 하나도 없어야 한다(탈진 상태에서 쓰기엔 무거운 기술이면 State.Monster.Exhausted). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Selection", meta=(Categories="State"))
	FGameplayTagContainer BlockedOwnerTags;
};

/** 공격 선택에 쓰는 현재 상황 정보. Make로 만든다. */
struct KNIGHT_API FknightMonsterAttackContext
{
	const UAbilitySystemComponent* MonsterASC = nullptr;
	bool bHasTarget = false;
	float Distance = 0.f;
	/** 타깃까지의 부호 있는 yaw(도). */
	float AngleToTarget = 0.f;
	/** 각도 조건(MinAngle, MaxAngle)을 검사하지 않는다. 발동 전에 몸을 돌리는 흐름에서 쓴다. */
	bool bIgnoreAngle = false;
	bool bTargetAirborne = false;
	/** 타깃이 등 뒤에 계속 머문 시간(초). */
	float TargetBehindTime = 0.f;
	/** 몬스터가 마지막으로 맞은 뒤 지난 시간(초). 맞은 적이 없으면 아주 큰 값이다. */
	float TimeSinceHit = UE_BIG_NUMBER;
	double WorldTime = 0.0;

	static FknightMonsterAttackContext Make(const AActor* Monster, const AActor* Target);
};

/**
 * StateTree의 Knight Activate Ability Task가 고르는 몬스터 공격의 베이스 클래스. 하위 클래스 하나가 기술 하나이고,
 * Selection이 사용 가능 조건과 뽑힐 확률을 정하므로 기술 구성은 어빌리티별로 조정한다.
 *
 * 어빌리티는 액터마다 인스턴스가 만들어지므로 쿨다운은 인스턴스에서 관리한다.
 */
UCLASS(Abstract)
class KNIGHT_API UknightGameplayAbility_MonsterAttack : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_MonsterAttack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 현재 상황에서의 선택 가중치. 이 공격을 고르면 안 될 때는 0이다. */
	virtual float GetSelectionWeight(const FknightMonsterAttackContext& Context) const;

	/** Selection.Cooldown이 끝나 이 공격을 다시 쓸 수 있을 때까지 남은 시간(초). 준비되었으면 0이다. */
	float GetCooldownRemaining(double WorldTime) const;

	/** 마지막으로 발동한 시각(월드 초). 한 번도 쓰지 않았으면 아주 작은 값이다. */
	double GetLastActivationTime() const { return LastActivationTime; }

	const FknightMonsterAttackSelection& GetSelection() const { return Selection; }

	/** 공격 판정이 없는 이동 행동(옆걸음, 물러나기)이면 false. 공중 턴의 공격 횟수에 세지 않는다. */
	virtual bool CountsAsAttack() const { return true; }

	/**
	 * 이 공격이 끝난 뒤 쉬지 않고 바로 이어 쓸 공격을 FollowUpAttacks에서 고른다. Selection 조건(사거리·각도·태그)은 보되
	 * 쿨다운은 보지 않는다. 연계가 없으면 AfterAttackMoves에서 고른다. 이어 쓸 게 없으면 무효 핸들.
	 */
	FGameplayAbilitySpecHandle PickFollowUp(UAbilitySystemComponent& ASC, const FknightMonsterAttackContext& Context) const;
	bool HasFollowUps() const { return !FollowUpAttacks.IsEmpty() || !AfterAttackMoves.IsEmpty(); }

	/**
	 * 탈진(State.Monster.Exhausted) 중에는 어떤 공격도 시작하지 않는다. 이미 시작한 공격은 끝까지 한다.
	 * ActivationBlockedTags는 BP와 몬스터 생성기가 부위 파괴 태그로 통째로 덮어쓰므로 여기서 검사한다.
	 */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category="Selection")
	FknightMonsterAttackSelection Selection;

	/**
	 * 발동하면 타깃 방향에서 이 각도(도)만큼 틀어진 쪽으로 몸을 돌린다. 양수는 오른쪽, 음수는 왼쪽, 0이면 돌지 않는다.
	 * 정면이 아닌 쪽을 치는 공격이 타깃을 판정 안에 두도록 쓴다(오른쪽 앞을 덮는 공격이면 -45: 왼쪽으로 몸을 틀어
	 * 타깃이 오른쪽 앞에 오게 한다). 회전은 몸을 다 돌리거나 StartTurnMaxTime이 지나면 멈춘다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Turn", meta=(ClampMin=-180, ClampMax=180, Units="deg"))
	float StartTurnYaw = 0.f;

	/**
	 * 켜면 StartTurnYaw가 0이어도 발동할 때 타깃 쪽으로 돈다. AI가 공격 전에 몸을 돌려 주지 않는 연계(FollowUpAttacks)로
	 * 나오는 공격이 옆을 보고 발동하지 않게 한다. 이미 타깃을 보고 있으면 바로 끝난다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Turn")
	bool bStartTurnToTarget = false;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Turn", meta=(ClampMin=1, EditCondition="StartTurnYaw != 0 || bStartTurnToTarget"))
	float StartTurnRate = 180.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Turn", meta=(ClampMin=0, Units="s", EditCondition="StartTurnYaw != 0 || bStartTurnToTarget"))
	float StartTurnMaxTime = 0.5f;

	/**
	 * 이 공격이 끝나면 쉬는 시간 없이 바로 이어 쓸 공격 후보(연계). 지금 쓸 수 있는 것 중 Selection.Weight로 하나를 고른다.
	 * 자기 자신을 넣으면 한 번 더 쓴다(연타). 이어서 쓴 공격의 연계는 다시 잇지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|FollowUp")
	TArray<TSubclassOf<UknightGameplayAbility_MonsterAttack>> FollowUpAttacks;

	/** 몬스터에 이 태그가 모두 있을 때만 잇는다(잔향은 공명해방 State.Monster.Enraged). 비우면 항상. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|FollowUp")
	FGameplayTagContainer FollowUpRequiredTags;

	UPROPERTY(EditDefaultsOnly, Category="Attack|FollowUp", meta=(ClampMin=0, ClampMax=1))
	float FollowUpChance = 1.f;

	/**
	 * 연계가 나오지 않았을 때 이 공격 뒤에 바로 쓸 이동 행동 후보(물러나기 등). FollowUpRequiredTags·FollowUpChance를 보지 않고
	 * 각자의 Selection 조건(예: MinTimeSinceHit)만 본다. 연계 전용으로 두려면 그쪽 Selection.bOnlyAsFollowUp을 켠다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|FollowUp")
	TArray<TSubclassOf<UknightGameplayAbility_MonsterAttack>> AfterAttackMoves;

private:
	float GetSelectionWeightInternal(const FknightMonsterAttackContext& Context, bool bCheckCooldown) const;

	/** Classes 중 지금 쓸 수 있는 것(Selection 조건, 쿨다운 제외)을 가중치로 하나 고른다. */
	FGameplayAbilitySpecHandle PickWeighted(UAbilitySystemComponent& ASC, const FknightMonsterAttackContext& Context,
		const TArray<TSubclassOf<UknightGameplayAbility_MonsterAttack>>& Classes) const;

	UFUNCTION()
	void HandleStartTurnTick(int32 ActionNumber);

	TWeakObjectPtr<UAbilityTask_Repeat> StartTurnTask;
	double LastActivationTime = -1.e9;
};
