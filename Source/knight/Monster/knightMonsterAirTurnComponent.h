#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "knightMonsterAirTurnComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UknightMonsterHoverComponent;
struct FAbilityEndedData;

/**
 * 지상 턴과 공중 턴을 번갈아 돌리는 날짐승 몬스터(잔향)의 턴 관리.
 *
 *   지상 턴: 타깃을 잡은 뒤 GroundTurnTime 동안 지상 공격 → 공격이 끝나면 이륙(Event.Monster.TakeOff)
 *   공중 턴: State.Monster.Airborne이 붙는다. 공중 공격을 AirAttacksPerTurn번 쓰거나 AirTurnMaxTime이 지나면,
 *            공격이 끝난 뒤 착지(Event.Monster.Land)
 *   격추: 공중 턴에 다운(State.HitReact.Knockdown)되면 곧바로 떨어지고 지상 턴이 처음부터 시작된다.
 *
 * 공중에서도 캡슐과 이동은 땅에 그대로 두고 메시만 띄운다(UknightMonsterHoverComponent::SetAirborne). 길찾기, 추격,
 * StateTree를 지상 턴과 똑같이 쓰기 위해서다. 그 대신 공중 턴 동안 캡슐이 플레이어(Pawn)를 막지 않게 한다.
 * 공격 구분은 태그로 한다. 지상 공격은 Airborne을 차단 태그로, 공중 공격은 필요 태그로 갖는다(레시피 Build가 넣는다).
 * 이륙·착지 애니메이션은 이벤트로 발동하는 반응 어빌리티라 그동안 StateTree가 기다린다.
 *
 * GroundTurnTime이 0이면 꺼진다(대부분의 몬스터).
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightMonsterAirTurnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UknightMonsterAirTurnComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category="Knight|Monster")
	bool IsAirborne() const { return bAirborne; }

	/** 지금 턴 조건과 상관없이 이륙하거나 착지한다(테스트·연출용). */
	void TakeOff();
	void Land();

protected:
	/** 타깃을 잡은 뒤 지상 턴이 이어지는 시간. 반응(다운·포효) 중에는 흐르지 않는다. 0이면 공중 턴을 쓰지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn", meta=(ClampMin=0, Units="s"))
	float GroundTurnTime = 0.f;

	/** 공중 턴 한 번에 쓰는 공중 공격 수. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn", meta=(ClampMin=1, EditCondition="GroundTurnTime > 0"))
	int32 AirAttacksPerTurn = 2;

	/** 공중 공격을 다 쓰지 못해도 이 시간이 지나면 착지한다(쓸 수 있는 공중 공격이 없을 때). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn", meta=(ClampMin=0, Units="s", EditCondition="GroundTurnTime > 0"))
	float AirTurnMaxTime = 20.f;

	/** 메시가 공중 높이로 오르는 시간. 이륙 애니메이션 길이에 맞춘다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn", meta=(ClampMin=0, Units="s", EditCondition="GroundTurnTime > 0"))
	float TakeOffBlendTime = 1.f;

	/** 메시가 지상 높이로 내려오는 시간. 착지 애니메이션 길이에 맞춘다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn", meta=(ClampMin=0, Units="s", EditCondition="GroundTurnTime > 0"))
	float LandBlendTime = 1.5f;

	/** 격추되거나 공중에서 죽었을 때 떨어지는 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn", meta=(ClampMin=0, Units="s", EditCondition="GroundTurnTime > 0"))
	float FallBlendTime = 0.4f;

private:
	void HandleAbilityEnded(const FAbilityEndedData& Data);
	void HandleKnockdownChanged(const FGameplayTag Tag, int32 NewCount);
	/** 공중 턴을 끝낸다. bSendLandEvent가 false면 착지 애니메이션 없이 BlendTime 동안 떨어진다. */
	void EndAirTurn(float BlendTime, bool bSendLandEvent);
	void SetPawnBlocking(bool bBlock);
	bool IsAttacking() const;
	bool CanChangeTurn() const;
	void SendEvent(const FGameplayTag& EventTag) const;

	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	TWeakObjectPtr<UknightMonsterHoverComponent> Hover;
	FDelegateHandle AbilityEndedHandle;
	FDelegateHandle KnockdownHandle;
	bool bAirborne = false;
	float GroundTime = 0.f;
	float AirTime = 0.f;
	int32 AirAttacksUsed = 0;
	TEnumAsByte<ECollisionResponse> SavedPawnResponse = ECR_Block;
};
