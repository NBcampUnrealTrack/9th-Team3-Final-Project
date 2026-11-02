#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NativeGameplayTags.h"
#include "Combat/knightCombatTypes.h"
#include "Monster/knightMonsterKnockback.h"
#include "knightMonsterGuardPushComponent.generated.h"

namespace knightMonsterGuardPush
{
	/** 몬스터 공격을 막아 밀려났다. EventMagnitude 1 = 약하게 밀림, 2 = 크게 밀림. Instigator는 몬스터. */
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_GuardPushed);
	/** 막았지만 가드가 깨지는 공격이었다. Instigator는 몬스터. */
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_GuardBroken);
}

/** 몬스터 공격을 가드로 막았을 때의 반응 단계. 공격 무게 단계(히트스톱)와 같은 순서다. */
UENUM(BlueprintType)
enum class EknightMonsterGuardReaction : uint8
{
	/** 지속 피해(브레스·장판·토네이도). 타격마다 조금씩 밀린다. */
	Sustained,
	Light,
	Medium,
	Heavy,
	/** 막아도 가드가 깨진다. */
	Break
};

/**
 * 몬스터 공격을 가드로 막은 대상을 공격 무게(EknightMonsterGuardReaction)만큼 몬스터 반대쪽으로 밀어내고,
 * 대상에게 Event.GuardPushed(약하게 1 / 크게 2) 또는 Event.GuardBroken을 보낸다. 밀림 애니메이션과 가드 브레이크 처리는 받는 쪽이 정한다.
 *
 * 밀림은 대상의 시간으로 센다(대상 CustomTimeDilation). 막은 순간의 히트스톱 동안은 멈춰 있다가 풀리면 밀려난다.
 * 공격이 따로 넉백을 주면(돌진 등) 둘 중 더 먼 쪽 하나만 쓴다(HandleHit 반환값).
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightMonsterGuardPushComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UknightMonsterGuardPushComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Monster의 공격 결과를 넘긴다. 가드로 막힌 타격이면 이벤트를 보내고, ExistingPushDistance(공격 자체의 가드 넉백 거리)보다
	 * 단계 거리가 길면 밀어낸다. 밀어냈으면 true: 부른 쪽은 자기 넉백을 걸지 않는다.
	 */
	static bool HandleHit(const AActor* Monster, AActor& Target, EknightHitOutcome Outcome, EknightMonsterGuardReaction Reaction,
		float ExistingPushDistance = 0.f);

	float GetPushDistance(EknightMonsterGuardReaction Reaction) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Push", meta=(ClampMin=0, Units="Centimeters"))
	float SustainedDistance = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Push", meta=(ClampMin=0, Units="Centimeters"))
	float LightDistance = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Push", meta=(ClampMin=0, Units="Centimeters"))
	float MediumDistance = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Push", meta=(ClampMin=0, Units="Centimeters"))
	float HeavyDistance = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Push", meta=(ClampMin=0, Units="Centimeters"))
	float BreakDistance = 250.f;

	/** 밀려나는 시간(대상 시간). 거리 / 시간이 밀리는 속도다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Push", meta=(ClampMin=0.02, Units="s"))
	float PushTime = 0.2f;

private:
	bool OnGuarded(AActor& Target, EknightMonsterGuardReaction Reaction, float ExistingPushDistance);

	struct FPush
	{
		TWeakObjectPtr<AActor> Target;
		FknightMonsterKnockback Knockback;
		float Remaining = 0.f;
	};
	TArray<FPush> Pushes;
};
