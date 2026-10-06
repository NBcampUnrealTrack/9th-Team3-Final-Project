#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_MonsterAttack.generated.h"

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

	/** 지금 사용 가능한 공격들 사이의 상대 확률. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Selection", meta=(ClampMin=0))
	float Weight = 1.f;

	/** 발동 후 이 공격을 다시 고를 수 있을 때까지의 시간(초). 기술 패턴이 단조롭지 않게 한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Selection", meta=(ClampMin=0, Units="s"))
	float Cooldown = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Selection")
	EknightTargetAirborneRule AirborneRule = EknightTargetAirborneRule::Any;

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
	bool bTargetAirborne = false;
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

	const FknightMonsterAttackSelection& GetSelection() const { return Selection; }

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category="Selection")
	FknightMonsterAttackSelection Selection;

private:
	double LastActivationTime = -1.e9;
};
