#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "knightMonsterAirTurnComponent.generated.h"

class UAbilitySystemComponent;
class UAudioComponent;
class UGameplayAbility;
class UknightMonsterHoverComponent;
class USoundBase;
struct FAbilityEndedData;

/**
 * 지상 턴과 공중 턴을 번갈아 돌리는 날짐승 몬스터(잔향)의 턴 관리.
 *
 *   지상 턴: 타깃을 잡은 뒤 지상 공격을 GroundAttacksPerTurn번 쓰거나 GroundTurnTime이 지나면, 공격이 끝난 뒤 이륙(Event.Monster.TakeOff)
 *   공중 턴: State.Monster.Airborne이 붙는다. 공중 공격을 AirAttacksPerTurn번 쓰거나 AirTurnMaxTime이 지나면,
 *            공격이 끝난 뒤 착지(Event.Monster.Land)
 *            다음 공격이 이 턴의 마지막이면 State.Monster.LastAirAttack을 붙인다. 공중 턴을 스스로 끝내는 공격(잔향 회오리)은
 *            이 태그를 필요 태그로 가져 마지막 순서에만 나온다.
 *   격추: 공중 턴에 다운(State.HitReact.Knockdown)되면 곧바로 떨어지고 지상 턴이 처음부터 시작된다.
 *   거리 벌리기: 공중 턴에 공격·반응 중이 아니고 타깃이 AirKeepDistance보다 가까우면 타깃을 본 채 뒤로 물러난다(다이브 거리 확보).
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
	UFUNCTION(BlueprintCallable, Category="Knight|Monster")
	void TakeOff();

	UFUNCTION(BlueprintCallable, Category="Knight|Monster")
	void Land();

	/** 공격이 스스로 땅에 내려앉을 때(탈진 착지): 착지 애니메이션 없이 공중 턴을 끝내고 BlendTime 동안 지상 높이로 내린다. */
	void EndAirTurnNow(float BlendTime) { if (bAirborne) { EndAirTurn(BlendTime, false); } }

	/** 지상 턴 시간을 처음부터 센다(탈진 착지 같은 딜 타임이 끝난 뒤). */
	void RestartGroundTurn() { GroundTime = 0.f; GroundAttacksUsed = 0; }

protected:
	/**
	 * 타깃을 잡은 뒤 지상 턴이 이어지는 시간. 반응(다운·포효) 중에는 흐르지 않는다. 0이면 공중 턴을 쓰지 않는다.
	 * GroundAttacksPerTurn을 쓰면 공격을 다 못 썼을 때(타깃이 멀리 도망감)의 최대 시간이 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn", meta=(ClampMin=0, Units="s"))
	float GroundTurnTime = 0.f;

	/** 지상 턴 한 번에 쓰는 지상 공격 수. 쉬는 시간이 길어도 지상 턴의 공격 수가 같게 유지된다. 0이면 GroundTurnTime만 본다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn", meta=(ClampMin=0, EditCondition="GroundTurnTime > 0"))
	int32 GroundAttacksPerTurn = 0;

	/** 공중 턴 한 번에 쓰는 공중 공격 수. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn", meta=(ClampMin=1, EditCondition="GroundTurnTime > 0"))
	int32 AirAttacksPerTurn = 2;

	/** 공중 공격을 다 쓰지 못해도 이 시간이 지나면 착지한다(쓸 수 있는 공중 공격이 없을 때). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn", meta=(ClampMin=0, Units="s", EditCondition="GroundTurnTime > 0"))
	float AirTurnMaxTime = 20.f;

	/** State.Monster.Enraged 동안 GroundTurnTime 대신 쓴다(공명해방은 공중 위주로). 0이면 평소 값. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn|Enraged", meta=(ClampMin=0, Units="s", EditCondition="GroundTurnTime > 0"))
	float EnragedGroundTurnTime = 0.f;

	/** State.Monster.Enraged 동안 GroundAttacksPerTurn 대신 쓴다. 0이면 평소 값. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn|Enraged", meta=(ClampMin=0, EditCondition="GroundTurnTime > 0"))
	int32 EnragedGroundAttacksPerTurn = 0;

	/** State.Monster.Enraged 동안 AirAttacksPerTurn 대신 쓴다. 0이면 평소 값. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn|Enraged", meta=(ClampMin=0, EditCondition="GroundTurnTime > 0"))
	int32 EnragedAirAttacksPerTurn = 0;

	/** State.Monster.Enraged 동안 AirTurnMaxTime 대신 쓴다. 0이면 평소 값. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn|Enraged", meta=(ClampMin=0, Units="s", EditCondition="GroundTurnTime > 0"))
	float EnragedAirTurnMaxTime = 0.f;

	/** 메시가 공중 높이로 오르는 시간. 이륙 애니메이션 길이에 맞춘다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn", meta=(ClampMin=0, Units="s", EditCondition="GroundTurnTime > 0"))
	float TakeOffBlendTime = 1.f;

	/** 메시가 지상 높이로 내려오는 시간. 착지 애니메이션 길이에 맞춘다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn", meta=(ClampMin=0, Units="s", EditCondition="GroundTurnTime > 0"))
	float LandBlendTime = 1.5f;

	/** 공중 턴에 타깃과 이 거리(수평)보다 가까우면 뒤로 물러난다. 0이면 물러나지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn", meta=(ClampMin=0, Units="cm", EditCondition="GroundTurnTime > 0"))
	float AirKeepDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn", meta=(ClampMin=1, Units="cm/s", EditCondition="AirKeepDistance > 0"))
	float RetreatSpeed = 1200.f;

	/** 격추되거나 공중에서 죽었을 때 떨어지는 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn", meta=(ClampMin=0, Units="s", EditCondition="GroundTurnTime > 0"))
	float FallBlendTime = 0.4f;

	/**
	 * 공중 턴에 제자리에서 떠 있는 동안(공격·반응 중이 아닐 때) HoverSoundInterval마다 겹쳐 재생하는 날갯짓 소리.
	 * 끝이 잦아드는 한 번짜리 소리도 겹쳐 이어지도록 반복(Looping) 대신 다시 재생한다. 비우면 소리 없음.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn|Sound", meta=(EditCondition="GroundTurnTime > 0"))
	TObjectPtr<USoundBase> HoverSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn|Sound", meta=(ClampMin=0.1, Units="s", EditCondition="GroundTurnTime > 0"))
	float HoverSoundInterval = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn|Sound", meta=(EditCondition="GroundTurnTime > 0"))
	FName HoverSoundBone = TEXT("spine_02");

	/** 떠 있기가 끝나면(공격·착지·격추) 울리던 날갯짓을 이 시간 동안 줄인다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Turn|Sound", meta=(ClampMin=0, Units="s", EditCondition="GroundTurnTime > 0"))
	float HoverSoundFadeOut = 0.4f;

private:
	void HandleAbilityEnded(const FAbilityEndedData& Data);
	void HandleKnockdownChanged(const FGameplayTag Tag, int32 NewCount);
	/** 공중 턴을 끝낸다. bSendLandEvent가 false면 착지 애니메이션 없이 BlendTime 동안 떨어진다. */
	void EndAirTurn(float BlendTime, bool bSendLandEvent);
	void SetPawnBlocking(bool bBlock);
	/** 공중 턴이고 남은 공중 공격이 하나면 State.Monster.LastAirAttack을 붙이고, 아니면 뗀다. */
	void UpdateLastAttackTag();
	bool IsAttacking() const;
	bool IsEnraged() const;
	float GetGroundTurnTime() const;
	int32 GetGroundAttacksPerTurn() const;
	int32 GetAirAttacksPerTurn() const;
	float GetAirTurnMaxTime() const;
	bool CanChangeTurn() const;
	void SendEvent(const FGameplayTag& EventTag) const;
	/** 타깃이 AirKeepDistance보다 가까우면 뒤로 물러난다. */
	void UpdateRetreat(float DeltaTime);
	/** bHovering이면 HoverSoundInterval마다 날갯짓 소리를 내고, 아니면 울리던 소리를 줄인다. */
	void UpdateHoverSound(bool bHovering, float DeltaTime);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> HoverAudios;
	float HoverSoundTime = 0.f;

	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	TWeakObjectPtr<UknightMonsterHoverComponent> Hover;
	FDelegateHandle AbilityEndedHandle;
	FDelegateHandle KnockdownHandle;
	bool bAirborne = false;
	double LastAttackEndTime = -1.e9;
	static constexpr float FollowUpGraceTime = 0.2f;
	float GroundTime = 0.f;
	float AirTime = 0.f;
	int32 AirAttacksUsed = 0;
	int32 GroundAttacksUsed = 0;
	double RetreatEndTime = 0.0;
	TEnumAsByte<ECollisionResponse> SavedPawnResponse = ECR_Block;
};
